// SPDX-FileCopyrightText: © 2018-2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "common/dsp/audio_bus_track.h"
#include "common/dsp/audio_group_track.h"
#include "common/dsp/audio_lane.h"
#include "common/dsp/audio_region.h"
#include "common/dsp/audio_track.h"
#include "common/dsp/automation_point.h"
#include "common/dsp/automation_track.h"
#include "common/dsp/channel.h"
#include "common/dsp/chord_track.h"
#include "common/dsp/foldable_track.h"
#include "common/dsp/folder_track.h"
#include "common/dsp/group_target_track.h"
#include "common/dsp/instrument_track.h"
#include "common/dsp/marker_track.h"
#include "common/dsp/midi_bus_track.h"
#include "common/dsp/midi_event.h"
#include "common/dsp/midi_group_track.h"
#include "common/dsp/midi_lane.h"
#include "common/dsp/midi_track.h"
#include "common/dsp/modulator_macro_processor.h"
#include "common/dsp/modulator_track.h"
#include "common/dsp/router.h"
#include "common/dsp/tempo_track.h"
#include "common/dsp/track.h"
#include "common/dsp/track_processor.h"
#include "common/dsp/tracklist.h"
#include "common/utils/debug.h"
#include "common/utils/flags.h"
#include "common/utils/gtest_wrapper.h"
#include "common/utils/logger.h"
#include "common/utils/rt_thread_id.h"
#include "common/utils/string.h"
#include "gui/backend/backend/actions/tracklist_selections.h"
#include "gui/backend/backend/actions/undo_manager.h"
#include "gui/backend/backend/project.h"
#include "gui/backend/backend/zrythm.h"

Track::Track (
  Type        type,
  std::string name,
  int         pos,
  PortType    in_signal_type,
  PortType    out_signal_type)
    : pos_ (pos), type_ (type), name_ (std::move (name)),
      in_signal_type_ (in_signal_type), out_signal_type_ (out_signal_type)
{
  z_debug ("creating {} track '{}'", type, name_);
}

Tracklist *
Track::get_tracklist () const
{
  if (tracklist_ != nullptr)
    return tracklist_;

  if (is_auditioner ())
    {
      return SAMPLE_PROCESSOR->tracklist_.get ();
    }
  else
    {
      return TRACKLIST;
    }
}

PortConnectionsManager *
Track::get_port_connections_manager () const
{
  auto * tracklist = get_tracklist ();
  z_return_val_if_fail (tracklist, nullptr);
  z_return_val_if_fail (tracklist_->port_connections_manager_, nullptr);
  return tracklist->port_connections_manager_;
}

Track *
Track::from_variant (const TrackPtrVariant &variant)
{
  return std::visit ([&] (auto &&t) -> Track * { return t; }, variant);
}

std::unique_ptr<Track>
Track::create_track (Track::Type type, const std::string &name, int pos)
{
  switch (type)
    {
    case Track::Type::Instrument:
      return *InstrumentTrack::create_unique (name, pos);
      break;
    case Track::Type::Audio:
      return *AudioTrack::create_unique (name, pos, AUDIO_ENGINE->sample_rate_);
      break;
    case Track::Type::AudioBus:
      return *AudioBusTrack::create_unique (name, pos);
      break;
    case Track::Type::AudioGroup:
      return *AudioGroupTrack::create_unique (name, pos);
      break;
    case Track::Type::Midi:
      return *MidiTrack::create_unique (name, pos);
      break;
    case Track::Type::MidiBus:
      return *MidiBusTrack::create_unique (name, pos);
      break;
    case Track::Type::MidiGroup:
      return *MidiGroupTrack::create_unique (name, pos);
      break;
    case Track::Type::Folder:
      return *FolderTrack::create_unique (name, pos);
      break;
    case Track::Type::Master:
    case Track::Type::Chord:
    case Track::Type::Marker:
    case Track::Type::Tempo:
    case Track::Type::Modulator:
    default:
      z_return_val_if_reached (nullptr);
      break;
    }
}

void
Track::copy_members_from (const Track &other)
{
  pos_ = other.pos_;
  type_ = other.type_;
  name_ = other.name_;
  name_hash_ = other.name_hash_;
  icon_name_ = other.icon_name_;
  visible_ = other.visible_;
  filtered_ = other.filtered_;
  main_height_ = other.main_height_;
  enabled_ = other.enabled_;
  color_ = other.color_;
  trigger_midi_activity_ = other.trigger_midi_activity_;
  in_signal_type_ = other.in_signal_type_;
  out_signal_type_ = other.out_signal_type_;
  comment_ = other.comment_;
  bounce_ = other.bounce_;
  bounce_to_master_ = other.bounce_to_master_;
  frozen_ = other.frozen_;
  pool_id_ = other.pool_id_;
  disconnecting_ = other.disconnecting_;
}

void
Track::select (bool select, bool exclusive, bool fire_events)
{
  if (select)
    {
      if (exclusive)
        {
          TRACKLIST_SELECTIONS->select_single (*this, fire_events);
        }
      else
        {
          TRACKLIST_SELECTIONS->add_track (*this, fire_events);
        }
    }
  else
    {
      TRACKLIST_SELECTIONS->remove_track (*this, fire_events);
    }

  if (fire_events)
    {
      // EVENTS_PUSH (EventType::ET_TRACK_CHANGED, this);
    }
}

