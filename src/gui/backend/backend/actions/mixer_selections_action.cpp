// SPDX-FileCopyrightText: © 2019-2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "gui/backend/backend/actions/mixer_selections_action.h"
#include "gui/backend/backend/mixer_selections.h"
#include "gui/backend/backend/project.h"
#include "gui/backend/backend/settings/settings.h"
#include "gui/backend/backend/settings_manager.h"
#include "gui/backend/backend/zrythm.h"
#include "gui/backend/channel.h"
#include "gui/backend/ui.h"
#include "gui/dsp/automation_region.h"
#include "gui/dsp/carla_native_plugin.h"
#include "gui/dsp/channel_track.h"
#include "gui/dsp/modulator_track.h"
#include "gui/dsp/router.h"
#include "gui/dsp/track.h"
#include "gui/dsp/tracklist.h"
#include "utils/logger.h"

using namespace zrythm::gui::actions;

MixerSelectionsAction::MixerSelectionsAction (QObject * parent)
    : QObject (parent), UndoableAction (UndoableAction::Type::MixerSelections)
{
}

MixerSelectionsAction::MixerSelectionsAction (
  const FullMixerSelections *                    ms,
  const PortConnectionsManager *                 connections_mgr,
  Type                                           type,
  zrythm::dsp::PluginSlotType                    slot_type,
  unsigned int                                   to_track_name_hash,
  int                                            to_slot,
  const PluginSetting *                          setting,
  int                                            num_plugins,
  int                                            new_val,
  zrythm::gui::old_dsp::plugins::CarlaBridgeMode new_bridge_mode,
  QObject *                                      parent)
    : MixerSelectionsAction (parent)

{
  mixer_selections_action_type_ = type;
  slot_type_ = slot_type;
  to_slot_ = to_slot;
  to_track_name_hash_ = to_track_name_hash;
  new_channel_ = to_track_name_hash == 0;
  num_plugins_ = num_plugins;
  new_val_ = new_val;
  new_bridge_mode_ = new_bridge_mode;
  if (setting)
    {
      setting_ = setting->clone_unique ();
      setting_->validate ();
    }

  if (ms)
    {
      ms_before_ = ms->clone_unique ();
      z_return_if_fail (ms->slots_[0] == ms_before_->slots_[0]);

      /* clone the automation tracks */
      clone_ats (*ms_before_, false, 0);
    }

  if (connections_mgr)
    port_connections_before_ = connections_mgr->clone_unique ();
}

void
MixerSelectionsAction::init_loaded_impl ()
{
  if (ms_before_)
    {
      ms_before_->init_loaded ();
    }
  if (deleted_ms_)
    {
      deleted_ms_->init_loaded ();
    }

  for (auto &at : ats_)
    {
      at->init_loaded (nullptr);
    }
  for (auto &at : deleted_ats_)
    {
      at->init_loaded (nullptr);
    }
}

void
MixerSelectionsAction::init_after_cloning (const MixerSelectionsAction &other)
{
  UndoableAction::copy_members_from (other);
  mixer_selections_action_type_ = other.mixer_selections_action_type_;
  slot_type_ = other.slot_type_;
  to_slot_ = other.to_slot_;
  to_track_name_hash_ = other.to_track_name_hash_;
  new_channel_ = other.new_channel_;
  num_plugins_ = other.num_plugins_;
  new_val_ = other.new_val_;
  new_bridge_mode_ = other.new_bridge_mode_;
  if (other.setting_)
    setting_ = other.setting_->clone_unique ();
  if (other.ms_before_)
    ms_before_ = other.ms_before_->clone_unique ();
  if (other.deleted_ms_)
    deleted_ms_ = other.deleted_ms_->clone_unique ();
  clone_unique_ptr_container (deleted_ats_, other.deleted_ats_);
  clone_unique_ptr_container (ats_, other.ats_);
}

