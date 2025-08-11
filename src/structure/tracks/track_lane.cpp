// SPDX-FileCopyrightText: © 2019-2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "dsp/midi_event.h"
#include "gui/backend/backend/actions/tracklist_selections_action.h"
#include "gui/backend/backend/project.h"
#include "gui/backend/backend/zrythm.h"
#include "structure/arrangement/midi_region.h"
#include "structure/tracks/laned_track.h"
#include "structure/tracks/track_lane.h"
#include "structure/tracks/tracklist.h"
#include "utils/rt_thread_id.h"

#include "midilib/src/midifile.h"
#include "midilib/src/midiinfo.h"
#include <fmt/printf.h>

namespace zrythm::structure::tracks
{

void
TrackLane::generate_name (int pos)
{
  name_ = utils::Utf8String::from_qstring (
    format_qstr (QObject::tr ("Lane {}"), pos + 1));
}

template <arrangement::RegionObject RegionT>
void
TrackLaneImpl<RegionT>::rename_with_action (const utils::Utf8String &new_name)
{
  rename (new_name, true);
}

template <arrangement::RegionObject RegionT>
void
TrackLaneImpl<RegionT>::unselect_all ()
{
  for (auto * region : this->get_children_view ())
    {
      auto selection_mgr =
        arrangement::ArrangerObjectFactory::get_instance ()
          ->get_selection_manager_for_object (*region);
      selection_mgr.remove_from_selection (region->get_uuid ());
    }
}

template <arrangement::RegionObject RegionT>
void
TrackLaneImpl<RegionT>::rename (
  const utils::Utf8String &new_name,
  bool                     with_action)
{
  if (with_action)
    {
      try
        {
          UNDO_MANAGER->perform (
            new gui::actions::RenameTrackLaneAction (*this, new_name));
        }
      catch (const ZrythmException &e)
        {
          e.handle (QObject::tr ("Failed to rename lane"));
          return;
        }
      // EVENTS_PUSH (EventType::ET_TRACK_LANES_VISIBILITY_CHANGED, nullptr);
    }
  else
    {
      name_ = new_name;
    }
}

template <arrangement::RegionObject RegionT>
void
TrackLaneImpl<
  RegionT>::set_soloed (bool solo, bool trigger_undo, bool fire_events)
{
  if (trigger_undo)
    {
      try
        {
          UNDO_MANAGER->perform (
            new gui::actions::SoloTrackLaneAction (*this, solo));
        }
      catch (const ZrythmException &e)
        {
          e.handle (QObject::tr ("Cannot set track lane soloed"));
          return;
        }
    }
  else
    {
      z_debug ("setting lane '{}' soloed to {}", name_, solo);
      solo_ = solo;
    }

  if (fire_events)
    {
      /* TODO use more specific event */
      // EVENTS_PUSH (EventType::ET_TRACK_LANES_VISIBILITY_CHANGED, nullptr);
    }
}

template <arrangement::RegionObject RegionT>
void
TrackLaneImpl<RegionT>::set_muted (bool mute, bool trigger_undo, bool fire_events)
{
  if (trigger_undo)
    {
      try
        {
          UNDO_MANAGER->perform (
            new gui::actions::MuteTrackLaneAction (*this, mute));
        }
      catch (const ZrythmException &e)
        {
          e.handle (QObject::tr ("Could not mute/unmute track lane"));
          return;
        }
    }
  else
    {
      z_debug ("setting lane '{}' muted to {}", name_, mute);
      mute_ = mute;
    }

  if (fire_events)
    {
      /* TODO use more specific event */
      // EVENTS_PUSH (EventType::ET_TRACK_LANES_VISIBILITY_CHANGED, nullptr);
    }
}

template <arrangement::RegionObject RegionT>
void
TrackLaneImpl<RegionT>::after_remove_region ()
{
// TODO
#if 0
  auto track = get_track ();
  z_return_if_fail (track);
  if (
    !arrangement::ArrangerObjectOwner<RegionT>::clearing_
    && !track->block_auto_creation_and_deletion_)
    {
      track->remove_empty_last_lanes ();
    }
#endif
}

template <arrangement::RegionObject RegionT>
bool
TrackLaneImpl<RegionT>::is_effectively_muted () const
{
  if (get_muted ())
    return true;

  /* if lane is non-soloed while other soloed lanes exist, this should
   * be muted */
  auto track = get_track ();
  z_return_val_if_fail (track, true);
  if (track->has_soloed_lanes () && !get_soloed ())
    return true;

  return false;
}

/**
 * Calculates a unique index for this lane.
 */
template <arrangement::RegionObject RegionT>
int
TrackLaneImpl<RegionT>::calculate_lane_idx_for_midi_serialization () const
{
  auto        track = get_track ();
  Tracklist * tracklist = TRACKLIST;
  int         pos = 1;
  for (
    const auto * cur_track :
    tracklist->get_track_span ().get_elements_derived_from<LanedTrackT> ())
    {
      if (cur_track == track)
        {
          pos +=
            track->get_lane_index (*dynamic_cast<const TrackLaneT *> (this));
          break;
        }

      pos += static_cast<int> (cur_track->lanes_.size ());
    }

  return pos;
}

template <arrangement::RegionObject RegionT>
void
TrackLaneImpl<RegionT>::write_to_midi_file (
  MIDI_FILE *            mf,
  dsp::MidiEventVector * events,
  const Position *       start,
  const Position *       end,
  bool                   lanes_as_tracks,
  bool                   use_track_or_lane_pos)
  requires std::derived_from<MidiRegion, RegionT>
{
// TODO
#if 0
  auto track = get_track ();
  z_return_if_fail (track);
  auto                                  midi_track_pos = track->get_index ();
  std::unique_ptr<dsp::MidiEventVector> own_events;
  if (lanes_as_tracks)
    {
      z_return_if_fail (!events);
      midi_track_pos = calculate_lane_idx_for_midi_serialization ();
      own_events = std::make_unique<dsp::MidiEventVector> ();
    }
  else if (!use_track_or_lane_pos)
    {
      z_return_if_fail (events);
      midi_track_pos = 1;
    }
  /* else if using track positions */
  else
    {
      z_return_if_fail (events);
    }

  /* All data is written out to tracks not channels. We therefore set the
   * current channel before writing data out. Channel assignments can change any
   * number of times during the file, and affect all tracks messages until it is
   * changed. */
  midiFileSetTracksDefaultChannel (mf, midi_track_pos, MIDI_CHANNEL_1);

  /* add track name */
  if (lanes_as_tracks && use_track_or_lane_pos)
    {
      const auto midi_track_name =
        fmt::format ("{} - {}", track->get_name (), get_name ());
      midiTrackAddText (
        mf, midi_track_pos, textTrackName, midi_track_name.c_str ());
    }

  for (auto * region : this->get_children_view ())
    {
      /* skip regions not inside the given range */
      if (start)
        {
          if (region->get_end_position () < *start)
            continue;
        }
      if (end)
        {
          if (region->get_position () > *end)
            continue;
        }

      region->add_events (
        own_events ? *own_events : *events, start, end, true, true);
    }

  if (own_events)
    {
      own_events->write_to_midi_file (mf, midi_track_pos);
    }
#endif
}

template <arrangement::RegionObject RegionT>
std::unique_ptr<typename TrackLaneImpl<RegionT>::TrackLaneT>
TrackLaneImpl<RegionT>::gen_snapshot () const
{
  // TODO
  throw std::runtime_error ("not implemented");
#if 0
  auto ret = utils::clone_unique (
    get_derived_lane (), utils::ObjectCloneType::Snapshot,
    track_);
  return ret;

  ret->track_ = track_;

  /* clone_unique above creates the regions in `regions_` but we want them in
   * `region_snapshots_`... */
  for (auto &region_var : this->region_list_->regions_)
    {
      auto region = std::get<RegionT *> (region_var);
      ret->region_snapshots_.emplace_back (region->clone_unique ());
    }
  ret->region_list_->clear ();
  return ret;
#endif
}

template class TrackLaneImpl<arrangement::MidiRegion>;
template class TrackLaneImpl<arrangement::AudioRegion>;
}