std::unique_ptr<Track>
Track::create_unique_from_type (Type type)
{
  switch (type)
    {
    case Track::Type::Instrument:
      return *InstrumentTrack::create_unique ();
    case Track::Type::Audio:
      return *AudioTrack::create_unique ();
    case Track::Type::AudioBus:
      return *AudioBusTrack::create_unique ();
    case Track::Type::AudioGroup:
      return *AudioGroupTrack::create_unique ();
    case Track::Type::Midi:
      return *MidiTrack::create_unique ();
    case Track::Type::MidiBus:
      return *MidiBusTrack::create_unique ();
    case Track::Type::MidiGroup:
      return *MidiGroupTrack::create_unique ();
    case Track::Type::Folder:
      return *FolderTrack::create_unique ();
    case Track::Type::Master:
      return *MasterTrack::create_unique ();
    case Track::Type::Chord:
      return *ChordTrack::create_unique ();
    case Track::Type::Marker:
      return *MarkerTrack::create_unique ();
    case Track::Type::Tempo:
      return *TempoTrack::create_unique ();
    case Track::Type::Modulator:
      return *ModulatorTrack::create_unique ();
    default:
      z_return_val_if_reached (nullptr);
    }
}

bool
Track::is_in_active_project () const
{
  return tracklist_ && tracklist_->is_in_active_project ();
}

bool
Track::is_auditioner () const
{
  return tracklist_ && tracklist_->is_auditioner ();
}

Track::Type
Track::type_get_from_plugin_descriptor (
  const zrythm::plugins::PluginDescriptor &descr)
{
  if (descr.is_instrument ())
    return Track::Type::Instrument;
  else if (descr.is_midi_modifier ())
    return Track::Type::Midi;
  else
    return Track::Type::AudioBus;
}

template <FinalRegionSubclass T>
T *
Track::insert_region (
  T *               region,
  AutomationTrack * at,
  int               lane_pos,
  int               idx,
  bool              gen_name,
  bool              fire_events)
{
  z_return_val_if_fail (region->validate (false, 0), nullptr);
  z_return_val_if_fail (
    type_can_have_region_type (type_, region->id_.type_), nullptr);

  if (gen_name)
    {
      region->gen_name (nullptr, at, this);
    }

  z_return_val_if_fail (region->name_.length () > 0, nullptr);
  z_debug (
    "inserting region '{}' to track '{}' at lane {} (idx {})", region->name_,
    name_, lane_pos, idx);

  T * added_region = nullptr;
  if constexpr (RegionImpl<T>::is_laned ())
    {
      using LanedTrackT = TrackLaneImpl<T>::LanedTrackT;
      auto laned_track = dynamic_cast<LanedTrackT *> (this);
      z_return_val_if_fail (laned_track, nullptr);

      /* enable extra lane if necessary */
      laned_track->create_missing_lanes (lane_pos);

      auto lane = std::get<typename LanedTrackT::TrackLaneType *> (
        laned_track->lanes_.at (lane_pos));
      z_return_val_if_fail (lane, nullptr);
      if (idx == -1)
        {
          lane->add_region (region);
          added_region = region;
        }
      else
        {
          lane->insert_region (region, idx);
          added_region = region;
        }
      z_return_val_if_fail (added_region != nullptr, nullptr);
      z_return_val_if_fail (added_region->id_.idx_ >= 0, nullptr);
    }
  else if constexpr (std::is_same_v<T, AutomationRegion>)
    {
      z_return_val_if_fail (at, nullptr);
      if (idx == -1)
        {
          at->add_region (region);
          added_region = region;
        }
      else
        {
          at->insert_region (region, idx);
          added_region = region;
        }
    }
  else if constexpr (std::is_same_v<T, ChordRegion>)
    {
      auto chord_track = dynamic_cast<ChordTrack *> (this);
      z_return_val_if_fail (chord_track, nullptr);

      chord_track->RegionOwnerImpl<ChordRegion>::insert_region (
        region, idx == -1 ? chord_track->region_list_->regions_.size () : idx);
      added_region = region;
    }
  z_return_val_if_fail (added_region, nullptr);
  z_return_val_if_fail (
    added_region->track_name_hash_ == get_name_hash (), nullptr);

  /* write clip if audio region */
  if constexpr (std::is_same_v<T, AudioRegion>)
    {
      if (!is_auditioner ())
        {
          auto clip = added_region->get_clip ();
          z_return_val_if_fail (clip, nullptr);
          clip->write_to_pool (false, false);
        }
    }

  z_debug ("inserted: {}", added_region->print_to_str ());

  if (fire_events)
    {
      // EVENTS_PUSH (EventType::ET_ARRANGER_OBJECT_CREATED, added_region.get ());

      if constexpr (RegionImpl<T>::is_laned ())
        {
          // EVENTS_PUSH (EventType::ET_TRACK_LANE_ADDED, nullptr);
        }
    }

  return added_region;
}

void
Track::add_folder_parents (std::vector<FoldableTrack *> &parents, bool prepend)
  const
{
  for (auto cur_track : TRACKLIST->tracks_ | type_is<FoldableTrack> ())
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
}

void
Track::remove_from_folder_parents ()
{
  std::vector<FoldableTrack *> parents;
  add_folder_parents (parents, false);
  for (auto parent : parents)
    {
      parent->size_--;
    }
}

bool
Track::type_can_host_region_type (const Track::Type tt, const RegionType rt)
{
  switch (rt)
    {
    case RegionType::Midi:
      return tt == Track::Type::Midi || tt == Track::Type::Instrument;
    case RegionType::Audio:
      return tt == Track::Type::Audio;
    case RegionType::Automation:
      return tt != Track::Type::Chord && tt != Track::Type::Marker;
    case RegionType::Chord:
      return tt == Track::Type::Chord;
    }
  z_return_val_if_reached (false);
}