void
MixerSelectionsAction::
  clone_ats (const FullMixerSelections &ms, bool deleted, int start_slot)
{
  auto * track = std::visit (
    [&] (auto &&t) { return dynamic_cast<AutomatableTrack *> (t); },
    TRACKLIST->find_track_by_name_hash (ms.track_name_hash_).value ());
  z_return_if_fail (track);
  z_debug ("cloning automation tracks for track {}", track->name_);
  auto &atl = track->automation_tracklist_;
  int   count = 0;
  int   regions_count = 0;
  for (int slot : ms.slots_)
    {
      for (const auto &at : atl->ats_)
        {
          if (
            at->port_id_->owner_type_ != dsp::PortIdentifier::OwnerType::Plugin
            || at->port_id_->plugin_id_.slot_ != slot
            || at->port_id_->plugin_id_.slot_type_ != ms.type_)
            continue;

          if (deleted)
            {
              deleted_ats_.emplace_back (at->clone_unique ());
            }
          else
            {
              ats_.emplace_back (at->clone_unique ());
            }
          count++;
          regions_count += at->region_list_->regions_.size ();
        }
    }
  z_debug (
    "cloned %d automation tracks for track %s, total regions %d", count,
    track->name_, regions_count);
}

void
MixerSelectionsAction::copy_at_regions (
  AutomationTrack       &dest,
  const AutomationTrack &src)
{
  dest.region_list_->regions_.clear ();
  dest.region_list_->regions_.reserve (src.region_list_->regions_.size ());

  src.foreach_region ([&] (auto &src_r) {
    auto dest_region = src_r.clone_raw_ptr ();
    dest_region->set_automation_track (dest);
    dest_region->setParent (&dest);
    dest.region_list_->regions_.push_back (dest_region);
  });

  if (!dest.region_list_->regions_.empty ())
    {
      z_debug (
        "reverted {} regions for automation track {}:",
        dest.region_list_->regions_.size (), dest.index_);
      dest.port_id_->print ();
    }
}

void
MixerSelectionsAction::revert_automation (
  AutomatableTrack    &track,
  FullMixerSelections &ms,
  int                  slot,
  bool                 deleted)
{
  z_debug ("reverting automation for {}#{}", track.name_, slot);

  auto &atl = track.automation_tracklist_;
  auto &ats = deleted ? deleted_ats_ : ats_;
  int   num_reverted_ats = 0;
  int   num_reverted_regions = 0;
  for (auto &cloned_at : ats)
    {
      if (
        cloned_at->port_id_->plugin_id_.slot_ != slot
        || cloned_at->port_id_->plugin_id_.slot_type_ != ms.type_)
        {
          continue;
        }

      /* find corresponding automation track in track and copy regions */
      auto actual_at = atl->get_plugin_at (
        ms.type_, slot, cloned_at->port_id_->port_index_,
        cloned_at->port_id_->sym_);

      copy_at_regions (*actual_at, *cloned_at);
      num_reverted_regions += actual_at->region_list_->regions_.size ();
      num_reverted_ats++;
    }

  z_debug (
    "reverted %d automation tracks and %d regions", num_reverted_ats,
    num_reverted_regions);
}

void
MixerSelectionsAction::save_existing_plugin (
  FullMixerSelections *       tmp_ms,
  Track *                     from_tr,
  zrythm::dsp::PluginSlotType from_slot_type,
  int                         from_slot,
  Track *                     to_tr,
  zrythm::dsp::PluginSlotType to_slot_type,
  int                         to_slot)
{
  auto existing_pl = to_tr->get_plugin_at_slot (to_slot_type, to_slot);
  z_debug (
    "existing plugin at ({}:{}:{} => {}:{}:{}): {}",
    from_tr ? from_tr->name_ : "(none)", ENUM_NAME (from_slot_type), from_slot,
    to_tr ? to_tr->name_ : "(none)", ENUM_NAME (to_slot_type), to_slot,
    existing_pl ? existing_pl->get_name () : "(none)");
  if (
    existing_pl
    && (from_tr != to_tr || from_slot_type != to_slot_type || from_slot != to_slot))
    {
      tmp_ms->add_plugin (*to_tr, to_slot_type, to_slot);
      clone_ats (*tmp_ms, true, tmp_ms->slots_.size () - 1);
    }
  else
    {
      z_info (
        "skipping saving slot and cloning "
        "automation tracks - same slot");
    }
}

