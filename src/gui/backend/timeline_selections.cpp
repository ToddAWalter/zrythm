// SPDX-FileCopyrightText: © 2019-2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "dsp/chord_track.h"
#include "dsp/engine.h"
#include "dsp/marker_track.h"
#include "dsp/midi_event.h"
#include "dsp/position.h"
#include "dsp/tempo_track.h"
#include "dsp/track.h"
#include "dsp/tracklist.h"
#include "dsp/transport.h"
#include "gui/backend/event.h"
#include "gui/backend/event_manager.h"
#include "gui/backend/timeline_selections.h"
#include "project.h"
#include "utils/dsp.h"
#include "utils/math.h"
#include "utils/rt_thread_id.h"
#include "zrythm.h"
#include "zrythm_app.h"

#include "gtk_wrapper.h"

TimelineSelections::TimelineSelections (
  const Position &start_pos,
  const Position &end_pos)
    : ArrangerSelections (ArrangerSelections::Type::Timeline)
{
  for (auto &track : TRACKLIST->tracks_)
    {
      std::vector<ArrangerObject *> objs;
      track->append_objects (objs);

      for (auto obj : objs)
        {
          if (obj->is_start_hit_by_range (start_pos, end_pos))
            {
              std::visit (
                [&] (auto &&o) { add_object_owned (o->clone_unique ()); },
                convert_to_variant<ArrangerObjectPtrVariant> (obj));
            }
        }
    }
}

void
TimelineSelections::sort_by_indices (bool desc)
{
  auto sort_regions = [] (const auto &a, const auto &b) {
    Track * at = TRACKLIST->find_track_by_name_hash (a.id_.track_name_hash_);
    z_return_val_if_fail (at, false);
    Track * bt = TRACKLIST->find_track_by_name_hash (b.id_.track_name_hash_);
    z_return_val_if_fail (bt, false);
    if (at->pos_ < bt->pos_)
      return true;
    else if (at->pos_ > bt->pos_)
      return false;

    int have_lane = region_type_has_lane (a.id_.type_);
    /* order doesn't matter in this case */
    if (have_lane != region_type_has_lane (b.id_.type_))
      return true;

    if (have_lane)
      {
        if (a.id_.lane_pos_ < b.id_.lane_pos_)
          {
            return true;
          }
        else if (a.id_.lane_pos_ > b.id_.lane_pos_)
          {
            return false;
          }
      }
    else if (
      a.id_.type_ == RegionType::Automation
      && b.id_.type_ == RegionType::Automation)
      {
        if (a.id_.at_idx_ < b.id_.at_idx_)
          {
            return true;
          }
        else if (a.id_.at_idx_ > b.id_.at_idx_)
          {
            return false;
          }
      }

    return a.id_.idx_ < b.id_.idx_;
  };

  std::sort (
    objects_.begin (), objects_.end (),
    [desc, sort_regions] (const auto &a, const auto &b) {
      bool ret = false;
      if (a->is_region ())
        {
          ret = sort_regions (
            dynamic_cast<Region &> (*a), dynamic_cast<Region &> (*b));
        }
      else if (a->is_scale_object ())
        {
          ret =
            dynamic_cast<ScaleObject &> (*a).index_in_chord_track_
            < dynamic_cast<ScaleObject &> (*b).index_in_chord_track_;
        }
      else if (a->is_marker ())
        {
          ret =
            dynamic_cast<Marker &> (*a).marker_track_index_
            < dynamic_cast<Marker &> (*b).marker_track_index_;
        }
      return desc ? !ret : ret;
    });
}

bool
TimelineSelections::contains_looped () const
{
  return std::any_of (objects_.begin (), objects_.end (), [] (const auto &obj) {
    return obj->can_loop ()
           && dynamic_cast<LoopableObject *> (obj.get ())->is_looped ();
  });
}