bool
Track::should_be_visible () const
{
  if (!visible_ || filtered_)
    return false;

  std::vector<FoldableTrack *> parents;
  add_folder_parents (parents, false);
  for (auto parent : parents)
    {
      if (!parent->visible_ || parent->folded_)
        return false;
    }

  return true;
}

double
Track::get_full_visible_height () const
{
  double height = main_height_;

  if (has_lanes ())
    {
      height += std::visit (
        [] (const auto &laned_track) {
          return laned_track->get_visible_lane_heights ();
        },
        convert_to_variant<LanedTrackPtrVariant> (this));
    }
  if (type_has_automation (type_))
    {
      const auto * automatable_track =
        dynamic_cast<const AutomatableTrack *> (this);
      if (automatable_track && automatable_track->automation_visible_)
        {
          const AutomationTracklist &atl =
            automatable_track->get_automation_tracklist ();
          for (const auto &at : atl.visible_ats_)
            {
              z_warn_if_fail (at->height_ > 0);
              if (at->visible_)
                height += at->height_;
            }
        }
    }
  return height;
}

bool
Track::multiply_heights (double multiplier, bool visible_only, bool check_only)
{
  if (main_height_ * multiplier < TRACK_MIN_HEIGHT)
    return false;

  if (!check_only)
    {
      main_height_ *= multiplier;
    }

  if (type_has_lanes (type_))
    {
      auto ret = std::visit (
        [&] (auto &&track) {
          using TrackT = base_type<decltype (track)>;
          if (!visible_only || track->lanes_visible_)
            {
              for (auto &lane_var : track->lanes_)
                {
                  using TrackLaneT = TrackT::LanedTrackImpl::TrackLaneType;
                  auto lane = std::get<TrackLaneT *> (lane_var);
                  if (lane->height_ * multiplier < TRACK_MIN_HEIGHT)
                    {
                      return false;
                    }

                  if (!check_only)
                    {
                      lane->height_ *= multiplier;
                    }
                }
            }
          return true;
        },
        convert_to_variant<LanedTrackPtrVariant> (this));
      if (!ret)
        {
          return false;
        }
    }
  if (type_has_automation (type_))
    {
      AutomatableTrack * automatable_track =
        dynamic_cast<AutomatableTrack *> (this);
      if (
        automatable_track
        && (!visible_only || automatable_track->automation_visible_))
        {
          AutomationTracklist &atl =
            automatable_track->get_automation_tracklist ();
          for (auto &at : atl.ats_)
            {
              if (visible_only && !at->visible_)
                continue;

              if (at->height_ * multiplier < TRACK_MIN_HEIGHT)
                {
                  return false;
                }

              if (!check_only)
                {
                  at->height_ *= multiplier;
                }
            }
        }
    }

  return true;
}

bool
Track::is_selected () const
{
  return TRACKLIST_SELECTIONS->contains_track (*this);
}

bool
Track::contains_uninstantiated_plugin () const
{
  std::vector<zrythm::plugins::Plugin *> plugins;
  get_plugins (plugins);
  return std::ranges::any_of (plugins, [] (auto pl) {
    return pl->instantiation_failed_;
  });
}

#if 0
static void
freeze_progress_close_cb (ExportData * data)
{
  g_thread_join (data->thread);

  Track * self = (Track *) g_ptr_array_index (data->tracks, 0);

  exporter_post_export (data->info, data->conns, data->state);

  /* assert exporting is finished */
  z_return_if_fail (!AUDIO_ENGINE->exporting);

  if (
    data->info->progress_info->get_completion_type ()
    == ProgressInfo::CompletionType::SUCCESS)
    {
      /* move the temporary file to the pool */
      GError *    err = NULL;
      AudioClip * clip = audio_clip_new_from_file (data->info->file_uri, &err);
      if (!clip)
        {
          HANDLE_ERROR (
            err, QObject::tr ("Failed creating audio clip from file at {}"),
            data->info->file_uri);
          return;
        }
      audio_pool_add_clip (AUDIO_POOL, clip);
      err = NULL;
      bool success =
        audio_clip_write_to_pool (clip, F_NO_PARTS, F_NOT_BACKUP, &err);
      if (!success)
        {
          HANDLE_ERROR (
            err, "Failed to write frozen audio for track '{}' to pool",
            self->name);
          return;
        }
      self->pool_id_ = clip->pool_id_;
    }

  if (g_file_test (data->info->file_uri, G_FILE_TEST_IS_REGULAR))
    {
      io_remove (data->info->file_uri);
    }

  self->frozen = true;
  // EVENTS_PUSH (EventType::ET_TRACK_FREEZE_CHANGED, self);
}

bool
track_freeze (Track * self, bool freeze, GError ** error)
{
  z_info ("{}freezing {}...", freeze ? "" : "un", self->name);

  if (freeze)
    {
      ExportSettings * info = export_settings_new ();
      ExportData *     data = export_data_new (nullptr, info);
      data->tracks = g_ptr_array_new ();
      g_ptr_array_add (data->tracks, self);
      self->bounce_to_master = true;
      track_mark_for_bounce (
        self, F_BOUNCE, F_MARK_REGIONS, F_NO_MARK_CHILDREN, F_NO_MARK_PARENTS);
      data->info->mode = Exporter::Mode::EXPORT_MODE_TRACKS;
      export_settings_set_bounce_defaults (
        data->info, Exporter::Format::WAV, "", self->name);

      data->conns = exporter_prepare_tracks_for_export (data->info, data->state);

      /* start exporting in a new thread */
      data->thread = g_thread_new (
        "bounce_thread", (GThreadFunc) exporter_generic_export_thread,
        data->info);
      Exporter exporter (*info);
      data->thread = exporter.begin_generic_thread ();

      /* create a progress dialog and block */
      ExportProgressDialogWidget * progress_dialog =
        export_progress_dialog_widget_new (
          data, true, freeze_progress_close_cb, false, F_CANCELABLE);
      adw_dialog_present (
        ADW_DIALOG (progress_dialog), GTK_WIDGET (MAIN_WINDOW));
      return true;
    }
  else
    {
      /* FIXME */
      /*audio_pool_remove_clip (*/
      /*AUDIO_POOL, self->pool_id_, true);*/

      self->frozen = false;
      // EVENTS_PUSH (EventType::ET_TRACK_FREEZE_CHANGED, self);
    }

  return true;
}
#endif

