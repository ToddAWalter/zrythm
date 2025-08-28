// SPDX-FileCopyrightText: © 2018-2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "structure/tracks/track_all.h"
#include "structure/tracks/tracklist.h"

namespace zrythm::structure::tracks
{

Tracklist::Tracklist (TrackRegistry &track_registry, QObject * parent)
    : QObject (parent), track_registry_ (track_registry),
      track_collection_ (
        utils::make_qobject_unique<TrackCollection> (track_registry, this)),
      track_routing_ (
        utils::make_qobject_unique<TrackRouting> (track_registry, this)),
      track_selection_manager_ (
        std::make_unique<TrackSelectionManager> (
          selected_tracks_,
          track_registry_,
          [this] () { Q_EMIT selectedTracksChanged (); })),
      singleton_tracks_ (utils::make_qobject_unique<SingletonTracks> (this))
{
}

// ========================================================================
// QML Interface
// ========================================================================

QVariant
Tracklist::selectedTrack () const
{
  if (selected_tracks_.empty ())
    return {};
  return QVariant::fromStdVariant (
    track_registry_.find_by_id_or_throw (*selected_tracks_.begin ()));
}

void
Tracklist::setExclusivelySelectedTrack (QVariant track)
{
  auto track_var = qvariantToStdVariant<TrackPtrVariant> (track);
  get_selection_manager ().select_unique (
    TrackSpan::uuid_projection (track_var));
}

// ========================================================================

std::optional<TrackUuidReference>
Tracklist::get_track_for_plugin (const plugins::Plugin::Uuid &plugin_id) const
{
  for (const auto &tr_ref : collection ()->tracks ())
    {
      auto * tr = tracks::from_variant (tr_ref.get_object ());
      auto * channel = tr->channel ();
      if (channel == nullptr)
        continue;

      std::vector<plugins::PluginPtrVariant> plugins;
      channel->get_plugins (plugins);
      if (
        std::ranges::contains (
          plugins | std::views::transform (&plugins::plugin_ptr_variant_to_base),
          plugin_id, &plugins::Plugin::get_uuid))
        {
          return tr_ref;
        }
    }

  return std::nullopt;
}

void
Tracklist::mark_track_for_bounce (
  TrackPtrVariant track_var,
  bool            bounce,
  bool            mark_regions,
  bool            mark_children,
  bool            mark_parents)
{
// TODO
#if 0
  std::visit (
    [&] (auto &&track) {
      using TrackT = base_type<decltype (track)>;
      if constexpr (!std::derived_from<TrackT, ChannelTrack>)
        return;

      z_debug (
        "marking {} for bounce {}, mark regions {}", track->get_name (), bounce,
        mark_regions);

      track->bounce_ = bounce;

      if (mark_regions)
        {
          if constexpr (std::derived_from<TrackT, LanedTrack>)
            {
              for (auto &lane_var : track->lanes_)
                {
                  using LaneT = TrackT::TrackLaneType;
                  auto lane = std::get<LaneT *> (lane_var);
                  for (auto * region : lane->get_children_view ())
                    {
                      region->bounce_ = bounce;
                    }
                }
            }

          if constexpr (std::is_same_v<TrackT, ChordTrack>)
            for (
              auto * region :
              track->arrangement::template ArrangerObjectOwner<
                arrangement::ChordRegion>::get_children_view ())
              {
                region->bounce_ = bounce;
              }
        }

      if constexpr (std::derived_from<TrackT, ChannelTrack>)
        {
          auto * direct_out = track->channel ()->get_output_track ();
          if (direct_out && mark_parents)
            {
              std::visit (
                [&] (auto &&direct_out_derived) {
                  mark_track_for_bounce (
                    direct_out_derived, bounce, false, false, true);
                },
                convert_to_variant<TrackPtrVariant> (direct_out));
            }
        }

      if (mark_children)
        {
          if constexpr (std::derived_from<TrackT, GroupTargetTrack>)
            {
              for (auto child_id : track->children_)
                {
                  if (
                    auto child_var =
                      track->get_tracklist ()->get_track (child_id))
                    {
                      std::visit (
                        [&] (auto &&c) {
                          c->bounce_to_master_ = track->bounce_to_master_;
                          mark_track_for_bounce (
                            c, bounce, mark_regions, true, false);
                        },
                        *child_var);
                    }
                }
            }
        }
    },
    track_var);
#endif
}

int
Tracklist::get_visible_track_diff (Track::Uuid src_track, Track::Uuid dest_track)
  const
{
  const auto &tracks = collection ()->tracks ();
  const auto  src_track_index = std::distance (
    tracks.begin (),
    std::ranges::find (tracks, src_track, &TrackUuidReference::id));
  const auto dest_track_index = std::distance (
    tracks.begin (),
    std::ranges::find (tracks, dest_track, &TrackUuidReference::id));

  // Determine range boundaries and direction
  const auto [lower, upper] = std::minmax (src_track_index, dest_track_index);
  const int sign = src_track_index < dest_track_index ? 1 : -1;

  // Count visible tracks in the range (exclusive upper bound)
  auto       span = get_track_span ();
  const auto count = std::ranges::count_if (
    span.begin () + lower, span.begin () + upper, [this] (const auto &track_var) {
      return should_be_visible (TrackSpan::uuid_projection (track_var));
    });

  // Apply direction modifier
  return sign * static_cast<int> (count);
}

TrackPtrVariant
Tracklist::insert_track (const TrackUuidReference &track_id, int pos)
{
  auto track_var = track_id.get_object ();

  std::visit (
    [&] (auto &&track) {
      using TrackT = base_type<decltype (track)>;
      z_info ("inserting {} at {}", track->get_name (), pos);

      // throw if attempted to re-add a singleton track
      if (
        !Track::type_is_deletable (Track::get_type_for_class<TrackT> ())
        && get_track_span ().contains_type<TrackT> ())
        {
          throw std::invalid_argument (
            fmt::format (
              "cannot re-add track of type {} when it already exists",
              Track::get_type_for_class<TrackT> ()));
        }

      /* set to -1 so other logic knows it is a new track */
      // track->set_index (-1);

      /* this needs to be called before appending the track to the tracklist */
      track->setName (
        get_unique_name_for_track (track->get_uuid (), track->get_name ())
          .to_qstring ());

      /* insert the track */
      track_collection_->insert_track (track_id, pos);
      track->set_selection_status_getter ([&] (const TrackUuid &id) {
        return TrackSelectionManager{ selected_tracks_, track_registry_ }
          .is_selected (id);
      });

      /* remember important tracks */
      if constexpr (std::is_same_v<TrackT, MasterTrack>)
        singleton_tracks_->master_track_ = track;
      else if constexpr (std::is_same_v<TrackT, ChordTrack>)
        singleton_tracks_->chord_track_ = track;
      else if constexpr (std::is_same_v<TrackT, MarkerTrack>)
        singleton_tracks_->marker_track_ = track;
      else if constexpr (std::is_same_v<TrackT, ModulatorTrack>)
        singleton_tracks_->modulator_track_ = track;

      /* make the track the only selected track */
      get_selection_manager ().select_unique (track->get_uuid ());

      /* if audio output route to master */
      if constexpr (!std::is_same_v<TrackT, MasterTrack>)
        {
          if (
            track->get_output_signal_type () == dsp::PortType::Audio
            && singleton_tracks_->masterTrack ())
            {
              track_routing_->add_or_replace_route (
                track->get_uuid (),
                singleton_tracks_->masterTrack ()->get_uuid ());
            }
        }

      z_debug (
        "done - inserted track '{}' ({}) at {}", track->get_name (),
        track->get_uuid (), pos);
    },
    track_var);
  return track_var;
}

#if 0
ChordTrack *
Tracklist::get_chord_track () const
{
  auto span = get_track_span ();
  return std::get<ChordTrack *> (
    *std::ranges::find_if (span, TrackSpan::type_projection<ChordTrack>));
}
#endif

bool
Tracklist::should_be_visible (const Track::Uuid &track_id) const
{
  auto track = from_variant (get_track (track_id).value ());
  if (!track->visible ())
    return false;

  return true;
// TODO
#if 0
  std::vector<FoldableTrack *> parents;
  add_folder_parents (track_id, parents, false);
  return std::ranges::all_of (parents, [] (const auto &parent) {
    return parent->visible_ && !parent->folded_;
  });
#endif
}

// TODO
#if 0
void
Tracklist::add_folder_parents (
  const Track::Uuid            &track_id,
  std::vector<FoldableTrack *> &parents,
  bool                          prepend) const
{
  for (
    const auto &cur_track_var :
    get_track_span ()
      | std::views::filter (
        TrackSpan::derived_from_type_projection<FoldableTrack>))
    {
      std::visit (
        [&] (const auto &cur_track) {
          using TrackT = base_type<decltype (cur_track)>;
          if constexpr (std::derived_from<TrackT, FoldableTrack>)
            {
              /* last position covered by the foldable track cur_track */
              int last_covered_pos = cur_track->pos_ + (cur_track->size_ - 1);

              if (cur_track->pos_ < pos_ && pos_ <= last_covered_pos)
                {
                  if (prepend)
                    {
                      parents.insert (parents.begin (), cur_track);
                    }
                  else
                    {
                      parents.push_back (cur_track);
                    }
                }
            }
        },
        cur_track_var);
    }
  }

void
Tracklist::remove_from_folder_parents (const Track::Uuid &track_id)
{
  std::vector<FoldableTrack *> parents;
  add_folder_parents (track_id, parents, false);
  for (auto parent : parents)
    {
      parent->size_--;
    }
}
#endif

bool
Tracklist::multiply_track_heights (
  double multiplier,
  bool   visible_only,
  bool   check_only,
  bool   fire_events)
{
  auto span = get_track_span ();
  for (const auto &track_var : span)
    {
      bool ret = std::visit (
        [&] (auto &&track) {
          if (visible_only && !should_be_visible (track->get_uuid ()))
            return true;

          if (!track->multiply_heights (multiplier, visible_only, check_only))
            {
              return false;
            }

          if (!check_only && fire_events)
            {
              /* FIXME should be event */
              // track_widget_update_size (tr->widget_);
            }

          return true;
        },
        track_var);
      if (!ret)
        {
          return false;
        }
    }

  return true;
}

std::optional<TrackPtrVariant>
Tracklist::get_visible_track_after_delta (Track::Uuid track_id, int delta) const
{
  // TODO
  return std::nullopt;
#if 0
  auto span =
    get_track_span () | std::views::filter (TrackSpan::visible_projection);
  auto current_it =
    std::ranges::find (span, track_id, TrackSpan::uuid_projection);
  auto found = std::ranges::next (current_it, delta);
  if (found == span.end ())
    return std::nullopt;
  return *(found);
#endif
}

std::optional<TrackPtrVariant>
Tracklist::get_first_visible_track (const bool pinned) const
{
  // TODO
  return std::nullopt;
#if 0
  auto span = get_track_span ().get_visible_tracks ();
  auto it = std::ranges::find_if (span, [&] (const auto &track_var) {
    return std::visit (
      [&] (auto &&track) {
        return is_track_pinned (track->get_uuid ()) == pinned;
      },
      track_var);
  });
  return it != span.end () ? std::make_optional (*it) : std::nullopt;
#endif
}

std::optional<TrackPtrVariant>
Tracklist::get_prev_visible_track (TrackUuid track_id) const
{
  return get_visible_track_after_delta (track_id, -1);
}

std::optional<TrackPtrVariant>
Tracklist::get_next_visible_track (TrackUuid track_id) const
{
  return get_visible_track_after_delta (track_id, 1);
}

utils::Utf8String
Tracklist::get_unique_name_for_track (
  const Track::Uuid       &track_to_skip,
  const utils::Utf8String &name) const
{
  auto new_name = name;
  while (!track_name_is_unique (new_name, track_to_skip))
    {
      auto [ending_num, name_without_num] = new_name.get_int_after_last_space ();
      if (ending_num == -1)
        {
          new_name += u8" 1";
        }
      else
        {
          new_name = utils::Utf8String::from_utf8_encoded_string (
            fmt::format ("{} {}", name_without_num, ending_num + 1));
        }
    }
  return new_name;
}

void
Tracklist::remove_track (const TrackUuid &track_id)
{
  auto track_index = track_collection_->get_track_index (track_id);
  auto track_var = track_collection_->get_track_at_index (track_index);
  std::visit (
    [&] (auto &&track) {
      z_debug (
        "removing [{}] {} - num tracks before deletion: {}", track_index,
        track->get_name (), track_collection_->track_count ());

      std::optional<TrackPtrVariant> prev_visible = std::nullopt;
      std::optional<TrackPtrVariant> next_visible = std::nullopt;
      prev_visible = get_prev_visible_track (track_id);
      next_visible = get_next_visible_track (track_id);

      get_selection_manager ().remove_from_selection (track->get_uuid ());

      track_collection_->remove_track (track_id);
      track->unset_selection_status_getter ();

      // recreate the span because underlying vector changed
      auto span = get_track_span ();

      /* if it was the only track selected, select the next one */
      if (get_selection_manager ().empty ())
        {
          auto track_to_select = next_visible ? next_visible : prev_visible;
          if (!track_to_select && track_collection_->track_count () > 0)
            {
              track_to_select = span.at (0);
            }
          if (track_to_select)
            {
              get_selection_manager ().append_to_selection (
                TrackSpan::uuid_projection (*track_to_select));
            }
        }

      z_debug ("done removing track {}", track->name ());
    },
    track_var);
}

void
Tracklist::clear_selections_for_object_siblings (
  const ArrangerObject::Uuid &object_id)
{
// TODO
#if 0
  auto obj_var = PROJECT->find_arranger_object_by_id (object_id);
  std::visit (
    [&] (auto &&obj) {
      using ObjT = base_type<decltype (obj)>;
      if constexpr (std::derived_from<ObjT, arrangement::RegionOwnedObject>)
        {
          auto region = obj->get_region ();
          for (auto * child : region->get_children_view ())
            {
              auto selection_mgr =
                arrangement::PROJECT->getArrangerObjectFactory ()
                  ->get_selection_manager_for_object (*child);
              selection_mgr.remove_from_selection (child->get_uuid ());
            }
        }
      else
        {
          auto tl_objs = get_timeline_objects_in_range ();
          for (const auto &tl_obj_var : tl_objs)
            {
              std::visit (
                [&] (auto &&tl_obj) {
                  auto selection_mgr =
                    arrangement::PROJECT->getArrangerObjectFactory ()
                      ->get_selection_manager_for_object (*tl_obj);
                  selection_mgr.remove_from_selection (tl_obj->get_uuid ());
                },
                tl_obj_var);
            }
        }
    },
    *obj_var);
#endif
}

std::vector<arrangement::ArrangerObjectPtrVariant>
Tracklist::get_timeline_objects () const
{
  std::vector<ArrangerObjectPtrVariant> ret;
// TODO
#if 0
  for (const auto &track : get_track_span ())
    {
      std::vector<ArrangerObjectPtrVariant> objs;
      std::visit (
        [&] (auto &&tr) {
          tr->collect_timeline_objects (objs);

          for (auto &obj_var : objs)
            {
              std::visit (
                [&] (auto &&o) {
                  using ObjT = base_type<decltype (o)>;
                  if constexpr (arrangement::BoundedObject<ObjT>)
                    {
                      if (arrangement::ArrangerObjectSpan::bounds_projection (o)
                            ->is_hit_by_range (range->first, range->second))
                        {
                          ret.push_back (o);
                        }
                    }
                  else
                    {
                      if (o->is_start_hit_by_range (range->first, range->second))
                        {
                          ret.push_back (o);
                        }
                    }
                },
                obj_var);
            }
        },
        track);
    }
#endif
  return ret;
}

void
Tracklist::move_track (const TrackUuid track_id, int pos)
{
  auto       track_var = get_track (track_id);
  const auto track_index = collection ()->get_track_index (track_id);

  std::visit (
    [&] (auto &&track) {
      z_debug (
        "moving track: {} from {} to {}", track->get_name (), track_index, pos);

      if (pos == track_index)
        return;

      auto prev_visible = get_prev_visible_track (track_id);
      auto next_visible = get_next_visible_track (track_id);

      {
        /* clear the editor region if it exists and belongs to this track */
        // CLIP_EDITOR->unset_region_if_belongs_to_track (track_id);

        /* deselect all objects */
        // TODO
        // track->Track::unselect_all ();

        get_selection_manager ().remove_from_selection (track->get_uuid ());

        /* if it was the only track selected, select the next one */
        if (get_selection_manager ().empty () && (prev_visible || next_visible))
          {
            auto track_to_add = next_visible ? *next_visible : *prev_visible;
            get_selection_manager ().append_to_selection (
              TrackSpan::uuid_projection (track_to_add));
          }
      }

      collection ()->move_track (track_id, pos);

      /* make the track the only selected track */
      get_selection_manager ().select_unique (track_id);

      z_debug ("finished moving track");
    },
    *track_var);
}

bool
Tracklist::track_name_is_unique (
  const utils::Utf8String &name,
  const TrackUuid          track_to_skip) const
{
  auto track_ids_to_check = std::ranges::to<std::vector> (
    std::views::filter (collection ()->tracks (), [&] (const auto &ref) {
      return ref.id () != track_to_skip;
    }));
  return !TrackSpan{ track_ids_to_check }.contains_track_name (name);
}

void
init_from (
  Tracklist             &obj,
  const Tracklist       &other,
  utils::ObjectCloneType clone_type)
{
  obj.pinned_tracks_cutoff_ = other.pinned_tracks_cutoff_;

  if (clone_type == utils::ObjectCloneType::Snapshot)
    {
      // init_from(*obj.track_collection_, *other.track_collection_);
      obj.selected_tracks_ = other.selected_tracks_;
    }
  else if (clone_type == utils::ObjectCloneType::NewIdentity)
    {
    }
}

void
Tracklist::handle_click (TrackUuid track_id, bool ctrl, bool shift, bool dragged)
{
  const auto track_var_opt = get_track (track_id);
  z_return_if_fail (track_var_opt.has_value ());
  auto span = get_track_span ();
  auto selected_tracks =
    std::views::filter (span, TrackSpan::selected_projection)
    | std::ranges::to<std::vector> ();
  bool is_selected = get_selection_manager ().is_selected (track_id);
  if (is_selected)
    {
      if ((ctrl || shift) && !dragged)
        {
          if (get_selection_manager ().size () > 1)
            {
              get_selection_manager ().remove_from_selection (track_id);
            }
        }
      else
        {
          /* do nothing */
        }
    }
  else /* not selected */
    {
      if (shift)
        {
          if (!selected_tracks.empty ())
            {
// TODO
#if 0
              TrackSpan selected_tracks_span{ selected_tracks };
              auto      highest_var = selected_tracks_span.get_first_track ();
              auto      lowest_var = selected_tracks_span.get_last_track ();
              std::visit (
                [&] (auto &&highest, auto &&lowest) {
                  const auto track_index = get_track_index (track_id);
                  const auto highest_index =
                    get_track_index (highest->get_uuid ());
                  const auto lowest_index =
                    get_track_index (lowest->get_uuid ());

                  if (track_index > highest_index)
                    {
                      /* select all tracks in between */
                      auto tracks_to_select = std::span (
                        tracks_.begin () + highest_index,
                        tracks_.begin () + track_index);
                      get_selection_manager ().select_only_these (
                        tracks_to_select
                        | std::views::transform ([&] (const auto &id_ref) {
                            return id_ref.id ();
                          }));
                    }
                  else if (track_index < lowest_index)
                    {
                      /* select all tracks in between */
                      auto tracks_to_select = std::span (
                        tracks_.begin () + track_index,
                        tracks_.begin () + lowest_index);
                      get_selection_manager ().select_only_these (
                        tracks_to_select
                        | std::views::transform ([&] (const auto &id_ref) {
                            return id_ref.id ();
                          }));
                    }
                },
                highest_var, lowest_var);
#endif
            }
        }
      else if (ctrl)
        {
          // append to selections
          get_selection_manager ().append_to_selection (track_id);
        }
      else
        {
          // select exclusively
          get_selection_manager ().select_unique (track_id);
        }
    }
}

void
from_json (const nlohmann::json &j, Tracklist &t)
{
  j.at (Tracklist::kPinnedTracksCutoffKey).get_to (t.pinned_tracks_cutoff_);
  // TODO
  // j.at (Tracklist::kTracksKey).get_to (t.tracks_);
  j.at (Tracklist::kSelectedTracksKey).get_to (t.selected_tracks_);
  j.at (Tracklist::kTracksKey).get_to (*t.track_collection_);
}
}