bool
TimelineSelections::all_on_same_lane () const
{
  if (objects_.empty ())
    return true;

  auto first_obj =
    convert_to_variant<ArrangerObjectPtrVariant> (objects_.front ().get ());
  return std::visit (
    [&] (auto &&first) {
      if constexpr (std::derived_from<std::decay_t<decltype (first)>, Region>)
        {
          const auto &id = first->id_;
          return std::all_of (
            objects_.begin () + 1, objects_.end (), [&] (const auto &obj) {
              return std::visit (
                [&] (auto &&curr) {
                  if constexpr (
                    std::derived_from<std::decay_t<decltype (curr)>, Region>)
                    {
                      if (id.type_ != curr->id_.type_)
                        return false;

                      if constexpr (
                        std::derived_from<
                          std::decay_t<decltype (curr)>, LaneOwnedObject>)
                        return id.track_name_hash_ == curr->id_.track_name_hash_
                               && id.lane_pos_ == curr->id_.lane_pos_;
                      else if constexpr (
                        std::is_same_v<
                          std::decay_t<decltype (curr)>, AutomationRegion>)
                        return id.track_name_hash_ == curr->id_.track_name_hash_
                               && id.at_idx_ == curr->id_.at_idx_;
                      else
                        return true;
                    }
                  else
                    {
                      return false;
                    }
                },
                convert_to_variant<ArrangerObjectPtrVariant> (obj.get ()));
            });
        }
      else
        {
          return false;
        }
    },
    first_obj);
}

bool
TimelineSelections::can_be_merged () const
{
  return objects_.size () > 1 && all_on_same_lane () && !contains_looped ();
}

void
TimelineSelections::merge ()
{
  if (!all_on_same_lane ())
    {
      z_warning ("selections not on same lane");
      return;
    }

  auto ticks_length = get_length_in_ticks ();
  auto num_frames = static_cast<unsigned_frame_t> (
    ceil (AUDIO_ENGINE->frames_per_tick_ * ticks_length));
  auto [first_obj, pos] = get_first_object_and_pos (true);
  Position end_pos{ pos.ticks_ + ticks_length };

  z_return_if_fail (!first_obj || !first_obj->is_region ());
  auto &first_r = dynamic_cast<Region &> (*first_obj);

  std::unique_ptr<Region> new_r;
  switch (first_r.get_type ())
    {
    case RegionType::Midi:
      {
        new_r = std::make_unique<MidiRegion> (
          pos, end_pos, first_r.id_.track_name_hash_, first_r.id_.lane_pos_,
          first_r.id_.idx_);
        for (auto &obj : objects_)
          {
            auto  &r = dynamic_cast<MidiRegion &> (*obj);
            double ticks_diff = r.pos_.ticks_ - first_obj->pos_.ticks_;

            for (auto &mn : r.midi_notes_)
              {
                auto new_mn = mn->clone_shared ();
                new_mn->move (ticks_diff);
                dynamic_cast<MidiRegion &> (*new_r).append_object (
                  new_mn, false);
              }
          }
      }
      break;
    case RegionType::Audio:
      {
        std::vector<float> lframes (num_frames, 0);
        std::vector<float> rframes (num_frames, 0);
        std::vector<float> frames (num_frames * 2, 0);
        auto first_r_clip = dynamic_cast<AudioRegion &> (first_r).get_clip ();
        auto max_depth = first_r_clip->bit_depth_;
        z_return_if_fail (!first_r_clip->name_.empty ());
        for (auto &obj : objects_)
          {
            auto &r = dynamic_cast<AudioRegion &> (*obj);
            long  frames_diff = r.pos_.frames_ - first_obj->pos_.frames_;
            long  r_frames_length = r.get_length_in_frames ();

            auto clip = r.get_clip ();
            dsp_add2 (
              &lframes[frames_diff], clip->ch_frames_.getReadPointer (0),
              static_cast<size_t> (r_frames_length));

            max_depth = std::max (max_depth, clip->bit_depth_);
          }
        /* interleave */
        for (unsigned_frame_t i = 0; i < num_frames; i++)
          {
            frames[i * 2] = lframes[i];
            frames[i * 2 + 1] = rframes[i];
          }

        new_r = std::make_unique<AudioRegion> (
          -1, std::nullopt, true, frames.data (), num_frames,
          first_r_clip->name_, 2, max_depth, pos, first_r.id_.track_name_hash_,
          first_r.id_.lane_pos_, first_r.id_.idx_);
      }
      break;
    case RegionType::Chord:
      {
        new_r = std::make_unique<ChordRegion> (pos, end_pos, first_r.id_.idx_);
        for (auto &obj : objects_)
          {
            auto  &r = dynamic_cast<ChordRegion &> (*obj);
            double ticks_diff = r.pos_.ticks_ - first_obj->pos_.ticks_;

            for (auto &co : r.chord_objects_)
              {
                auto new_co = co->clone_shared ();
                new_co->move (ticks_diff);
                dynamic_cast<ChordRegion &> (*new_r).append_object (
                  new_co, false);
              }
          }
      }
      break;
    case RegionType::Automation:
      {
        new_r = std::make_unique<AutomationRegion> (
          pos, end_pos, first_r.id_.track_name_hash_, first_r.id_.at_idx_,
          first_r.id_.idx_);
        for (auto &obj : objects_)
          {
            auto  &r = dynamic_cast<AutomationRegion &> (*obj);
            double ticks_diff = r.pos_.ticks_ - first_obj->pos_.ticks_;

            for (auto &ap : r.aps_)
              {
                auto new_ap = ap->clone_shared ();
                new_ap->move (ticks_diff);
                dynamic_cast<AutomationRegion &> (*new_r).append_object (
                  new_ap, false);
              }
          }
      }
      break;
    }

  new_r->gen_name (first_r.name_.c_str (), nullptr, nullptr);

  clear (false);
  add_object_owned (std::move (new_r));
}