template <typename T>
T *
Track::insert_plugin (
  std::unique_ptr<T>            &&pl,
  zrythm::plugins::PluginSlotType slot_type,
  int                             slot,
  bool                            instantiate_plugin,
  bool                            replacing_plugin,
  bool                            moving_plugin,
  bool                            confirm,
  bool                            gen_automatables,
  bool                            recalc_graph,
  bool                            fire_events)
{
  if (!zrythm::plugins::PluginIdentifier::validate_slot_type_slot_combo (
        slot_type, slot))
    {
      z_return_val_if_reached (nullptr);
    }

  T * inserted_plugin = nullptr;

  if (slot_type == zrythm::plugins::PluginSlotType::Modulator)
    {
      auto * modulator_track = dynamic_cast<ModulatorTrack *> (this);
      if (modulator_track)
        {
          std::shared_ptr<T> pl_shared = std::move (pl);
          inserted_plugin =
            modulator_track
              ->insert_modulator (
                slot, pl_shared, replacing_plugin, confirm, gen_automatables,
                recalc_graph, fire_events)
              .get ();
        }
    }
  else
    {
      auto * channel_track = dynamic_cast<ChannelTrack *> (this);
      if (channel_track)
        {
          inserted_plugin = dynamic_cast<
            T *> (channel_track->get_channel ()->add_plugin (
            std::move (pl), slot_type, slot, confirm, moving_plugin,
            gen_automatables, recalc_graph, fire_events));
        }
    }

  if (
    inserted_plugin && !inserted_plugin->instantiated_
    && !inserted_plugin->instantiation_failed_)
    {
      try
        {
          inserted_plugin->instantiate ();
        }
      catch (const ZrythmException &e)
        {
          e.handle ("Failed to instantiate plugin");
        }
    }

  return inserted_plugin;
}

void
Track::remove_plugin (
  zrythm::plugins::PluginSlotType slot_type,
  int                             slot,
  bool                            replacing_plugin,
  bool                            moving_plugin,
  bool                            deleting_plugin,
  bool                            deleting_track,
  bool                            recalc_graph)
{
  z_debug ("removing plugin from track {}", name_);
  if (slot_type == zrythm::plugins::PluginSlotType::Modulator)
    {
      auto * modulator_track = dynamic_cast<ModulatorTrack *> (this);
      if (modulator_track)
        {
          modulator_track->remove_modulator (
            slot, deleting_plugin, deleting_track, recalc_graph);
        }
    }
  else
    {
      auto * channel_track = dynamic_cast<ChannelTrack *> (this);
      if (channel_track)
        {
          channel_track->get_channel ()->remove_plugin (
            slot_type, slot, moving_plugin, deleting_plugin, deleting_track,
            recalc_graph);
        }
    }
}

void
Track::disconnect (bool remove_pl, bool recalc_graph)
{
  z_debug ("disconnecting track '{}' ({})...", name_, pos_);

  disconnecting_ = true;

  /* if this is a group track and has children, remove them */
  if (is_in_active_project () && !is_auditioner () && can_be_group_target ())
    {
      auto * group_target = dynamic_cast<GroupTargetTrack *> (this);
      if (group_target)
        {
          group_target->remove_all_children (true, false, false);
        }
    }

  /* disconnect all ports and free buffers */
  std::vector<Port *> ports;
  append_ports (ports, true);
  for (auto port : ports)
    {
      if (port->is_in_active_project () != is_in_active_project ())
        {
          z_error ("invalid port");
          return;
        }

      port->disconnect_all ();
    }

  if (is_in_active_project () && !is_auditioner ())
    {
      /* disconnect from folders */
      remove_from_folder_parents ();
    }

  if (recalc_graph)
    {
      ROUTER->recalc_graph (false);
    }

  if (has_channel ())
    {
      auto channel_track = dynamic_cast<ChannelTrack *> (this);
      channel_track->channel_->disconnect (remove_pl);
    }

  disconnecting_ = false;

  z_debug ("done disconnecting");
}

void
Track::unselect_all ()
{
  if (is_auditioner ())
    return;

  std::vector<ArrangerObject *> objs;
  append_objects (objs);
  for (auto obj : objs)
    {
      obj->select (false, false, false);
    }
}