void
MixerSelectionsAction::revert_deleted_plugin (Track &to_tr, int to_slot)
{
  if (!deleted_ms_)
    {
      z_debug ("No deleted plugin to revert at {}#{}", to_tr.name_, to_slot);
      return;
    }

  z_debug ("reverting deleted plugin at {}#{}", to_tr.name_, to_slot);

  if (deleted_ms_->type_ == zrythm::dsp::PluginSlotType::Modulator)
    {
      /* modulators are never replaced */
      return;
    }

  for (size_t j = 0; j < deleted_ms_->slots_.size (); j++)
    {
      int slot_to_revert = deleted_ms_->slots_[j];
      if (slot_to_revert != to_slot)
        {
          continue;
        }

      auto &deleted_pl = deleted_ms_->plugins_[j];
      z_debug (
        "reverting plugin {} in slot {}", deleted_pl->get_name (),
        slot_to_revert);

      /* add to channel - note: cloning deleted_pl also instantiates the clone */
      auto added_pl = to_tr.insert_plugin (
        clone_unique_with_variant<zrythm::gui::old_dsp::plugins::PluginVariant> (
          deleted_pl.get ()),
        deleted_ms_->type_, slot_to_revert, true, true, false, false, true,
        false, false);

      /* bring back automation */
      z_return_if_fail (to_tr.is_automatable ());
      auto automatable_track = dynamic_cast<AutomatableTrack *> (&to_tr);
      revert_automation (*automatable_track, *deleted_ms_, slot_to_revert, true);

      /* activate and set visibility */
      added_pl->activate (true);

      /* show if was visible before */
      if (ZRYTHM_HAVE_UI && deleted_pl->visible_)
        {
          added_pl->visible_ = true;
          /* EVENTS_PUSH (EventType::ET_PLUGIN_VISIBILITY_CHANGED, added_pl); */
        }
    }
}