Track *
TimelineSelections::get_first_track () const
{
  return (*std::min_element (
            objects_.begin (), objects_.end (),
            [] (const auto &a, const auto &b) {
              auto track_a = a->get_track ();
              auto track_b = b->get_track ();
              return track_a && (!track_b || track_a->pos_ < track_b->pos_);
            }))
    ->get_track ();
}

Track *
TimelineSelections::get_last_track () const
{
  return (*std::max_element (
            objects_.begin (), objects_.end (),
            [] (const auto &a, const auto &b) {
              auto track_a = a->get_track ();
              auto track_b = b->get_track ();
              return !track_a || (track_b && track_a->pos_ < track_b->pos_);
            }))
    ->get_track ();
}

void
TimelineSelections::set_vis_track_indices ()
{
  auto highest_tr = get_first_track ();
  if (!highest_tr)
    return;

  for (auto &obj : objects_)
    {
      auto region_track = obj->get_track ();
      if (obj->is_region ())
        {
          region_track_vis_index_ =
            TRACKLIST->get_visible_track_diff (*highest_tr, *region_track);
        }
      else if (obj->is_marker ())
        {
          marker_track_vis_index_ =
            TRACKLIST->get_visible_track_diff (*highest_tr, *P_MARKER_TRACK);
        }
      else if (obj->is_chord_object ())
        {
          chord_track_vis_index_ =
            TRACKLIST->get_visible_track_diff (*highest_tr, *P_CHORD_TRACK);
        }
    }
}

bool
TimelineSelections::can_be_pasted_at_impl (const Position pos, const int idx) const
{
  auto tr =
    idx >= 0
      ? TRACKLIST->get_track (idx)
      : TRACKLIST_SELECTIONS->get_highest_track ();
  if (!tr)
    return false;

  for (const auto &obj : objects_)
    {
      if (obj->is_region ())
        {
          auto r = dynamic_cast<const Region *> (obj.get ());
          // automation regions can't be copy-pasted this way
          if (r->is_automation ())
            return false;

          // check if this track can host this region
          if (!Track::type_can_host_region_type (tr->type_, r->get_type ()))
            {
              z_info (
                "track {} can't host region type {}", tr->get_name (),
                r->get_type ());
              return false;
            }
        }
      else if (obj->is_marker () && !tr->is_marker ())
        return false;
      else if (obj->is_chord_object () && !tr->is_chord ())
        return false;
    }

  return true;
}