void
Track::append_objects (std::vector<ArrangerObject *> &objs) const
{
  std::visit (
    [&] (auto &&self) {
      using TrackT = base_type<decltype (self)>;

      if constexpr (std::derived_from<TrackT, LanedTrack>)
        {
          for (auto &lane_var : self->lanes_)
            {
              using TrackLaneT = TrackT::LanedTrackImpl::TrackLaneType;
              auto lane = std::get<TrackLaneT *> (lane_var);
              for (auto &region_var : lane->region_list_->regions_)
                {
                  auto * region =
                    std::get<typename TrackLaneT::RegionT *> (region_var);
                  objs.push_back (region);
                }
            }
        }

      if constexpr (std::derived_from<TrackT, RegionOwner>)
        {
          for (auto &region_var : self->region_list_->regions_)
            {
              auto * region = std::get<typename TrackT::RegionTPtr> (region_var);
              objs.push_back (region);
            }
        }

      if constexpr (std::is_same_v<TrackT, ChordTrack>)
        {
          for (auto &scale : self->scales_)
            {
              objs.push_back (scale);
            }
        }
      else if constexpr (std::is_same_v<TrackT, MarkerTrack>)
        {
          for (auto &marker : self->markers_)
            {
              objs.push_back (marker);
            }
        }
      if constexpr (std::derived_from<TrackT, AutomatableTrack>)
        {
          for (auto &at : self->get_automation_tracklist ().ats_)
            {
              for (auto &region_var : at->region_list_->regions_)
                {
                  auto * region = std::get<AutomationRegion *> (region_var);
                  objs.push_back (region);
                }
            }
        }
    },
    convert_to_variant<TrackPtrVariant> (const_cast<Track *> (this)));
}

bool
Track::validate_base () const
{
  std::vector<Port *> ports;
  append_ports (ports, true);
  return std::ranges::all_of (ports, [this] (const Port * port) {
    const auto port_in_active_prj = port->is_in_active_project ();
    const auto track_in_active_prj = is_in_active_project ();
    if (port_in_active_prj != track_in_active_prj)
      {
        z_warning (
          "port '{}' in active project ({}) != track '{}' in active project ({})",
          port->get_label (), port_in_active_prj, get_name (),
          track_in_active_prj);
      }
    return port_in_active_prj == track_in_active_prj;
  });
}

void
Track::update_positions (bool from_ticks, bool bpm_change)
{
  /* not ready yet */
  if (!PROJECT || !AUDIO_ENGINE->pre_setup_)
    {
      z_warning ("not ready to update positions for {} yet", name_);
      return;
    }

  std::vector<ArrangerObject *> objects;
  append_objects (objects);
  for (auto obj : objects)
    {
      if (ZRYTHM_TESTING)
        obj->validate (is_in_active_project (), 0);
      obj->update_positions (from_ticks, bpm_change);
      if (ZRYTHM_TESTING)
        obj->validate (is_in_active_project (), 0);
    }
}

bool
Track::set_name_with_action_full (const std::string &name)
{
  try
    {
      UNDO_MANAGER->perform (std::make_unique<RenameTrackAction> (
        *this, *PORT_CONNECTIONS_MGR, name));
      return true;
    }
  catch (const ZrythmException &ex)
    {
      ex.handle (QObject::tr ("Failed to rename track"));
      return false;
    }
}

void
Track::set_name_with_action (const std::string &name)
{
  set_name_with_action_full (name);
}

void
Track::add_region_if_in_range (
  const Position *       p1,
  const Position *       p2,
  std::vector<Region *> &regions,
  Region *               region)
{
  if (!p1 && !p2)
    {
      regions.push_back (region);
      return;
    }
  else
    {
      z_return_if_fail (p1 && p2);
    }

  if (region->is_hit_by_range (*p1, *p2))
    {
      regions.push_back (region);
    }
}

std::string
Track::get_unique_name (
  const Tracklist   &tracklist,
  Track *            track_to_skip,
  const std::string &name)
{
  std::string new_name = name;
  while (!tracklist.track_name_is_unique (new_name, track_to_skip))
    {
      auto [ending_num, name_without_num] =
        string_get_int_after_last_space (new_name);
      if (ending_num == -1)
        {
          new_name += " 1";
        }
      else
        {
          new_name = fmt::format ("{} {}", name_without_num, ending_num + 1);
        }
    }
  return new_name;
}

void
Track::set_name (
  const Tracklist   &tracklist,
  const std::string &name,
  bool               pub_events)
{
  auto         new_name = get_unique_name (tracklist, this, name);
  unsigned int old_hash = name_.empty () ? 0 : get_name_hash ();
  name_ = new_name;
  unsigned int new_hash = get_name_hash ();

  if (old_hash != 0)
    {
      update_name_hash (new_hash);

      std::vector<ArrangerObject *> objects;
      append_objects (objects);
      for (auto obj : objects)
        {
          obj->set_track_name_hash (new_hash);
        }

      std::vector<Port *> ports;
      append_ports (ports, true);
      for (auto port : ports)
        {
          port->update_track_name_hash (*this, new_hash);
          if (port->is_exposed_to_backend ())
            {
              port->rename_backend ();
            }
        }

      auto processable_track = dynamic_cast<ProcessableTrack *> (this);
      if (processable_track)
        processable_track->processor_->track_ = processable_track;

      auto channel_track = dynamic_cast<ChannelTrack *> (this);
      if (channel_track)
        channel_track->channel_->update_track_name_hash (old_hash, new_hash);
    }

  if (is_in_active_project ())
    {

      auto group_target_track = dynamic_cast<GroupTargetTrack *> (this);
      if (group_target_track)
        {
          group_target_track->update_children ();
        }

      if (
        MIXER_SELECTIONS->has_any ()
        && MIXER_SELECTIONS->track_name_hash_ == old_hash)
        {
          MIXER_SELECTIONS->track_name_hash_ = new_hash;
        }

      if (
        CLIP_EDITOR->has_region_
        && CLIP_EDITOR->region_id_.track_name_hash_ == old_hash)
        {
          z_debug ("updating clip editor region track to {}", name_);
          CLIP_EDITOR->region_id_.track_name_hash_ = new_hash;
        }
    }

  // added 2024/10/13
  name_hash_ = new_hash;

  if (pub_events)
    {
      // EVENTS_PUSH (EventType::ET_TRACK_NAME_CHANGED, this);
    }
}

