// SPDX-FileCopyrightText: © 2018-2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include <utility>

#include "gui/backend/backend/project.h"
#include "gui/backend/backend/zrythm.h"
#include "structure/arrangement/automation_point.h"
#include "structure/arrangement/automation_region.h"
#include "structure/tracks/automatable_track.h"
#include "structure/tracks/automation_track.h"
#include "structure/tracks/track.h"
#include "structure/tracks/tracklist.h"
#include "utils/gtest_wrapper.h"
#include "utils/math.h"
#include "utils/rt_thread_id.h"

namespace zrythm::structure::tracks
{
AutomationTrack::AutomationTrack (
  dsp::FileAudioSourceRegistry        &file_audio_source_registry,
  ArrangerObjectRegistry              &obj_registry,
  TrackGetter                          track_getter,
  dsp::ProcessorParameterUuidReference param_id)
    : arrangement::ArrangerObjectOwner<arrangement::AutomationRegion> (
        obj_registry,
        file_audio_source_registry,
        *this),
      object_registry_ (obj_registry), track_getter_ (std::move (track_getter)),
      port_id_ (std::move (param_id))
{
  parameter ()->set_automation_provider (
    [this] (unsigned_frame_t sample_position) -> std::optional<float> {
      if (get_children_vector ().empty ())
        {
          return std::nullopt;
        }
      return get_normalized_val_at_pos (
        static_cast<signed_frame_t> (sample_position), false, false);
    });
#if 0
  auto * port =
    std::get<ControlPort *> (port_registry.find_by_id_or_throw (port_id));
  if (ENUM_BITSET_TEST(PortIdentifier::Flags,port->id_.flags,PortIdentifier::Flags::MIDI_AUTOMATABLE))
    {
      self->automation_mode =
        AutomationMode::AUTOMATION_MODE_RECORD;
      self->record_mode =
        AutomationRecordMode::AUTOMATION_RECORD_MODE_TOUCH;
    }
#endif
}

void
AutomationTrack::init_loaded ()
{
}

// ========================================================================
// QML Interface
// ========================================================================

void
AutomationTrack::setHeight (double height)
{
  if (utils::math::floats_equal (height, height_))
    return;

  height_ = height;
  Q_EMIT heightChanged (height);
}

void
AutomationTrack::setAutomationMode (int automation_mode)
{
  if (automation_mode == ENUM_VALUE_TO_INT (automation_mode_))
    return;

  automation_mode_ =
    ENUM_INT_TO_VALUE (decltype (automation_mode_), automation_mode);
  Q_EMIT automationModeChanged (automation_mode);
}

void
AutomationTrack::setRecordMode (int record_mode)
{
  if (record_mode == ENUM_VALUE_TO_INT (record_mode_))
    return;

  record_mode_ = ENUM_INT_TO_VALUE (decltype (record_mode_), record_mode);
  Q_EMIT recordModeChanged (record_mode);
}

// ========================================================================

AutomationTracklist *
AutomationTrack::get_automation_tracklist () const
{
  return std::visit (
    [&] (auto &&track) -> AutomationTracklist * {
      if constexpr (
        std::derived_from<base_type<decltype (track)>, AutomatableTrack>)
        {
          return &track->get_automation_tracklist ();
        }
      else
        {
          throw std::runtime_error ("not an automatable track");
        }
    },
    track_getter_ ());
}

structure::arrangement::AutomationPoint *
AutomationTrack::get_ap_around (
  const double position_ticks,
  double       delta_ticks,
  bool         before_only,
  bool         use_snapshots)
{
  const auto &tempo_map = PROJECT->get_tempo_map ();
  auto        pos_frames = tempo_map.tick_to_samples_rounded (position_ticks);
  AutomationPoint * ap = get_ap_before_pos (pos_frames, true, use_snapshots);
  if (
    (ap != nullptr) && position_ticks - ap->position ()->ticks () <= delta_ticks)
    {
      return ap;
    }
  else if (!before_only)
    {
      pos_frames = tempo_map.tick_to_samples_rounded (pos_frames + delta_ticks);
      ap = get_ap_before_pos (pos_frames, true, use_snapshots);
      if (ap != nullptr)
        {
          double diff = ap->position ()->ticks () - position_ticks;
          if (diff >= 0.0)
            return ap;
        }
    }

  return nullptr;
}

auto
AutomationTrack::get_region_before_pos (
  signed_frame_t pos,
  bool           ends_after,
  bool           use_snapshots) const -> AutomationRegion *
{
  auto process_regions = [=] (const auto &regions) {
    if (ends_after)
      {
        for (const auto &region : std::views::reverse (regions))
          {
            if (region->regionMixin ()->bounds ()->is_hit (pos))
              return region;
          }
      }
    else
      {
        AutomationRegion * latest_r = nullptr;
        signed_frame_t     latest_distance =
          std::numeric_limits<signed_frame_t>::min ();
        for (const auto &region : std::views::reverse (regions))
          {
            signed_frame_t distance_from_r_end =
              region->regionMixin ()->bounds ()->get_end_position_samples (true)
              - pos;
            if (
              region->position ()->samples () <= pos
              && distance_from_r_end > latest_distance)
              {
                latest_distance = distance_from_r_end;
                latest_r = region;
              }
          }
        return latest_r;
      }
    return static_cast<AutomationRegion *> (nullptr);
  };

  // TODO
  return process_regions (get_children_view ());
#if 0
  return use_snapshots
           ? process_regions (get_children_snapshots_view ())
           : process_regions (get_children_view ());
#endif
}

auto
AutomationTrack::get_ap_before_pos (
  signed_frame_t pos,
  bool           ends_after,
  bool           use_snapshots) const -> AutomationPoint *
{
  auto * r = get_region_before_pos (pos, ends_after, use_snapshots);

  if ((r == nullptr) || r->regionMixin ()->mute ()->muted ())
    {
      return nullptr;
    }

  const auto region_end_frames =
    r->regionMixin ()->bounds ()->get_end_position_samples (true);

  /* if region ends before pos, assume pos is the region's end pos */
  signed_frame_t local_pos = timeline_frames_to_local (
    *r, !ends_after && (region_end_frames < pos) ? region_end_frames - 1 : pos,
    true);

  for (auto * ap : std::ranges::reverse_view (r->get_children_view ()))
    {
      if (ap->position ()->samples () <= local_pos)
        {
          return ap;
        }
    }

  return nullptr;
}

void
AutomationTrack::set_automation_mode (AutomationMode mode, bool fire_events)
{
  z_return_if_fail (ZRYTHM_IS_QT_THREAD);

  auto atl = get_automation_tracklist ();
  z_return_if_fail (atl);

  /* add to atl cache if recording */
  if (mode == AutomationMode::Record)
    {
      auto ats_in_record_mode = atl->get_automation_tracks_in_record_mode ();
      if (
        std::ranges::find (
          ats_in_record_mode, this, &QPointer<AutomationTrack>::get)
        == ats_in_record_mode.end ())
        {
          ats_in_record_mode.emplace_back (this);
        }
    }

  automation_mode_ = mode;
}

bool
AutomationTrack::should_read_automation () const
{
  if (automation_mode_ == AutomationMode::Off)
    return false;

  /* TODO last argument should be true but doesnt work properly atm */
  if (should_be_recording (false))
    return false;

  return true;
}

bool
AutomationTrack::should_be_recording (bool record_aps) const
{
  if (automation_mode_ != AutomationMode::Record) [[likely]]
    return false;

  if (record_mode_ == AutomationRecordMode::Latch)
    {
      /* in latch mode, we are always recording, even if the value doesn't
       * change (an automation point will be created as soon as latch mode is
       * armed) and then only when changes are made) */
      return true;
    }

  if (record_mode_ == AutomationRecordMode::Touch)
    {
// TODO
#if 0
      const auto &port = get_port ();
      const auto  diff = port.ms_since_last_change ();
      if (
        diff
        < static_cast<RtTimePoint> (AUTOMATION_RECORDING_TOUCH_REL_MS) * 1000)
        {
          /* still recording */
          return true;
        }
#endif

      if (!record_aps)
        return recording_started_;
    }

  return false;
}

TrackPtrVariant
AutomationTrack::get_track () const
{
  return track_getter_ ();
#if 0
  auto port_var = PROJECT->find_port_by_id (port_id_);
  z_return_val_if_fail (
    port_var && std::holds_alternative<ControlPort *> (*port_var), nullptr);
  auto * port = std::get<ControlPort *> (*port_var);
  z_return_val_if_fail (port_id_ == port->get_uuid (), nullptr);
  auto track_id_opt = port->id_->get_track_id ();
  z_return_val_if_fail (track_id_opt.has_value (), nullptr);
  auto track_var = PROJECT->find_track_by_id (track_id_opt.value ());
  z_return_val_if_fail (track_var, nullptr);
  return std::visit (
    [&] (auto &track) -> AutomatableTrack * {
      using TrackT = base_type<decltype (track)>;
      if constexpr (std::derived_from<TrackT, AutomatableTrack>)
        return track;
      z_return_val_if_reached (nullptr);
    },
    *track_var);
#endif
}

void
AutomationTrack::set_index (int index)
{
  index_ = index;

#if 0
  for (auto * region : get_children_view ())
    {
      // region->id_.at_idx_ = index;
      region->update_identifier ();
    }
#endif
}

std::optional<float>
AutomationTrack::get_normalized_val_at_pos (
  signed_frame_t pos,
  bool           ends_after,
  bool           use_snapshots) const
{
  auto ap = get_ap_before_pos (pos, ends_after, use_snapshots);

  /* no automation points yet, return negative (no change) */
  if (ap == nullptr)
    {
      return std::nullopt;
    }

  auto region = get_region_before_pos (pos, ends_after, use_snapshots);
  z_return_val_if_fail (region, 0.f);

  /* if region ends before pos, assume pos is the region's end pos */
  const auto region_end_position =
    region->regionMixin ()->bounds ()->get_end_position_samples (true);
  signed_frame_t localp = timeline_frames_to_local (
    *region,
    !ends_after && (region_end_position < pos) ? region_end_position - 1 : pos,
    true);

  auto next_ap = region->get_next_ap (*ap, false);

  /* return value at last ap */
  if (next_ap == nullptr)
    {
      return ap->value ();
    }

  bool  prev_ap_lower = ap->value () <= next_ap->value ();
  float cur_next_diff = std::abs (ap->value () - next_ap->value ());

  /* ratio of how far in we are in the curve */
  signed_frame_t ap_frames = ap->position ()->samples ();
  signed_frame_t next_ap_frames = next_ap->position ()->samples ();
  double         ratio = 1.0;
  signed_frame_t numerator = localp - ap_frames;
  signed_frame_t denominator = next_ap_frames - ap_frames;
  if (numerator == 0)
    {
      ratio = 0.0;
    }
  else if (denominator == 0) [[unlikely]]
    {
      z_warning ("denominator is 0. this should never happen");
      ratio = 1.0;
    }
  else
    {
      ratio = (double) numerator / (double) denominator;
    }
  z_return_val_if_fail (ratio >= 0, 0.f);

  auto result =
    static_cast<float> (region->get_normalized_value_in_curve (*ap, ratio));
  result = result * cur_next_diff;
  if (prev_ap_lower)
    result += ap->value ();
  else
    result += next_ap->value ();

  return result;
}

void
AutomationTrack::set_caches (CacheType types)
{
  if (ENUM_BITSET_TEST (types, CacheType::PlaybackSnapshots))
    {
// TODO
#if 0
      get_children_snapshots_vector ().clear ();
      for (const auto &r_var : region_list_->regions_)
        {
          region_snapshots_.emplace_back (
            std::get<AutomationRegion *> (r_var)->clone_unique ());
        }
#endif
    }

#if 0
  if (ENUM_BITSET_TEST ( types, CacheType::AutomationLanePorts))
    {
      port_ = get_port ();
    }
#endif
}

void
init_from (
  AutomationTrack       &obj,
  const AutomationTrack &other,
  utils::ObjectCloneType clone_type)
{
  init_from (
    static_cast<AutomationTrack::ArrangerObjectOwner &> (obj),
    static_cast<const AutomationTrack::ArrangerObjectOwner &> (other),
    clone_type);
  obj.visible_ = other.visible_;
  obj.created_ = other.created_;
  obj.index_ = other.index_;
  obj.y_ = other.y_;
  obj.automation_mode_ = other.automation_mode_;
  obj.record_mode_ = other.record_mode_;
  obj.height_ = other.height_;
  assert (obj.height_ >= Track::MIN_HEIGHT);
  obj.port_id_ = other.port_id_;
}
}