void
TimelineSelections::mark_for_bounce (bool with_parents)
{
  AUDIO_ENGINE->reset_bounce_mode ();

  for (const auto &obj : objects_)
    {
      auto track = obj->get_track ();
      if (!track)
        continue;

      if (!with_parents)
        {
          track->bounce_to_master_ = true;
        }
      track->mark_for_bounce (true, false, true, with_parents);

      if (obj->is_region ())
        {
          auto r = dynamic_cast<Region *> (obj.get ());
          r->bounce_ = true;
        }
    }
}

bool
TimelineSelections::contains_clip (const AudioClip &clip) const
{
  return std::ranges::any_of (objects_, [&clip] (const auto &obj) {
    auto r = dynamic_cast<const AudioRegion *> (obj.get ());
    return r && r->pool_id_ == clip.pool_id_;
  });
}

bool
TimelineSelections::move_regions_to_new_lanes_or_tracks_or_ats (
  const int vis_track_diff,
  const int lane_diff,
  const int vis_at_diff)
{
  /* if nothing to do return */
  if (vis_track_diff == 0 && lane_diff == 0 && vis_at_diff == 0)
    return false;

  /* only 1 operation supported at once */
  if (vis_track_diff != 0)
    {
      z_return_val_if_fail (lane_diff == 0 && vis_at_diff == 0, false);
    }
  if (lane_diff != 0)
    {
      z_return_val_if_fail (vis_track_diff == 0 && vis_at_diff == 0, false);
    }
  if (vis_at_diff != 0)
    {
      z_return_val_if_fail (lane_diff == 0 && vis_track_diff == 0, false);
    }

  /* if there are objects other than regions, moving is not supported */
  if (std::ranges::any_of (objects_, [] (const auto &obj) {
        return !obj->is_region ();
      }))
    {
      z_debug (
        "selection contains non-regions - skipping moving to another track/lane");
      return false;
    }

  /* if there are no objects do nothing */
  if (objects_.empty ())
    return false;

  sort_by_indices (false);

  /* store selected regions because they will be deselected during moving */
  std::vector<Region *> regions_arr;
  for (const auto &obj : objects_)
    {
      regions_arr.push_back (dynamic_cast<Region *> (obj.get ()));
    }

  /*
   * for tracks, check that:
   * - all regions can be moved to a compatible track
   * for lanes, check that:
   * - all regions are in the same track
   * - only lane regions are selected
   * - the lane bounds are not exceeded
   */
  bool compatible = true;
  for (auto region : regions_arr)
    {
      auto track = region->get_track ();
      if (vis_track_diff != 0)
        {
          auto visible =
            TRACKLIST->get_visible_track_after_delta (*track, vis_track_diff);
          if (
            !visible
            || !Track::type_is_compatible_for_moving (
              track->type_, visible->type_)
            ||
            /* do not allow moving automation tracks to other tracks for now */
            region->is_automation ())
            {
              compatible = false;
              break;
            }
        }
      else if (lane_diff != 0)
        {
          auto laned_track = dynamic_cast<LanedTrack *> (track);
          if (!laned_track)
            {
              compatible = false;
              break;
            }

          laned_track->block_auto_creation_and_deletion_ = true;
          if (region->id_.lane_pos_ + lane_diff < 0)
            {
              compatible = false;
              break;
            }

          /* don't create more than 1 extra lanes */
          auto r_variant = convert_to_variant<RegionPtrVariant> (region);
          compatible = std::visit (
            [&] (auto &&r) {
              if constexpr (
                std::derived_from<std::decay_t<decltype (r)>, LaneOwnedObject>)
                {
                  auto laned_track_impl = dynamic_cast<
                    LanedTrackImpl<std::decay_t<decltype (r)> *>> (track);
                  auto lane = r->get_lane ();
                  z_return_val_if_fail (region && lane, -1);
                  int new_lane_pos = lane->pos + lane_diff;
                  z_return_val_if_fail (new_lane_pos >= 0, -1);
                  if (new_lane_pos >= laned_track_impl->lanes_.size ())
                    {
                      z_debug (
                        "new lane position %d is >= the number of lanes in the track (%d)",
                        new_lane_pos, laned_track_impl->lanes_.size ());
                      return false;
                    }
                  if (
                    new_lane_pos > laned_track_impl->last_lane_created_
                    && laned_track_impl->last_lane_created_ > 0 && lane_diff > 0)
                    {
                      z_debug (
                        "already created a new lane at %d, skipping new lane for %d",
                        laned_track_impl->last_lane_created_, new_lane_pos);
                      return false;
                    }
                }
              else
                {
                  return false;
                }
            },
            r_variant);

          if (!compatible)
            {
              break;
            }
        }
      else if (vis_at_diff != 0)
        {
          if (!region->is_automation ())
            {
              compatible = false;
              break;
            }

          /* don't allow moving automation regions -- too error prone */
          compatible = false;
          break;
        }
    }
  if (!compatible)
    {
      return false;
    }

  /* new positions are all compatible, move the regions */
  for (auto region : regions_arr)
    {
      auto r_variant = convert_to_variant<RegionPtrVariant> (region);
      auto success = std::visit (
        [&] (auto &&r) {
          if (vis_track_diff != 0)
            {
              auto region_track = region->get_track ();
              z_warn_if_fail (region && region_track);
              auto track_to_move_to = TRACKLIST->get_visible_track_after_delta (
                *region_track, vis_track_diff);
              z_return_val_if_fail (track_to_move_to, false);
              r->move_to_track (track_to_move_to, -1, -1);
            }
          else if (lane_diff != 0)
            {
              if constexpr (
                std::derived_from<std::decay_t<decltype (r)>, LaneOwnedObject>)
                {
                  auto lane = r->get_lane ();
                  z_return_val_if_fail (r && lane, false);

                  int new_lane_pos = lane->pos_ + lane_diff;
                  z_return_val_if_fail (new_lane_pos >= 0, false);
                  auto laned_track = lane->get_track ();
                  bool new_lanes_created =
                    laned_track->create_missing_lanes (new_lane_pos);
                  if (new_lanes_created)
                    {
                      laned_track->last_lane_created_ = new_lane_pos;
                    }
                  r->move_to_track (laned_track, new_lane_pos, -1);
                }
            }
          else if (vis_at_diff != 0)
            {
              if constexpr (
                std::is_same_v<std::decay_t<decltype (r)>, AutomationRegion>)
                {
                  auto at = r->get_automation_track ();
                  z_return_val_if_fail (region && at, false);
                  auto              atl = at->get_automation_tracklist ();
                  AutomationTrack * new_at =
                    atl->get_visible_at_after_delta (at, vis_at_diff);

                  if (at != new_at)
                    {
                      /* TODO */
                      z_warning ("!MOVING!");
                      /*automation_track_remove_region (at, region);*/
                      /*automation_track_add_region (new_at, region);*/
                    }
                }
            }
          return true;
        },
        r_variant);

      if (!success)
        {
          z_warning ("failed to move region {}", region->get_name ());
          return false;
        }
    }

  EVENTS_PUSH (EventType::ET_TRACK_LANES_VISIBILITY_CHANGED, nullptr);

  return true;
}