void
Track::get_plugins (std::vector<zrythm::plugins::Plugin *> &arr) const
{
  if (type_has_channel (type_))
    {
      auto channel_track = dynamic_cast<const ChannelTrack *> (this);
      channel_track->channel_->get_plugins (arr);
    }

  if (type_ == Type::Modulator)
    {
      auto modulator_track = dynamic_cast<const ModulatorTrack *> (this);
      for (const auto &modulator : modulator_track->modulators_)
        {
          if (modulator)
            {
              arr.push_back (modulator.get ());
            }
        }
    }
}

void

Track::activate_all_plugins (bool activate)
{
  std::vector<zrythm::plugins::Plugin *> pls;
  get_plugins (pls);

  for (auto pl : pls)
    {
      if (!pl->instantiated_ && !pl->instantiation_failed_)
        {
          try
            {

              pl->instantiate ();
            }
          catch (const ZrythmException &e)
            {
              e.handle ("Failed to instantiate plugin");
            }
        }

      if (pl->instantiated_)
        {
          pl->activate (activate);
        }
    }
}

void
Track::set_comment (const std::string &comment, bool undoable)
{
  if (undoable)
    {
      select (true, true, false);

      try
        {
          UNDO_MANAGER->perform (
            std::make_unique<EditTrackCommentAction> (*this, comment));
        }
      catch (const ZrythmException &e)
        {
          e.handle (QObject::tr ("Failed to set track comment"));
          return;
        }
    }
  else
    {
      comment_ = comment;
    }
}

void
Track::set_color (const Color &color, bool undoable, bool fire_events)
{
  if (undoable)
    {
      select (true, true, false);

      try
        {
          UNDO_MANAGER->perform (
            std::make_unique<EditTrackColorAction> (*this, color));
        }
      catch (const ZrythmException &e)
        {
          e.handle (QObject::tr ("Failed to set track color"));
          return;
        }
    }
  else
    {
      color_ = color;

      if (fire_events)
        {
          // EVENTS_PUSH (EventType::ET_TRACK_COLOR_CHANGED, this);
        }
    }
}

void
Track::set_icon (const std::string &icon_name, bool undoable, bool fire_events)
{
  if (undoable)
    {
      select (true, true, false);

      try
        {
          UNDO_MANAGER->perform (
            std::make_unique<EditTrackIconAction> (*this, icon_name));
        }
      catch (const ZrythmException &e)
        {
          e.handle (QObject::tr ("Cannot set track icon"));
          return;
        }
    }
  else
    {
      icon_name_ = icon_name;

      if (fire_events)
        {
          // EVENTS_PUSH (EventType::ET_TRACK_STATE_CHANGED, this);
        }
    }
}

zrythm::plugins::Plugin *
Track::get_plugin_at_slot (zrythm::plugins::PluginSlotType slot_type, int slot)
  const
{
  if (auto channel_track = dynamic_cast<const ChannelTrack *> (this))
    {
      auto &channel = channel_track->get_channel ();
      switch (slot_type)
        {
        case zrythm::plugins::PluginSlotType::MidiFx:
          return channel->midi_fx_[slot].get ();
        case zrythm::plugins::PluginSlotType::Instrument:
          return channel->instrument_.get ();
        case zrythm::plugins::PluginSlotType::Insert:
          return channel->inserts_[slot].get ();
        default:
          break;
        }
    }
  else if (auto modulator_track = dynamic_cast<const ModulatorTrack *> (this))
    {
      if (
        slot_type == zrythm::plugins::PluginSlotType::Modulator
        && slot < (int) modulator_track->modulators_.size ())
        {
          return modulator_track->modulators_[slot].get ();
        }
    }

  return nullptr;
}

void
Track::mark_for_bounce (
  bool bounce,
  bool mark_regions,
  bool mark_children,
  bool mark_parents)
{
  if (!has_channel ())
    return;

  z_debug (
    "marking {} for bounce {}, mark regions {}", name_, bounce, mark_regions);

  bounce_ = bounce;

  if (mark_regions)
    {
      if (has_lanes ())
        {
          std::visit (
            [bounce] (auto &&laned_track) {
              using TrackT = base_type<decltype (laned_track)>;
              for (auto &lane_var : laned_track->lanes_)
                {
                  using LaneT = TrackT::TrackLaneType;
                  auto lane = std::get<LaneT *> (lane_var);
                  for (auto &region_var : lane->region_list_->regions_)
                    {
                      auto region =
                        std::get<typename LaneT::RegionTPtr> (region_var);
                      region->bounce_ = bounce;
                    }
                }
            },
            convert_to_variant<LanedTrackPtrVariant> (this));
        }

      if (auto chord_track = dynamic_cast<ChordTrack *> (this))
        for (auto &region_var : chord_track->region_list_->regions_)
          {
            auto region = std::get<ChordRegion *> (region_var);
            region->bounce_ = bounce;
          }
    }

  if (auto channel_track = dynamic_cast<ChannelTrack *> (this))
    {
      auto * direct_out = channel_track->get_channel ()->get_output_track ();
      if (direct_out && mark_parents)
        direct_out->mark_for_bounce (bounce, false, false, true);
    }

  if (mark_children)
    {
      if (auto group_target_track = dynamic_cast<GroupTargetTrack *> (this))
        {
          for (auto child_hash : group_target_track->children_)
            {
              if (auto child = TRACKLIST->find_track_by_name_hash (child_hash))
                {
                  std::visit (
                    [&] (auto &&c) {
                      c->bounce_to_master_ = bounce_to_master_;
                      c->mark_for_bounce (bounce, mark_regions, true, false);
                    },
                    *child);
                }
            }
        }
    }
}