void
MixerSelectionsAction::do_or_undo_create_or_delete (bool do_it, bool create)
{
  AutomatableTrack * track = nullptr;
  track = std::visit (
    [&] (auto &&t) { return dynamic_cast<AutomatableTrack *> (t); },
    TRACKLIST
      ->find_track_by_name_hash (
        create ? to_track_name_hash_ : ms_before_->track_name_hash_)
      .value ());
  z_return_if_fail (track);

  auto * ch =
    track->has_channel ()
      ? dynamic_cast<ChannelTrack *> (track)->channel_
      : nullptr;
  auto &own_ms = ms_before_;
  auto  slot_type = create ? slot_type_ : own_ms->type_;
  int   loop_times =
    create && mixer_selections_action_type_ != Type::Paste
        ? num_plugins_
        : own_ms->slots_.size ();
  bool delete_ = !create;

  /* if creating plugins (create do or delete undo) */
  if ((create && do_it) || (delete_ && !do_it))
    {
      /* clear deleted caches */
      deleted_ats_.clear ();
      deleted_ms_ = std::make_unique<FullMixerSelections> ();

      for (int i = 0; i < loop_times; i++)
        {
          int slot = create ? (to_slot_ + i) : own_ms->plugins_[i]->id_.slot_;

          /* create new plugin */
          std::unique_ptr<zrythm::gui::old_dsp::plugins::Plugin> pl;
          if (create)
            {
              if (mixer_selections_action_type_ == Type::Paste)
                {
                  pl = clone_unique_with_variant<
                    zrythm::gui::old_dsp::plugins::PluginVariant> (
                    own_ms->plugins_[i].get ());
                }
              else
                {
                  pl = setting_->create_plugin (
                    to_track_name_hash_, slot_type, slot);
                }
              z_return_if_fail (pl);

              /* instantiate so that ports are created */
              pl->instantiate ();
            }
          else if (delete_)
            {
              /* note: this also instantiates the plugin */
              pl = clone_unique_with_variant<
                zrythm::gui::old_dsp::plugins::PluginVariant> (
                own_ms->plugins_[i].get ());
            }

          /* validate */
          z_return_if_fail (pl);
          if (delete_)
            {
              z_return_if_fail (slot == own_ms->slots_[i]);
            }

          /* set track */
          pl->track_ = track;
          pl->set_track_name_hash (track->get_name_hash ());

          /* save any plugin about to be deleted */
          save_existing_plugin (
            deleted_ms_.get (), nullptr, slot_type, -1,
            slot_type == zrythm::dsp::PluginSlotType::Modulator
              ? P_MODULATOR_TRACK
              : track,
            slot_type, slot);

          /* add to destination */
          auto added_pl = track->insert_plugin (
            std::move (pl), slot_type, slot, true, false, false, false, true,
            false, false);

          /* select the plugin */
          MIXER_SELECTIONS->add_slot (
            *track, slot_type, added_pl->id_.slot_, true);

          /* set visibility */
          if (create)
            {
              /* set visible from settings */
              added_pl->visible_ =
                ZRYTHM_HAVE_UI
                && gui::SettingsManager::openPluginsOnInstantiation ();
            }
          else if (delete_)
            {
              /* set visible if plugin was visible before deletion */
              added_pl->visible_ =
                ZRYTHM_HAVE_UI && own_ms->plugins_[i]->visible_;
            }
          /* EVENTS_PUSH (EventType::ET_PLUGIN_VISIBILITY_CHANGED, added_pl); */

          /* activate */
          added_pl->activate (true);
        }

      /* if undoing deletion */
      if (delete_)
        {
          for (int i = 0; i < loop_times; i++)
            {
              /* restore port connections */
              auto pl = own_ms->plugins_[i].get ();
              z_debug (
                "restoring custom connections for plugin '{}'", pl->get_name ());
              std::vector<Port *> ports;
              pl->append_ports (ports);
              for (auto port : ports)
                {
                  auto prj_port_var = PROJECT->find_port_by_id (*port->id_);
                  z_return_if_fail (prj_port_var);
                  std::visit (
                    [&] (auto &&prj_port) {
                      prj_port->restore_from_non_project (*port);
                    },
                    *prj_port_var);
                }

              /* copy automation from before deletion */
              int slot = pl->id_.slot_;
              if (track->is_automatable ())
                {
                  revert_automation (
                    dynamic_cast<AutomatableTrack &> (*track), *own_ms, slot,
                    false);
                }
            }
        }

      track->validate ();

      /* EVENTS_PUSH (EventType::ET_PLUGINS_ADDED, track); */
    }
  /* else if deleting plugins (create undo or delete do) */
  else
    {
      for (int i = 0; i < loop_times; i++)
        {
          int slot = create ? (to_slot_ + i) : own_ms->plugins_[i]->id_.slot_;

          /* if doing deletion, remember port metadata */
          if (do_it)
            {
              auto own_pl = own_ms->plugins_[i].get ();
              auto prj_pl = track->get_plugin_at_slot (slot_type, slot);

              /* remember any custom connections */
              z_debug (
                "remembering custom connections for plugin '{}'",
                own_pl->get_name ());
              std::vector<Port *> ports, own_ports;
              prj_pl->append_ports (ports);
              own_pl->append_ports (own_ports);
              for (auto prj_port : ports)
                {
                  auto it = std::find_if (
                    own_ports.begin (), own_ports.end (),
                    [&prj_port] (auto own_port) {
                      return own_port->id_ == prj_port->id_;
                    });
                  z_return_if_fail (it != own_ports.end ());

                  (*it)->copy_metadata_from_project (*prj_port);
                }
            }

          /* remove the plugin at given slot */
          track->remove_plugin (
            slot_type, slot, false, false, true, false, false);

          /* if there was a plugin at the slot before, bring it back */
          revert_deleted_plugin (*track, slot);
        }

      /* EVENTS_PUSH (EventType::ET_PLUGINS_REMOVED, nullptr); */
    }

  /* restore connections */
  save_or_load_port_connections (do_it);

  ROUTER->recalc_graph (false);

  if (ch)
    {
      /* EVENTS_PUSH (EventType::ET_CHANNEL_SLOTS_CHANGED, ch); */
    }
}