bool
TimelineSelections::move_regions_to_new_ats (const int vis_at_diff)
{
  return move_regions_to_new_lanes_or_tracks_or_ats (0, 0, vis_at_diff);
}

bool
TimelineSelections::move_regions_to_new_lanes (const int diff)
{
  return move_regions_to_new_lanes_or_tracks_or_ats (0, diff, 0);
}

bool
TimelineSelections::move_regions_to_new_tracks (const int vis_track_diff)
{
  return move_regions_to_new_lanes_or_tracks_or_ats (vis_track_diff, 0, 0);
}

void
TimelineSelections::set_index_in_prev_lane ()
{
  for (auto &obj : objects_)
    {
      if (auto region = dynamic_cast<Region *> (obj.get ()))
        {
          auto r_variant = convert_to_variant<RegionPtrVariant> (region);
          std::visit (
            [&] (auto &&r) {
              if constexpr (
                std::derived_from<std::decay_t<decltype (r)>, LaneOwnedObject>)
                {
                  r->index_in_prev_lane_ = region->id_.idx_;
                }
            },
            r_variant);
        }
    }
}

bool
TimelineSelections::contains_only_regions () const
{
  return std::all_of (objects_.begin (), objects_.end (), [] (const auto &obj) {
    return obj->is_region ();
  });
}