#if 0
template <typename T>
void
Track::append_ports (
  const T*      track_var,
  std::vector<Port *> &ports,
  bool                 include_plugins)
{
  std::visit (
    [&] (auto &&track) {
      using TrackT = base_type<decltype (track)>;
      if constexpr (std::derived_from<TrackT, ChannelTrack>)
        {
          track->get_channel ()->append_ports (ports, include_plugins);
        }

      if constexpr (std::derived_from<TrackT, ProcessableTrack>)
        {
          track->processor_->append_ports (ports);
        }

      auto add_port = [&ports] (Port * port) {
        if (port)
          ports.push_back (port);
        else
          z_warning ("Port is null");
      };

      if constexpr (std::derived_from<TrackT, RecordableTrack>)
        {
          add_port (track->recording_.get ());
        }

      if constexpr (std::is_same_v<TrackT, TempoTrack>)
        {
          add_port (track->bpm_port_.get ());
          add_port (track->beats_per_bar_port_.get ());
          add_port (track->beat_unit_port_.get ());
        }
      else if constexpr (std::is_same_v<TrackT, ModulatorTrack>)
        {
          for (const auto &modulator : track->modulators_)
            modulator->append_ports (ports);
          for (const auto &macro : track->modulator_macro_processors_)
            {
              add_port (macro->macro_.get ());
              add_port (macro->cv_in_.get ());
              add_port (macro->cv_out_.get ());
            }
        }
    },
    track_var);
}
#endif

void
Track::set_enabled (
  bool enabled,
  bool trigger_undo,
  bool auto_select,
  bool fire_events)
{
  if (enabled_ == enabled)
    return;

  enabled_ = enabled;
  z_debug ("Setting track {} {}", name_, enabled_ ? "enabled" : "disabled");

  if (auto_select)
    select (true, true, fire_events);

  if (trigger_undo)
    {
      try
        {
          UNDO_MANAGER->perform (
            std::make_unique<EnableTrackAction> (*this, enabled_));
        }
      catch (const ZrythmException &e)
        {
          e.handle (QObject::tr ("Cannot set track enabled status"));
          return;
        }
    }
  else
    {
      enabled_ = enabled;

      if (fire_events)
        {
          std::visit (
            [this] (auto &&track) { Q_EMIT track->enabledChanged (enabled_); },
            convert_to_variant<TrackPtrVariant> (this));
        }
    }
}

int
Track::get_total_bars (const Transport &transport, int total_bars) const
{
  Position pos;
  pos.from_bars (total_bars);

  std::vector<ArrangerObject *> objs;
  append_objects (objs);

  for (auto obj : objs)
    {
      Position end_pos;
      if (obj->has_length ())
        {
          auto lobj = dynamic_cast<const LengthableObject *> (obj);
          lobj->get_end_pos (&end_pos);
        }
      else
        {
          obj->get_pos (&end_pos);
        }
      if (end_pos > pos)
        {
          pos = end_pos;
        }
    }

  int new_total_bars = pos.get_total_bars (transport, true);
  return std::max (new_total_bars, total_bars);
}

void
Track::create_with_action (
  Type                   type,
  const PluginSetting *  pl_setting,
  const FileDescriptor * file_descr,
  const Position *       pos,
  int                    index,
  int                    num_tracks,
  int                    disable_track_idx,
  TracksReadyCallback    ready_cb)
{
  z_return_if_fail (num_tracks > 0);

  /* only support 1 track when using files */
  z_return_if_fail (file_descr == nullptr || num_tracks == 1);

  if (file_descr)
    {
      TRACKLIST->import_files (
        nullptr, file_descr, nullptr, nullptr, index, pos, ready_cb);
    }
  else
    {
      UNDO_MANAGER->perform (std::make_unique<CreateTracksAction> (
        type, pl_setting, file_descr, index, pos, num_tracks,
        disable_track_idx));
    }

  if (ZRYTHM_TESTING)
    {
      auto tr = TRACKLIST->get_track (index);
      std::visit (
        [&] (auto &&track) {
          z_return_if_fail (track);
          z_return_if_fail (track->type_ == type);
          z_return_if_fail (track->pos_ == index);
        },
        tr);
    }
}

Track *
Track::create_empty_at_idx_with_action (Type type, int index)
{
  return create_without_file_with_action (type, nullptr, index);
}

Track *
Track::create_empty_with_action (Type type)
{
  return create_empty_at_idx_with_action (type, TRACKLIST->tracks_.size ());
}

Track *
Track::create_for_plugin_at_idx_w_action (
  Type                  type,
  const PluginSetting * pl_setting,
  int                   index)
{
  return create_without_file_with_action (type, pl_setting, index);
}

Track *
Track::create_without_file_with_action (
  Type                  type,
  const PluginSetting * pl_setting,
  int                   index)
{
  /* this may throw, and if it does we don't care - caller is expected to catch
   * it */
  create_with_action (type, pl_setting, nullptr, nullptr, index, 1, -1, nullptr);

  auto track = TRACKLIST->get_track (index);
  std::visit (
    [&] (auto &&tr) {
      z_return_if_fail (tr);
      z_return_if_fail (tr->type_ == type);
      z_return_if_fail (tr->pos_ == index);
    },
    track);
  return Track::from_variant (track);
}