void
MixerSelectionsAction::do_or_undo_change_status (bool do_it)
{
  auto ms = ms_before_.get ();
  auto * track = std::visit (
    [&] (auto &&t) { return dynamic_cast<Track *> (t); },
    TRACKLIST->find_track_by_name_hash (ms_before_->track_name_hash_).value ());

  auto ch =
    track->has_channel ()
      ? dynamic_cast<ChannelTrack *> (track)->channel_
      : nullptr;

  for (size_t i = 0; i < ms->slots_.size (); i++)
    {
      auto own_pl = ms->plugins_[i].get ();
      auto pl = zrythm::gui::old_dsp::plugins::Plugin::find (own_pl->id_);
      pl->set_enabled (
        do_it ? new_val_ : own_pl->is_enabled (false),
        i == ms->slots_.size () - 1);
    }

  if (ch)
    {
      /* EVENTS_PUSH (EventType::ET_CHANNEL_SLOTS_CHANGED, ch); */
    }
}

void
MixerSelectionsAction::do_or_undo_change_load_behavior (bool do_it)
{
  auto ms = ms_before_.get ();
  auto track = std::visit (
    [&] (auto &&t) { return dynamic_cast<Track *> (t); },
    TRACKLIST->find_track_by_name_hash (ms_before_->track_name_hash_).value ());
  auto ch =
    track->has_channel ()
      ? dynamic_cast<ChannelTrack *> (track)->channel_
      : nullptr;

  for (size_t i = 0; i < ms->slots_.size (); i++)
    {
      auto own_pl = ms->plugins_[i].get ();
      auto pl = zrythm::gui::old_dsp::plugins::Plugin::find (own_pl->id_);
      pl->setting_->bridge_mode_ =
        do_it ? new_bridge_mode_ : own_pl->setting_->bridge_mode_;

      /* TODO - below is tricky */
#if 0
      carla_set_engine_option (
        pl->carla->host_handle, ENGINE_OPTION_PREFER_UI_BRIDGES, false, nullptr);
      carla_set_engine_option (
        pl->carla->host_handle, ENGINE_OPTION_PREFER_PLUGIN_BRIDGES, false, nullptr);

      switch (pl->setting_->bridge_mode_)
        {
        case zrythm::gui::old_dsp::plugins::CarlaBridgeMode::Full:
          carla_set_engine_option (
            pl->carla->host_handle, ENGINE_OPTION_PREFER_PLUGIN_BRIDGES, true, nullptr);
          break;
        case zrythm::gui::old_dsp::plugins::CarlaBridgeMode::UI:
          carla_set_engine_option (
            pl->carla->host_handle, ENGINE_OPTION_PREFER_UI_BRIDGES, true, nullptr);
          break;
        default:
          break;
        }

      pl->activate (false);
      carla_remove_plugin (
        pl->carla->host_handle, 0);
      carla_native_plugin_add_internal_plugin_from_descr (
        pl->carla, pl->setting_->descr);
      pl->activate (true);
#endif
    }

  if (ZRYTHM_HAVE_UI)
    {
// TODO
#if 0
      ui_show_error_message (
        QObject::tr ("Project Reload Needed"),
        QObject::tr (
          "Plugin load behavior changes will only take effect after you save and re-load the project"));
#endif
    }

  if (ch)
    {
      /* EVENTS_PUSH (EventType::ET_CHANNEL_SLOTS_CHANGED, ch); */
    }
}

void
MixerSelectionsAction::copy_automation_from_track1_to_track2 (
  const AutomatableTrack     &from_track,
  AutomatableTrack           &to_track,
  zrythm::dsp::PluginSlotType slot_type,
  int                         from_slot,
  int                         to_slot)
{
  auto &prev_atl = from_track.get_automation_tracklist ();
  for (auto &prev_at : prev_atl.ats_)
    {
      if (
        prev_at->region_list_->regions_.empty ()
        || prev_at->port_id_->owner_type_ != dsp::PortIdentifier::OwnerType::Plugin
        || prev_at->port_id_->plugin_id_.slot_ != from_slot
        || prev_at->port_id_->plugin_id_.slot_type_ != slot_type)
        {
          continue;
        }

      /* find the corresponding at in the new track */
      auto &atl = to_track.get_automation_tracklist ();
      for (auto &at : atl.ats_)
        {
          if (
            at->port_id_->owner_type_ != dsp::PortIdentifier::OwnerType::Plugin
            || at->port_id_->plugin_id_.slot_ != to_slot
            || at->port_id_->plugin_id_.slot_type_ != slot_type
            || at->port_id_->port_index_ != prev_at->port_id_->port_index_)
            {
              continue;
            }

          /* copy the automation regions */
          prev_at->foreach_region ([&] (auto &prev_region) {
            to_track.add_region (
              prev_region.clone_raw_ptr (), at, -1, false, false);
          });
          break;
        }
    }
}