bool
TimelineSelections::contains_only_region_types (RegionType types) const
{
  if (!contains_only_regions ())
    return false;

  return std::all_of (objects_.begin (), objects_.end (), [types] (const auto &obj) {
    auto region = dynamic_cast<Region *> (obj.get ());
    return region
           && (static_cast<int> (region->get_type ()) & static_cast<int> (types));
  });
}

bool
TimelineSelections::export_to_midi_file (
  const char * full_path,
  int          midi_version,
  bool         export_full_regions,
  bool         lanes_as_tracks) const
{
  auto mf = midiFileCreate (full_path, true);
  if (!mf)
    return false;

  /* write tempo info to track 1 */
  auto tempo_track = TRACKLIST->tempo_track_;
  midiSongAddTempo (mf, 1, static_cast<int> (tempo_track->get_current_bpm ()));

  /* all data is written out to tracks (not channels). we therefore set the
  current channel before writing data out. channel assignments can change any
  number of times during the file and affect all track messages until changed */
  midiFileSetTracksDefaultChannel (mf, 1, MIDI_CHANNEL_1);
  midiFileSetPPQN (mf, TICKS_PER_QUARTER_NOTE);
  midiFileSetVersion (mf, midi_version);

  int beats_per_bar = tempo_track->get_beats_per_bar ();
  midiSongAddSimpleTimeSig (
    mf, 1, beats_per_bar,
    math_round_double_to_signed_32 (TRANSPORT->ticks_per_beat_));

  auto sel_clone = clone_unique ();
  sel_clone->sort_by_indices (false);

  int                              last_midi_track_pos = -1;
  std::unique_ptr<MidiEventVector> events;

  for (const auto &obj : sel_clone->objects_)
    {
      auto region = dynamic_cast<MidiRegion *> (obj.get ());
      if (!region)
        continue;

      int midi_track_pos = 1;
      if (midi_version > 0)
        {
          auto track = dynamic_cast<PianoRollTrack *> (region->get_track ());
          if (!track)
            {
              z_return_val_if_fail (track, false);
            }

          std::string midi_track_name;
          if (lanes_as_tracks)
            {
              auto lane = region->get_lane ();
              z_return_val_if_fail (lane, false);
              midi_track_name =
                fmt::format ("{} - {}", track->name_, lane->name_);
              midi_track_pos = lane->calculate_lane_idx ();
            }
          else
            {
              midi_track_name = track->name_;
              midi_track_pos = track->pos_;
            }
          midiTrackAddText (
            mf, midi_track_pos, textTrackName, midi_track_name.c_str ());
        }

      if (last_midi_track_pos != midi_track_pos)
        {
          /* finish prev events (if any) */
          if (events)
            {
              events->write_to_midi_file (mf, last_midi_track_pos);
            }

          /* start new events */
          events = std::make_unique<MidiEventVector> ();
        }

      /* append to the current events */
      region->add_events (*events, nullptr, nullptr, true, export_full_regions);
      last_midi_track_pos = midi_track_pos;
    }

  /* finish prev events (if any) again */
  if (events)
    {
      events->write_to_midi_file (mf, last_midi_track_pos);
    }

  midiFileClose (mf);
  return true;
}

#if 0
/**
 * Gets index of the lowest track in the selections.
 *
 * Used during pasting.
 */
static int
get_lowest_track_pos (TimelineSelections * ts)
{
  int track_pos = INT_MAX;

#  define CHECK_POS(id) \
    { \
    }

  for (int i = 0; i < ts->num_regions; i++)
    {
      Region * r = ts->regions[i];
      Track *   tr = tracklist_find_track_by_name_hash (
          TRACKLIST, r->id.track_name_hash);
      if (tr->pos < track_pos)
        {
          track_pos = tr->pos;
        }
    }

  if (ts->num_scale_objects > 0)
    {
      if (ts->chord_track_vis_index < track_pos)
        track_pos = ts->chord_track_vis_index;
    }
  if (ts->num_markers > 0)
    {
      if (ts->marker_track_vis_index < track_pos)
        track_pos = ts->marker_track_vis_index;
    }

  return track_pos;
}
#endif