void
Track::set_caches (CacheType types)
{
  if (ENUM_BITSET_TEST (CacheType, types, CacheType::TrackNameHashes))
    {
      name_hash_ = get_name_hash ();
    }

  if (
    ENUM_BITSET_TEST (CacheType, types, CacheType::PlaybackSnapshots)
    && !is_auditioner ())
    {
      z_return_if_fail (AUDIO_ENGINE->run_.load () == false);

      set_playback_caches ();
    }

  if (ENUM_BITSET_TEST (CacheType, types, CacheType::PluginPorts))
    {
      if (auto channel_track = dynamic_cast<ChannelTrack *> (this))
        {
          channel_track->get_channel ()->set_caches ();
        }
    }

  if (
    ENUM_BITSET_TEST (CacheType, types, CacheType::AutomationLaneRecordModes)
    || ENUM_BITSET_TEST (CacheType, types, CacheType::AutomationLanePorts))
    {
      if (auto automatable_track = dynamic_cast<AutomatableTrack *> (this))
        {
          automatable_track->get_automation_tracklist ().set_caches (
            CacheType::AutomationLaneRecordModes
            | CacheType::AutomationLanePorts);
        }
    }
}

#if 0
GMenu *
Track::generate_edit_context_menu (int num_selected)
{
  GMenu *     edit_submenu = g_menu_new ();
  GMenuItem * menuitem;

  if (type_is_copyable (type_))
    {
      char * str;
      /* delete track */
      if (num_selected == 1)
        str = g_strdup (QObject::tr ("_Delete Track"));
      else
        str = g_strdup (QObject::tr ("_Delete Tracks"));
      menuitem = g_menu_item_new (str, "app.delete-selected-tracks");
      g_menu_item_set_attribute (
        menuitem, "verb-icon", "s",
        "gnome-icon-library-user-trash-full-symbolic");
      g_free (str);
      g_menu_append_item (edit_submenu, menuitem);

      /* duplicate track */
      if (num_selected == 1)
        str = g_strdup (QObject::tr ("Duplicate Track"));
      else
        str = g_strdup (QObject::tr ("Duplicate Tracks"));
      menuitem = g_menu_item_new (str, "app.duplicate-selected-tracks");
      g_menu_item_set_attribute (
        menuitem, "verb-icon", "s", "gnome-icon-library-copy-symbolic");
      g_free (str);
      g_menu_append_item (edit_submenu, menuitem);
    }

    /* add regions TODO */
#  if 0
  if (track->type == Track::Type::INSTRUMENT)
    {
      menuitem = g_menu_item_new (
        _("Add Region"), "app.add-region");
      g_menu_item_set_attribute (menuitem, "verb-icon", "s", "add");
      g_menu_append_item (edit_submenu, menuitem);
    }
#  endif

  menuitem = g_menu_item_new (
    num_selected == 1 ? QObject::tr ("Hide Track") : QObject::tr ("Hide Tracks"),
    "app.hide-selected-tracks");
  g_menu_item_set_attribute (
    menuitem, "verb-icon", "s", "gnome-icon-library-eye-not-looking-symbolic");
  g_menu_append_item (edit_submenu, menuitem);

  menuitem = g_menu_item_new (
    num_selected == 1 ? QObject::tr ("Pin/Unpin Track") : QObject::tr ("Pin/Unpin Tracks"),
    "app.pin-selected-tracks");
  g_menu_item_set_attribute (
    menuitem, "verb-icon", "s", "gnome-icon-library-pin-symbolic");
  g_menu_append_item (edit_submenu, menuitem);

  menuitem = g_menu_item_new (QObject::tr ("Change Color..."), "app.change-track-color");
  g_menu_item_set_attribute (
    menuitem, "verb-icon", "s", "gnome-icon-library-color-picker-symbolic");
  g_menu_append_item (edit_submenu, menuitem);

  if (num_selected == 1)
    {
      menuitem = g_menu_item_new (QObject::tr ("Rename..."), "app.rename-track");
      g_menu_item_set_attribute (
        menuitem, "verb-icon", "s", "gnome-icon-library-text-insert-symbolic");
      g_menu_append_item (edit_submenu, menuitem);
    }

  return edit_submenu;
}
#endif

bool
Track::is_pinned () const
{
  return pos_ < TRACKLIST->pinned_tracks_cutoff_;
}

template zrythm::plugins::Plugin *
Track::insert_plugin (
  std::unique_ptr<zrythm::plugins::Plugin> &&pl,
  zrythm::plugins::PluginSlotType            slot_type,
  int                                        slot,
  bool                                       instantiate_plugin,
  bool                                       replacing_plugin,
  bool                                       moving_plugin,
  bool                                       confirm,
  bool                                       gen_automatables,
  bool                                       recalc_graph,
  bool                                       fire_events);
template zrythm::plugins::CarlaNativePlugin *
Track::insert_plugin (
  std::unique_ptr<zrythm::plugins::CarlaNativePlugin> &&pl,
  zrythm::plugins::PluginSlotType                       slot_type,
  int                                                   slot,
  bool                                                  instantiate_plugin,
  bool                                                  replacing_plugin,
  bool                                                  moving_plugin,
  bool                                                  confirm,
  bool                                                  gen_automatables,
  bool                                                  recalc_graph,
  bool                                                  fire_events);

template MidiRegion *
Track::add_region (
  MidiRegion *      region,
  AutomationTrack * at,
  int               lane_pos,
  bool              gen_name,
  bool              fire_events);
template AudioRegion *
Track::add_region (
  AudioRegion *     region,
  AutomationTrack * at,
  int               lane_pos,
  bool              gen_name,
  bool              fire_events);
template ChordRegion *
Track::add_region (
  ChordRegion *     region,
  AutomationTrack * at,
  int               lane_pos,
  bool              gen_name,
  bool              fire_events);
template AutomationRegion *
Track::add_region (
  AutomationRegion * region,
  AutomationTrack *  at,
  int                lane_pos,
  bool               gen_name,
  bool               fire_events);