void
MixerSelectionsAction::do_or_undo_move_or_copy (bool do_it, bool copy)
{
  auto                            own_ms = ms_before_.get ();
  zrythm::dsp::PluginSlotType     from_slot_type = own_ms->type_;
  zrythm::dsp::PluginSlotType     to_slot_type = slot_type_;
  auto *                          from_tr = std::visit (
    [&] (auto &&t) { return dynamic_cast<AutomatableTrack *> (t); },
    TRACKLIST->find_track_by_name_hash (own_ms->track_name_hash_).value ());
  z_return_if_fail (from_tr);
  bool move = !copy;

  if (do_it)
    {
      AutomatableTrack * to_tr = nullptr;

      if (new_channel_)
        {
          /* get the own plugin */
          auto own_pl = own_ms->plugins_[0].get ();

          /* add a new track to the tracklist */
          std::string str = fmt::format ("{} (Copy)", own_pl->get_name ());
          to_tr = dynamic_cast<AutomatableTrack *> (TRACKLIST->append_track (
            Track::create_track (
              Track::Type::AudioBus, str, TRACKLIST->tracks_.size ()),
            *AUDIO_ENGINE, false, false));

          /* remember to track pos */
          to_track_name_hash_ = to_tr->get_name_hash ();
        }
      /* else if not new track/channel */
      else
        {
          to_tr = std::visit (
            [&] (auto &&t) { return dynamic_cast<AutomatableTrack *> (t); },
            TRACKLIST->find_track_by_name_hash (to_track_name_hash_).value ());
        }

      [[maybe_unused]] auto to_ch =
        to_tr->has_channel ()
          ? dynamic_cast<ChannelTrack *> (to_tr)->channel_
          : nullptr;

      MIXER_SELECTIONS->clear (false);

      /* sort own selections */
      own_ms->sort ();

      bool move_downwards_same_track =
        to_tr == from_tr && !own_ms->slots_.empty ()
        && to_slot_ > own_ms->plugins_[0]->id_.slot_;

      /* clear deleted caches */
      deleted_ats_.clear ();
      deleted_ms_ = std::make_unique<FullMixerSelections> ();

      /* foreach slot */
      for (
        int i = move_downwards_same_track ? own_ms->slots_.size () - 1 : 0;
        move_downwards_same_track ? (i >= 0) : (i < (int) own_ms->slots_.size ());
        move_downwards_same_track ? --i : ++i)
        {
          /* get/create the actual plugin */
          int from_slot = own_ms->plugins_[i]->id_.slot_;
          std::unique_ptr<zrythm::gui::old_dsp::plugins::Plugin> new_pl;
          zrythm::gui::old_dsp::plugins::Plugin *                pl = nullptr;
          if (move)
            {
              pl = from_tr->get_plugin_at_slot (own_ms->type_, from_slot);
              z_return_if_fail (
                new_pl->id_.track_name_hash_ == from_tr->get_name_hash ());
            }
          else
            {
              new_pl = clone_unique_with_variant<
                zrythm::gui::old_dsp::plugins::PluginVariant> (
                own_ms->plugins_[i].get ());
            }

          int to_slot = to_slot_ + i;

          /* save any plugin about to be deleted */
          save_existing_plugin (
            deleted_ms_.get (), from_tr, from_slot_type, from_slot, to_tr,
            to_slot_type, to_slot);

          /* move or copy the plugin */
          if (move)
            {
              z_debug (
                "moving plugin from "
                "{}:{}:{} to {}:{}:{}",
                from_tr->name_, ENUM_NAME (from_slot_type), from_slot,
                to_tr->name_, ENUM_NAME (to_slot_type), to_slot);

              if (
                from_tr != to_tr || from_slot_type != to_slot_type
                || from_slot != to_slot)
                {
                  pl->move (to_tr, to_slot_type, to_slot, false, false);
                }
            }
          else if (copy)
            {
              z_debug (
                "copying plugin from "
                "{}:{}:{} to {}:{}:{}",
                from_tr->name_, ENUM_NAME (from_slot_type), from_slot,
                to_tr->name_, ENUM_NAME (to_slot_type), to_slot);

              pl = to_tr->insert_plugin (
                std::move (new_pl), to_slot_type, to_slot, true, false, false,
                false, true, false, false);

              z_return_if_fail (
                pl->in_ports_.size () == own_ms->plugins_[i]->in_ports_.size ());
            }

          /* copy automation regions from original plugin */
          if (copy && to_tr->is_automatable ())
            {
              copy_automation_from_track1_to_track2 (
                dynamic_cast<AutomatableTrack &> (*from_tr),
                dynamic_cast<AutomatableTrack &> (*to_tr), to_slot_type,
                own_ms->slots_[i], to_slot);
            }

          /* select it */
          MIXER_SELECTIONS->add_slot (*to_tr, to_slot_type, to_slot, true);

          /* if new plugin (copy), instantiate it, activate it and set
           * visibility */
          if (copy)
            {
              pl->activate (true);

              /* show if was visible before */
              if (ZRYTHM_HAVE_UI && own_ms->plugins_[i]->visible_)
                {
                  pl->visible_ = true;
                  /* EVENTS_PUSH (EventType::ET_PLUGIN_VISIBILITY_CHANGED, pl); */
                }
            }
        }

      to_tr->validate ();

      if (new_channel_)
        {
          /* EVENTS_PUSH (EventType::ET_TRACKS_ADDED, nullptr); */
        }

      /* EVENTS_PUSH (EventType::ET_CHANNEL_SLOTS_CHANGED, to_ch); */
    }
  /* else if undoing (deleting/moving plugins back) */
  else
    {
      Track * to_tr = std::visit (
        [&] (auto &&t) { return dynamic_cast<Track *> (t); },
        TRACKLIST->find_track_by_name_hash (to_track_name_hash_).value ());
      [[maybe_unused]] auto* to_ch =
        to_tr->has_channel ()
          ? dynamic_cast<ChannelTrack *> (to_tr)->channel_
          : nullptr;
      z_return_if_fail (to_tr);

      /* clear selections to readd each original plugin */
      MIXER_SELECTIONS->clear (false);

      /* sort own selections */
      own_ms->sort ();

      bool move_downwards_same_track =
        to_tr == from_tr && !own_ms->slots_.empty ()
        && to_slot_ < own_ms->plugins_[0]->id_.slot_;
      for (
        int i = move_downwards_same_track ? own_ms->slots_.size () - 1 : 0;
        move_downwards_same_track ? (i >= 0) : (i < (int) own_ms->slots_.size ());
        move_downwards_same_track ? i-- : i++)
        {
          /* get the actual plugin */
          int                       to_slot = to_slot_ + i;
          zrythm::gui::old_dsp::plugins::Plugin * pl =
            to_tr->get_plugin_at_slot (to_slot_type, to_slot);
          z_return_if_fail (pl);

          /* original slot */
          int from_slot = own_ms->plugins_[i]->id_.slot_;

          /* if moving plugins back */
          if (move)
            {
              /* move plugin to its original
               * slot */
              z_debug (
                "moving plugin back from "
                "{}:{}:{} to {}:{}:{}",
                to_tr->name_, ENUM_NAME (to_slot_type), to_slot, from_tr->name_,
                ENUM_NAME (from_slot_type), from_slot);

              if (
                from_tr != to_tr || from_slot_type != to_slot_type
                || from_slot != to_slot)
                {
                  {
                    auto existing_pl =
                      from_tr->get_plugin_at_slot (from_slot_type, from_slot);
                    z_return_if_fail (!existing_pl);
                  }
                  pl->move (from_tr, from_slot_type, from_slot, false, false);
                }
            }
          else if (copy)
            {
              to_tr->remove_plugin (
                to_slot_type, to_slot, false, false, true, false, false);
              pl = nullptr;
            }

          /* if there was a plugin at the slot before, bring it back */
          revert_deleted_plugin (*to_tr, to_slot);

          if (copy)
            {
              pl = from_tr->get_plugin_at_slot (from_slot_type, from_slot);
            }

          /* add orig plugin to mixer selections */
          z_return_if_fail (pl);
          MIXER_SELECTIONS->add_slot (
            *from_tr, from_slot_type, from_slot, false);
        }

      /* if a new track was created delete it */
      if (new_channel_)
        {
          TRACKLIST->remove_track (*to_tr, true, true, true, false);
        }

      from_tr->validate ();

      /* EVENTS_PUSH (EventType::ET_CHANNEL_SLOTS_CHANGED, to_ch); */
    }

  /* restore connections */
  save_or_load_port_connections (do_it);

  ROUTER->recalc_graph (false);
}

void
MixerSelectionsAction::do_or_undo (bool do_it)
{
  switch (mixer_selections_action_type_)
    {
    case Type::Create:
      do_or_undo_create_or_delete (do_it, true);
      break;
    case Type::Delete:
      do_or_undo_create_or_delete (do_it, false);
      break;
    case Type::Move:
      do_or_undo_move_or_copy (do_it, false);
      break;
    case Type::Copy:
      do_or_undo_move_or_copy (do_it, true);
      break;
    case Type::Paste:
      do_or_undo_create_or_delete (do_it, true);
      break;
    case Type::ChangeStatus:
      do_or_undo_change_status (do_it);
      break;
    case Type::ChangeLoadBehavior:
      do_or_undo_change_load_behavior (do_it);
      break;
    default:
      z_warn_if_reached ();
    }

  /* if first do and keeping track of connections, clone the new connections */
  if (do_it && port_connections_before_ && !port_connections_after_)
    port_connections_after_ = PORT_CONNECTIONS_MGR->clone_unique ();
}

void
MixerSelectionsAction::perform_impl ()
{
  do_or_undo (true);
}

void
MixerSelectionsAction::undo_impl ()
{
  do_or_undo (false);
}

QString
MixerSelectionsAction::to_string () const
{
  switch (mixer_selections_action_type_)
    {
    case Type::Create:
      if (num_plugins_ == 1)
        {
          return format_qstr (QObject::tr ("Create {}"), setting_->get_name ());
        }
      else
        {
          return format_qstr (
            QObject::tr ("Create {} {}s"), num_plugins_, setting_->get_name ());
        }
    case Type::Delete:
      if (ms_before_->slots_.size () == 1)
        {
          return QObject::tr ("Delete Plugin");
        }
      else
        {
          return format_qstr (
            QObject::tr ("Delete {} Plugins"), ms_before_->slots_.size ());
        }
    case Type::Move:
      if (ms_before_->slots_.size () == 1)
        {
          return format_qstr (
            QObject::tr ("Move {}"), ms_before_->plugins_[0]->get_name ());
        }
      else
        {
          return format_qstr (
            QObject::tr ("Move {} Plugins"), ms_before_->slots_.size ());
        }
    case Type::Copy:
      if (ms_before_->slots_.size () == 1)
        {
          return format_qstr (
            QObject::tr ("Copy {}"), ms_before_->plugins_[0]->get_name ());
        }
      else
        {
          return format_qstr (
            QObject::tr ("Copy {} Plugins"), ms_before_->slots_.size ());
        }
    case Type::Paste:
      if (ms_before_->slots_.size () == 1)
        {
          return format_qstr (
            QObject::tr ("Paste {}"), ms_before_->plugins_[0]->get_name ());
        }
      else
        {
          return format_qstr (
            QObject::tr ("Paste {} Plugins"), ms_before_->slots_.size ());
        }
    case Type::ChangeStatus:
      if (ms_before_->slots_.size () == 1)
        {
          return format_qstr (
            QObject::tr ("Change Status for {}"),
            ms_before_->plugins_[0]->get_name ());
        }
      else
        {
          return format_qstr (
            QObject::tr ("Change Status for {} Plugins"),
            ms_before_->slots_.size ());
        }
    case Type::ChangeLoadBehavior:
      return format_qstr (
        QObject::tr ("Change Load Behavior for {}"),
        ms_before_->plugins_[0]->get_name ());
    }

  return {};
}
