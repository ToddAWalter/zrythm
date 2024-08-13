// SPDX-FileCopyrightText: © 2020-2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "zrythm-test-config.h"

#include "actions/arranger_selections.h"
#include "actions/mixer_selections_action.h"
#include "actions/port_connection_action.h"
#include "actions/tracklist_selections.h"
#include "actions/undo_manager.h"
#include "dsp/control_port.h"
#include "dsp/port_identifier.h"
#include "plugins/carla_discovery.h"
#include "plugins/plugin_manager.h"
#include "project.h"
#include "utils/flags.h"
#include "zrythm.h"

#include "tests/helpers/plugin_manager.h"
#include "tests/helpers/project_helper.h"

static int num_master_children = 0;

static void
_test_copy_plugins (
  const char * pl_bundle,
  const char * pl_uri,
  bool         is_instrument,
  bool         with_carla)
{
  std::this_thread::sleep_for (std::chrono::microseconds (100));

  /* create the plugin track */
  test_plugin_manager_create_tracks_from_plugin (
    pl_bundle, pl_uri, is_instrument, with_carla, 1);

  num_master_children++;
  REQUIRE_SIZE_EQ (P_MASTER_TRACK->children_, num_master_children);
  auto track = TRACKLIST->get_last_track ();
  REQUIRE_EQ (
    P_MASTER_TRACK->children_[num_master_children - 1], track->get_name_hash ());
  track = TRACKLIST->get_track (5);
  REQUIRE_EQ (P_MASTER_TRACK->children_[0], track->get_name_hash ());

  /* save and reload the project */
  test_project_save_and_reload ();

  REQUIRE_SIZE_EQ (P_MASTER_TRACK->children_, num_master_children);
  track = TRACKLIST->get_last_track ();
  REQUIRE_EQ (
    P_MASTER_TRACK->children_[num_master_children - 1], track->get_name_hash ());
  track = TRACKLIST->get_track (5);
  REQUIRE_EQ (P_MASTER_TRACK->children_[0], track->get_name_hash ());

  /* select track */
  auto selected_track = TRACKLIST->get_last_track ();
  track->select (F_SELECT, F_EXCLUSIVE, F_NO_PUBLISH_EVENTS);

  UNDO_MANAGER->perform (std::make_unique<CopyTracksAction> (
    *TRACKLIST_SELECTIONS->gen_tracklist_selections (), *PORT_CONNECTIONS_MGR,
    TRACKLIST->get_num_tracks ()));
  num_master_children++;
  REQUIRE_SIZE_EQ (P_MASTER_TRACK->children_, num_master_children);
  track = TRACKLIST->get_last_track ();
  REQUIRE_EQ (
    P_MASTER_TRACK->children_[num_master_children - 1], track->get_name_hash ());
  track = TRACKLIST->get_track (5);
  REQUIRE_EQ (P_MASTER_TRACK->children_[0], track->get_name_hash ());
  track = TRACKLIST->get_track (6);
  REQUIRE_EQ (P_MASTER_TRACK->children_[1], track->get_name_hash ());
  auto new_track = TRACKLIST->get_last_track ();

  /* if instrument, copy tracks, otherwise copy
   * plugins */
  if (is_instrument)
    {
#if 0
      if (!with_carla)
        {
          g_assert_true (
            new_track->channel->instrument->lv2->ui);
        }
      ua =
        mixer_selections_action_new_create (
          PluginSlotType::Insert,
          new_track->pos, 0, descr, 1);
      undo_manager_perform (UNDO_MANAGER, ua);

      mixer_selections_clear (
        MIXER_SELECTIONS, F_NO_PUBLISH_EVENTS);
      mixer_selections_add_slot (
        MIXER_SELECTIONS, new_track,
        PluginSlotType::Insert, 0, F_NO_CLONE);
      ua =
        mixer_selections_action_new_copy (
          MIXER_SELECTIONS, PluginSlotType::Insert,
          -1, 0);
      undo_manager_perform (UNDO_MANAGER, ua);
      UNDO_MANAGER->undo();
      UNDO_MANAGER->redo();
      UNDO_MANAGER->undo();
#endif
    }
  else
    {
      MIXER_SELECTIONS->clear (false);
      MIXER_SELECTIONS->add_slot (
        *selected_track, PluginSlotType::Insert, 0, F_NO_PUBLISH_EVENTS);
      UNDO_MANAGER->perform (std::make_unique<MixerSelectionsCopyAction> (
        *MIXER_SELECTIONS->gen_full_from_this (), *PORT_CONNECTIONS_MGR,
        PluginSlotType::Insert, new_track, 1));
    }

  std::this_thread::sleep_for (std::chrono::microseconds (100));
}

TEST_CASE ("copy plugins")
{
  test_helper_zrythm_init ();

  _test_copy_plugins (TRIPLE_SYNTH_BUNDLE, TRIPLE_SYNTH_URI, true, false);
#ifdef HAVE_CARLA
  _test_copy_plugins (TRIPLE_SYNTH_BUNDLE, TRIPLE_SYNTH_URI, true, true);
#endif /* HAVE_CARLA */
#ifdef HAVE_NO_DELAY_LINE
  _test_copy_plugins (NO_DELAY_LINE_BUNDLE, NO_DELAY_LINE_URI, false, false);
#  ifdef HAVE_CARLA
  _test_copy_plugins (NO_DELAY_LINE_BUNDLE, NO_DELAY_LINE_URI, false, true);
#  endif /* HAVE_CARLA */
#endif   /* HAVE_NO_DELAY_LINE */

  test_helper_zrythm_cleanup ();
}

TEST_CASE ("MIDI FX slot deletion")
{
  test_helper_zrythm_init ();

  /* create MIDI track */
  Track::create_empty_with_action<MidiTrack> ();

#ifdef HAVE_MIDI_CC_MAP
  /* add plugin to slot */
  int             slot = 0;
  auto            setting = test_plugin_manager_get_plugin_setting (
    MIDI_CC_MAP_BUNDLE, MIDI_CC_MAP_URI, false);
  auto track_pos = TRACKLIST->get_last_pos ();
  auto track = TRACKLIST->get_track<ChannelTrack> (track_pos);
  REQUIRE_NOTHROW (
    UNDO_MANAGER->perform (std::make_unique<MixerSelectionsCreateAction> (
      PluginSlotType::MidiFx, *track, slot, setting)));

  auto pl = track->channel_->midi_fx_[slot].get ();

  /* set the value to check if it is brought
   * back on undo */
  auto port = pl->get_port_by_symbol<ControlPort> ("ccin");
  port->set_control_value (120.f, F_NOT_NORMALIZED, false);

  /* delete slot */
  pl->select (F_SELECT, F_EXCLUSIVE);
  UNDO_MANAGER->perform (std::make_unique<MixerSelectionsDeleteAction> (
    *MIXER_SELECTIONS->gen_full_from_this (), *PORT_CONNECTIONS_MGR));

  /* undo and check port value is restored */
  UNDO_MANAGER->undo ();
  pl = track->channel_->midi_fx_[slot].get ();
  port = pl->get_port_by_symbol<ControlPort> ("ccin");
  REQUIRE_FLOAT_NEAR (port->control_, 120.f, 0.0001f);

  UNDO_MANAGER->redo ();
#endif

  test_helper_zrythm_cleanup ();
}

static void
_test_create_plugins (
  PluginProtocol prot,
  const char *   pl_bundle,
  const char *   pl_uri,
  bool           is_instrument,
  bool           with_carla)
{
  std::optional<PluginSetting> setting;

#ifdef HAVE_SHERLOCK_ATOM_INSPECTOR
  if (string_is_equal (pl_uri, SHERLOCK_ATOM_INSPECTOR_URI))
    {
      /* expect messages */
      for (int i = 0; i < 7; i++)
        {
          g_test_expect_message (
            G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "*Failed from water*");
        }
      z_return_if_reached ();
    }
#endif

  switch (prot)
    {
    case PluginProtocol::LV2:
    case PluginProtocol::VST:
      setting =
        test_plugin_manager_get_plugin_setting (pl_bundle, pl_uri, with_carla);
      break;
    default:
      break;
    }
  REQUIRE (setting);

  if (is_instrument)
    {
      /* create an instrument track from helm */
      Track::create_with_action (
        Track::Type::Instrument, &(*setting), nullptr, nullptr,
        TRACKLIST->get_num_tracks (), 1, -1, nullptr);
    }
  else
    {
      /* create an audio fx track and add the plugin */
      auto track = Track::create_empty_with_action<AudioBusTrack> ();
      UNDO_MANAGER->perform (std::make_unique<MixerSelectionsCreateAction> (
        PluginSlotType::Insert, *track, 0, *setting));
    }

  setting.reset ();

  /* let the engine run */
  std::this_thread::sleep_for (std::chrono::seconds (1));

  test_project_save_and_reload ();

  auto src_track_pos = TRACKLIST->get_last_pos ();
  auto src_track = TRACKLIST->get_track<ChannelTrack> (src_track_pos);

  if (is_instrument)
    {
      REQUIRE_NONNULL (src_track->channel_->instrument_);
    }
  else
    {
      REQUIRE_NONNULL (src_track->channel_->inserts_[0]);
    }

  /* duplicate the track */
  src_track->select (F_SELECT, true, F_NO_PUBLISH_EVENTS);
  REQUIRE (src_track->validate ());
  UNDO_MANAGER->perform (std::make_unique<CopyTracksAction> (
    *TRACKLIST_SELECTIONS->gen_tracklist_selections (), *PORT_CONNECTIONS_MGR,
    TRACKLIST->get_num_tracks ()));

  auto dest_track_pos = TRACKLIST->get_last_pos ();
  auto dest_track = TRACKLIST->get_track<ChannelTrack> (dest_track_pos);

  REQUIRE (src_track->validate ());
  REQUIRE (dest_track->validate ());

  UNDO_MANAGER->undo ();
  UNDO_MANAGER->undo ();
  UNDO_MANAGER->redo ();
  UNDO_MANAGER->redo ();

  /* let the engine run */
  std::this_thread::sleep_for (std::chrono::seconds (1));

  test_project_save_and_reload ();

#ifdef HAVE_SHERLOCK_ATOM_INSPECTOR
  if (string_is_equal (pl_uri, SHERLOCK_ATOM_INSPECTOR_URI))
    {
      /* assert expected messages */
      g_test_assert_expected_messages ();
      z_return_if_reached ();

      /* remove to prevent further warnings */
      UNDO_MANAGER->undo ();
      UNDO_MANAGER->undo ();
    }
#endif

  z_info ("done");
}

static void
test_create_plugins (void)
{
  test_helper_zrythm_init ();

  /* only run with carla */
  for (int i = 1; i < 2; i++)
    {
      if (i == 1)
        {
#ifdef HAVE_CARLA
#  ifdef HAVE_NOIZEMAKER
          _test_create_plugins (
            PluginProtocol::VST, NOIZEMAKER_PATH, nullptr, true, i);
#  endif
#else
          break;
#endif
        }

#ifdef HAVE_SHERLOCK_ATOM_INSPECTOR
#  if 0
      /* need to refactor the error handling code because this is expected to fail */
      _test_create_plugins (
        PluginProtocol::LV2, SHERLOCK_ATOM_INSPECTOR_BUNDLE,
        SHERLOCK_ATOM_INSPECTOR_URI, false, i);
#  endif
#endif
      _test_create_plugins (
        PluginProtocol::LV2, TRIPLE_SYNTH_BUNDLE, TRIPLE_SYNTH_URI, true, i);
#ifdef HAVE_LSP_COMPRESSOR
      _test_create_plugins (
        PluginProtocol::LV2, LSP_COMPRESSOR_BUNDLE, LSP_COMPRESSOR_URI, false,
        i);
#endif
#ifdef HAVE_CARLA_RACK
      _test_create_plugins (
        PluginProtocol::LV2, CARLA_RACK_BUNDLE, CARLA_RACK_URI, true, i);
#endif
#if defined(HAVE_UNLIMITED_MEM) && defined(HAVE_CALF_COMPRESSOR)
      _test_create_plugins (
        PluginProtocol::LV2, CALF_COMPRESSOR_BUNDLE, CALF_COMPRESSOR_URI, true,
        i);
#endif
    }

  test_helper_zrythm_cleanup ();
}

#ifdef HAVE_LSP_COMPRESSOR
static void
_test_port_and_plugin_track_pos_after_move (
  const char * pl_bundle,
  const char * pl_uri,
  bool         with_carla)
{
  auto setting =
    test_plugin_manager_get_plugin_setting (pl_bundle, pl_uri, with_carla);

  /* create an instrument track from helm */
  Track::create_with_action (
    Track::Type::AudioBus, &setting, nullptr, nullptr,
    TRACKLIST->get_num_tracks (), 1, -1, nullptr);

  int src_track_pos = TRACKLIST->get_last_pos ();
  int dest_track_pos = src_track_pos + 1;

  /* select it */
  auto src_track = TRACKLIST->get_track<AutomatableTrack> (src_track_pos);
  src_track->select (F_SELECT, true, F_NO_PUBLISH_EVENTS);

  /* get an automation track */
  auto       &atl = src_track->get_automation_tracklist ();
  const auto &at = atl.ats_.back ();
  at->created_ = true;
  atl.set_at_visible (*at, true);

  /* create an automation region */
  Position start_pos, end_pos;
  start_pos.set_to_bar (2);
  end_pos.set_to_bar (4);
  auto region = std::make_shared<AutomationRegion> (
    start_pos, end_pos, src_track->get_name_hash (), at->index_,
    at->regions_.size ());
  src_track->add_region (region, at.get (), -1, F_GEN_NAME, F_NO_PUBLISH_EVENTS);
  region->select (true, false, F_NO_PUBLISH_EVENTS);
  UNDO_MANAGER->perform (
    std::make_unique<ArrangerSelectionsAction::CreateAction> (*TL_SELECTIONS));

  /* create some automation points */
  auto port = Port::find_from_identifier<ControlPort> (at->port_id_);
  start_pos.set_to_bar (1);
  auto ap = std::make_shared<AutomationPoint> (
    port->deff_, port->real_val_to_normalized (port->deff_), &start_pos);
  region->append_object (ap);
  ap->select (true, false, F_NO_PUBLISH_EVENTS);
  UNDO_MANAGER->perform (std::make_unique<ArrangerSelectionsAction::CreateAction> (
    *AUTOMATION_SELECTIONS));

  /* duplicate it */
  REQUIRE (src_track->validate ());
  UNDO_MANAGER->perform (std::make_unique<CopyTracksAction> (
    *TRACKLIST_SELECTIONS->gen_tracklist_selections (), *PORT_CONNECTIONS_MGR,
    TRACKLIST->get_num_tracks ()));

  auto dest_track = TRACKLIST->get_track (dest_track_pos);

  REQUIRE (src_track->validate ());
  REQUIRE (dest_track->validate ());

  /* move plugin from 1st track to 2nd track and undo/redo */
  MIXER_SELECTIONS->clear ();
  MIXER_SELECTIONS->add_slot (
    *src_track, PluginSlotType::Insert, 0, F_NO_PUBLISH_EVENTS);
  UNDO_MANAGER->perform (std::make_unique<MixerSelectionsMoveAction> (
    *MIXER_SELECTIONS->gen_full_from_this (), *PORT_CONNECTIONS_MGR,
    PluginSlotType::Insert, dest_track, 1));

  /* let the engine run */
  std::this_thread::sleep_for (std::chrono::seconds (1));

  REQUIRE (src_track->validate ());
  REQUIRE (dest_track->validate ());

  UNDO_MANAGER->undo ();

  REQUIRE (src_track->validate ());
  REQUIRE (dest_track->validate ());

  UNDO_MANAGER->redo ();

  REQUIRE (src_track->validate ());
  REQUIRE (dest_track->validate ());

  UNDO_MANAGER->undo ();

  /* move plugin from 1st slot to the 2nd slot and undo/redo */
  MIXER_SELECTIONS->clear ();
  MIXER_SELECTIONS->add_slot (
    *src_track, PluginSlotType::Insert, 0, F_NO_PUBLISH_EVENTS);
  UNDO_MANAGER->perform (std::make_unique<MixerSelectionsMoveAction> (
    *MIXER_SELECTIONS->gen_full_from_this (), *PORT_CONNECTIONS_MGR,
    PluginSlotType::Insert, src_track, 1));
  UNDO_MANAGER->undo ();
  UNDO_MANAGER->redo ();

  /* let the engine run */
  std::this_thread::sleep_for (std::chrono::seconds (1));

  /* move the plugin to a new track */
  MIXER_SELECTIONS->clear ();
  src_track = TRACKLIST->get_track<AutomatableTrack> (src_track_pos);
  MIXER_SELECTIONS->add_slot (
    *src_track, PluginSlotType::Insert, 1, F_NO_PUBLISH_EVENTS);
  UNDO_MANAGER->perform (std::make_unique<MixerSelectionsMoveAction> (
    *MIXER_SELECTIONS->gen_full_from_this (), *PORT_CONNECTIONS_MGR,
    PluginSlotType::Insert, nullptr, 0));
  UNDO_MANAGER->undo ();
  UNDO_MANAGER->redo ();

  /* let the engine run */
  std::this_thread::sleep_for (std::chrono::seconds (1));

  /* go back to the start */
  UNDO_MANAGER->undo ();
  UNDO_MANAGER->undo ();
  UNDO_MANAGER->undo ();
  UNDO_MANAGER->undo ();
  UNDO_MANAGER->undo ();
}
#endif

static void
test_port_and_plugin_track_pos_after_move (void)
{
  return;
  test_helper_zrythm_init ();

#ifdef HAVE_LSP_COMPRESSOR
  _test_port_and_plugin_track_pos_after_move (
    LSP_COMPRESSOR_BUNDLE, LSP_COMPRESSOR_URI, false);
#endif

  test_helper_zrythm_cleanup ();
}

#ifdef HAVE_CARLA
static void
test_port_and_plugin_track_pos_after_move_with_carla (void)
{
  return;
  test_helper_zrythm_init ();

#  ifdef HAVE_LSP_COMPRESSOR
  _test_port_and_plugin_track_pos_after_move (
    LSP_COMPRESSOR_BUNDLE, LSP_COMPRESSOR_URI, true);
#  endif

  test_helper_zrythm_cleanup ();
}
#endif

TEST_CASE ("move two plugins one slot up")
{
  test_helper_zrythm_init ();

#ifdef HAVE_LSP_COMPRESSOR

  /* create a track with an insert */
  auto setting = test_plugin_manager_get_plugin_setting (
    LSP_COMPRESSOR_BUNDLE, LSP_COMPRESSOR_URI, false);
  Track::create_for_plugin_at_idx_w_action (
    Track::Type::AudioBus, &setting, TRACKLIST->get_num_tracks ());
  UNDO_MANAGER->undo ();
  UNDO_MANAGER->redo ();

  int track_pos = TRACKLIST->get_last_pos ();

  auto get_track_and_validate = [&] (bool validate = true) {
    auto t = TRACKLIST->get_track<ChannelTrack> (track_pos);
    REQUIRE_NONNULL (t);
    if (validate)
      {
        REQUIRE (t->validate ());
      }
    return t;
  };

  /* select it */
  auto track = get_track_and_validate ();
  track->select (F_SELECT, true, F_NO_PUBLISH_EVENTS);

  /* save and reload the project */
  test_project_save_and_reload ();
  track = get_track_and_validate ();

  {
    /* get an automation track */
    auto       &atl = track->get_automation_tracklist ();
    const auto &at = atl.ats_.back ();
    at->created_ = true;
    atl.set_at_visible (*at, true);

    /* create an automation region */
    Position start_pos, end_pos;
    start_pos.set_to_bar (2);
    end_pos.set_to_bar (4);
    auto region = std::make_shared<AutomationRegion> (
      start_pos, end_pos, track->get_name_hash (), at->index_,
      at->regions_.size ());
    track->add_region (region, at.get (), -1, F_GEN_NAME, F_NO_PUBLISH_EVENTS);
    region->select (true, false, F_NO_PUBLISH_EVENTS);
    UNDO_MANAGER->perform (
      std::make_unique<ArrangerSelectionsAction::CreateAction> (*TL_SELECTIONS));
    UNDO_MANAGER->undo ();
    UNDO_MANAGER->redo ();
  }

  /* save and reload the project */
  test_project_save_and_reload ();
  track = get_track_and_validate ();
  auto &atl = track->get_automation_tracklist ();
  ;
  const auto &at = atl.ats_.back ();

  /* create some automation points */
  auto     port = Port::find_from_identifier<ControlPort> (at->port_id_);
  Position start_pos;
  start_pos.set_to_bar (1);
  REQUIRE_NONEMPTY (at->regions_);
  const auto &region = at->regions_.front ();
  auto        ap = std::make_shared<AutomationPoint> (
    port->deff_, port->real_val_to_normalized (port->deff_), start_pos);
  region->append_object (ap);
  ap->select (true, false, F_NO_PUBLISH_EVENTS);
  UNDO_MANAGER->perform (std::make_unique<ArrangerSelectionsAction::CreateAction> (
    *AUTOMATION_SELECTIONS));
  UNDO_MANAGER->undo ();
  UNDO_MANAGER->redo ();

  /* save and reload the project */
  test_project_save_and_reload ();
  track = get_track_and_validate ();

  /* duplicate the plugin to the 2nd slot */
  MIXER_SELECTIONS->clear ();
  MIXER_SELECTIONS->add_slot (
    *track, PluginSlotType::Insert, 0, F_NO_PUBLISH_EVENTS);
  UNDO_MANAGER->perform (std::make_unique<MixerSelectionsCopyAction> (
    *MIXER_SELECTIONS->gen_full_from_this (), *PORT_CONNECTIONS_MGR,
    PluginSlotType::Insert, track, 1));
  UNDO_MANAGER->undo ();
  UNDO_MANAGER->redo ();

  /* at this point we have a plugin at slot#0 and
   * its clone at slot#1 */

  /* remove slot #0 and undo */
  MIXER_SELECTIONS->clear ();
  MIXER_SELECTIONS->add_slot (
    *track, PluginSlotType::Insert, 0, F_NO_PUBLISH_EVENTS);
  UNDO_MANAGER->perform (std::make_unique<MixerSelectionsDeleteAction> (
    *MIXER_SELECTIONS->gen_full_from_this (), *PORT_CONNECTIONS_MGR));
  UNDO_MANAGER->undo ();
  UNDO_MANAGER->redo ();
  UNDO_MANAGER->undo ();

  /* save and reload the project */
  test_project_save_and_reload ();
  track = get_track_and_validate ();

  /* move the 2 plugins to start at slot#1 (2nd
   * slot) */
  MIXER_SELECTIONS->clear ();
  MIXER_SELECTIONS->add_slot (
    *track, PluginSlotType::Insert, 0, F_NO_PUBLISH_EVENTS);
  MIXER_SELECTIONS->add_slot (
    *track, PluginSlotType::Insert, 1, F_NO_PUBLISH_EVENTS);
  UNDO_MANAGER->perform (std::make_unique<MixerSelectionsMoveAction> (
    *MIXER_SELECTIONS->gen_full_from_this (), *PORT_CONNECTIONS_MGR,
    PluginSlotType::Insert, track, 1));
  REQUIRE (track->validate ());
  UNDO_MANAGER->undo ();
  REQUIRE (track->validate ());
  UNDO_MANAGER->redo ();
  REQUIRE (track->validate ());

  /* save and reload the project */
  test_project_save_and_reload ();
  track = get_track_and_validate ();

  /* move the 2 plugins to start at slot 2 (3rd
   * slot) */
  MIXER_SELECTIONS->clear ();
  MIXER_SELECTIONS->add_slot (
    *track, PluginSlotType::Insert, 1, F_NO_PUBLISH_EVENTS);
  MIXER_SELECTIONS->add_slot (
    *track, PluginSlotType::Insert, 2, F_NO_PUBLISH_EVENTS);
  UNDO_MANAGER->perform (std::make_unique<MixerSelectionsMoveAction> (
    *MIXER_SELECTIONS->gen_full_from_this (), *PORT_CONNECTIONS_MGR,
    PluginSlotType::Insert, track, 2));
  REQUIRE (track->validate ());
  UNDO_MANAGER->undo ();
  REQUIRE (track->validate ());
  UNDO_MANAGER->redo ();
  REQUIRE (track->validate ());

  /* save and reload the project */
  test_project_save_and_reload ();
  track = get_track_and_validate ();

  /* move the 2 plugins to start at slot 1 (2nd
   * slot) */
  MIXER_SELECTIONS->clear ();
  MIXER_SELECTIONS->add_slot (
    *track, PluginSlotType::Insert, 2, F_NO_PUBLISH_EVENTS);
  MIXER_SELECTIONS->add_slot (
    *track, PluginSlotType::Insert, 3, F_NO_PUBLISH_EVENTS);
  UNDO_MANAGER->perform (std::make_unique<MixerSelectionsMoveAction> (
    *MIXER_SELECTIONS->gen_full_from_this (), *PORT_CONNECTIONS_MGR,
    PluginSlotType::Insert, track, 1));
  REQUIRE (track->validate ());
  UNDO_MANAGER->undo ();
  REQUIRE (track->validate ());
  UNDO_MANAGER->redo ();
  REQUIRE (track->validate ());

  /* save and reload the project */
  test_project_save_and_reload ();
  track = get_track_and_validate ();

  /* move the 2 plugins to start back at slot 0 (1st
   * slot) */
  MIXER_SELECTIONS->clear ();
  MIXER_SELECTIONS->add_slot (
    *track, PluginSlotType::Insert, 2, F_NO_PUBLISH_EVENTS);
  MIXER_SELECTIONS->add_slot (
    *track, PluginSlotType::Insert, 1, F_NO_PUBLISH_EVENTS);
  UNDO_MANAGER->perform (std::make_unique<MixerSelectionsMoveAction> (
    *MIXER_SELECTIONS->gen_full_from_this (), *PORT_CONNECTIONS_MGR,
    PluginSlotType::Insert, track, 0));
  UNDO_MANAGER->undo ();
  UNDO_MANAGER->redo ();

  REQUIRE (track->validate ());
  REQUIRE_NONNULL (track->channel_->inserts_[0]);
  REQUIRE_NONNULL (track->channel_->inserts_[1]);

  /* move 2nd plugin to 1st plugin (replacing it) */
  MIXER_SELECTIONS->clear ();
  MIXER_SELECTIONS->add_slot (
    *track, PluginSlotType::Insert, 1, F_NO_PUBLISH_EVENTS);
  UNDO_MANAGER->perform (std::make_unique<MixerSelectionsMoveAction> (
    *MIXER_SELECTIONS->gen_full_from_this (), *PORT_CONNECTIONS_MGR,
    PluginSlotType::Insert, track, 0));

  /* verify that first plugin was replaced by 2nd
   * plugin */
  REQUIRE_NONNULL (track->channel_->inserts_[0]);
  REQUIRE_NONNULL (track->channel_->inserts_[1]);

  /* undo and verify that both plugins are back */
  UNDO_MANAGER->undo ();
  REQUIRE_NONNULL (track->channel_->inserts_[0]);
  REQUIRE_NONNULL (track->channel_->inserts_[1]);
  UNDO_MANAGER->redo ();
  UNDO_MANAGER->undo ();
  REQUIRE_NONNULL (track->channel_->inserts_[0]);
  REQUIRE_EQ (
    track->channel_->inserts_[0]->setting_.descr_.uri_, LSP_COMPRESSOR_URI);
  REQUIRE_NONNULL (track->channel_->inserts_[1]);

  test_project_save_and_reload ();
  track = get_track_and_validate ();

  /* TODO verify that custom connections are back */

#  ifdef HAVE_MIDI_CC_MAP
  /* add plugin to slot 0 (replacing current) */
  setting = test_plugin_manager_get_plugin_setting (
    MIDI_CC_MAP_BUNDLE, MIDI_CC_MAP_URI, false);
  UNDO_MANAGER->perform (std::make_unique<MixerSelectionsCreateAction> (
    PluginSlotType::Insert, *track, 0, setting, 1));

  /* undo and verify that the original plugin is
   * back */
  UNDO_MANAGER->undo ();
  REQUIRE_NONNULL (track->channel_->inserts_.at (0));
  REQUIRE_EQ (
    track->channel_->inserts_[0]->setting_.descr_.uri_, LSP_COMPRESSOR_URI);
  REQUIRE_NONNULL (track->channel_->inserts_.at (1));

  /* redo */
  UNDO_MANAGER->redo ();
  REQUIRE_NONNULL (track->channel_->inserts_.at (0));
  REQUIRE_EQ (
    track->channel_->inserts_[0]->setting_.descr_.uri_, setting.descr_.uri_);
  REQUIRE_NONNULL (track->channel_->inserts_.at (1));

  auto pl = track->channel_->inserts_[0].get ();

  /* set the value to check if it is brought back on undo */
  port = pl->get_port_by_symbol<ControlPort> ("ccin");
  port->set_control_value (120.f, F_NOT_NORMALIZED, true);

  REQUIRE_FLOAT_NEAR (port->control_, 120.f, 0.0001f);

  /* move 2nd plugin to 1st plugin (replacing it) */
  MIXER_SELECTIONS->clear ();
  MIXER_SELECTIONS->add_slot (
    *track, PluginSlotType::Insert, 1, F_NO_PUBLISH_EVENTS);
  UNDO_MANAGER->perform (std::make_unique<MixerSelectionsMoveAction> (
    *MIXER_SELECTIONS->gen_full_from_this (), *PORT_CONNECTIONS_MGR,
    PluginSlotType::Insert, track, 0));

  test_project_save_and_reload ();
  track = get_track_and_validate ();

  REQUIRE_NONNULL (track->channel_->inserts_.at (0));
  REQUIRE_NULL (track->channel_->inserts_[1]);

  /* undo and check plugin and port value are restored */
  UNDO_MANAGER->undo ();
  pl = track->channel_->inserts_[0].get ();
  REQUIRE_EQ (pl->setting_.descr_.uri_, setting.descr_.uri_);
  port = pl->get_port_by_symbol<ControlPort> ("ccin");
  REQUIRE_FLOAT_NEAR (port->control_, 120.f, 0.0001f);

  REQUIRE_NONNULL (track->channel_->inserts_[0]);
  REQUIRE_NONNULL (track->channel_->inserts_[1]);

  test_project_save_and_reload ();
  track = get_track_and_validate ();

  UNDO_MANAGER->redo ();
#  endif // HAVE_MIDI_CC_MAP

  REQUIRE (track->validate ());

  /* let the engine run */
  std::this_thread::sleep_for (std::chrono::seconds (1));

  test_project_save_and_reload ();

  UNDO_MANAGER->undo ();
  UNDO_MANAGER->redo ();

  UNDO_MANAGER->undo ();
  UNDO_MANAGER->undo ();
  UNDO_MANAGER->undo ();
  UNDO_MANAGER->undo ();
#endif // HAVE_LSP_COMPRESSOR

  test_helper_zrythm_cleanup ();
}

TEST_CASE ("create modulator")
{
  test_helper_zrythm_init ();

#ifdef HAVE_AMS_LFO
#  ifdef HAVE_CARLA
  /* create a track with an insert */
  auto setting =
    test_plugin_manager_get_plugin_setting (AMS_LFO_BUNDLE, AMS_LFO_URI, false);
  UNDO_MANAGER->perform (std::make_unique<MixerSelectionsCreateAction> (
    PluginSlotType::Modulator, *P_MODULATOR_TRACK,
    P_MODULATOR_TRACK->modulators_.size (), setting, 1));
  UNDO_MANAGER->undo ();
  UNDO_MANAGER->redo ();
  UNDO_MANAGER->undo ();
  UNDO_MANAGER->redo ();

  /* create another one */
  UNDO_MANAGER->perform (std::make_unique<MixerSelectionsCreateAction> (
    PluginSlotType::Modulator, *P_MODULATOR_TRACK,
    P_MODULATOR_TRACK->modulators_.size (), setting, 1));

  /* connect a cv output from the first modulator to a control of the 2nd */
  auto p1 =
    P_MODULATOR_TRACK->modulators_[P_MODULATOR_TRACK->modulators_.size () - 2]
      .get ();
  auto     p2 = P_MODULATOR_TRACK->modulators_.back ().get ();
  CVPort * cv_out = NULL;
  for (auto p : p1->out_ports_ | type_is<CVPort> ())
    {
      cv_out = p;
    }
  auto ctrl_in = p2->get_port_by_symbol<ControlPort> ("freq");
  REQUIRE_NONNULL (cv_out);
  REQUIRE_NONNULL (ctrl_in);
  PortIdentifier cv_out_id = cv_out->id_;
  PortIdentifier ctrl_in_id = ctrl_in->id_;

  /* connect the ports */
  UNDO_MANAGER->perform (
    std::make_unique<PortConnectionConnectAction> (cv_out->id_, ctrl_in->id_));
  UNDO_MANAGER->undo ();
  UNDO_MANAGER->redo ();

  auto sel = std::make_unique<FullMixerSelections> ();
  sel->add_slot (
    *P_MODULATOR_TRACK, PluginSlotType::Modulator,
    P_MODULATOR_TRACK->modulators_.size () - 2, F_NO_PUBLISH_EVENTS);
  UNDO_MANAGER->perform (std::make_unique<MixerSelectionsDeleteAction> (
    *sel, *PORT_CONNECTIONS_MGR));
  UNDO_MANAGER->undo ();

  /* verify port connection is back */
  cv_out = Port::find_from_identifier<CVPort> (cv_out_id);
  ctrl_in = Port::find_from_identifier<ControlPort> (ctrl_in_id);
  REQUIRE (cv_out->is_connected_to (*ctrl_in));

  UNDO_MANAGER->redo ();

#  endif /* HAVE_CARLA */
#endif   /* HAVE_AMS_LFO */

  test_helper_zrythm_cleanup ();
}

TEST_CASE ("move plugin after duplicating track")
{
  test_helper_zrythm_init ();

#if defined(HAVE_LSP_SIDECHAIN_COMPRESSOR)

  test_plugin_manager_create_tracks_from_plugin (
    LSP_SIDECHAIN_COMPRESSOR_BUNDLE, LSP_SIDECHAIN_COMPRESSOR_URI, false, false,
    1);
  test_plugin_manager_create_tracks_from_plugin (
    TRIPLE_SYNTH_BUNDLE, TRIPLE_SYNTH_URI, true, false, 1);

  auto ins_track =
    TRACKLIST->get_track<InstrumentTrack> (TRACKLIST->get_last_pos ());
  auto lsp_track =
    TRACKLIST->get_track<AudioBusTrack> (TRACKLIST->get_last_pos () - 1);
  REQUIRE_NONNULL (ins_track);
  REQUIRE_NONNULL (lsp_track);
  auto lsp = lsp_track->channel_->inserts_[0].get ();

  AudioPort * sidechain_port = nullptr;
  for (auto port : lsp->in_ports_ | type_is<AudioPort> ())
    {
      if (
        ENUM_BITSET_TEST (
          PortIdentifier::Flags, port->id_.flags_,
          PortIdentifier::Flags::Sidechain))
        {
          sidechain_port = port;
          break;
        }
    }
  REQUIRE_NONNULL (sidechain_port);

  /* create sidechain connection from instrument track to lsp plugin in lsp
   * track */
  UNDO_MANAGER->perform (std::make_unique<PortConnectionConnectAction> (
    ins_track->channel_->fader_->stereo_out_->get_l ().id_,
    sidechain_port->id_));

  /* duplicate instrument track */
  ins_track->select (F_SELECT, F_EXCLUSIVE, F_NO_PUBLISH_EVENTS);
  UNDO_MANAGER->perform (std::make_unique<CopyTracksAction> (
    *TRACKLIST_SELECTIONS->gen_tracklist_selections (), *PORT_CONNECTIONS_MGR,
    TRACKLIST->get_num_tracks ()));

  auto dest_track = TRACKLIST->get_last_track ();

  /* move lsp plugin to newly created track */
  MIXER_SELECTIONS->clear ();
  MIXER_SELECTIONS->add_slot (
    *lsp_track, PluginSlotType::Insert, 0, F_NO_PUBLISH_EVENTS);
  UNDO_MANAGER->perform (std::make_unique<MixerSelectionsMoveAction> (
    *MIXER_SELECTIONS->gen_full_from_this (), *PORT_CONNECTIONS_MGR,
    PluginSlotType::Insert, dest_track, 1));

#endif

  test_helper_zrythm_cleanup ();
}

TEST_CASE ("move plugin from inserts to midi fx")
{
#ifdef HAVE_MIDI_CC_MAP
  test_helper_zrythm_init ();

  /* create a track with an insert */
  auto track = Track::create_empty_with_action<MidiTrack> ();
  int  track_pos = TRACKLIST->get_last_pos ();
  auto setting = test_plugin_manager_get_plugin_setting (
    MIDI_CC_MAP_BUNDLE, MIDI_CC_MAP_URI, false);
  UNDO_MANAGER->perform (std::make_unique<MixerSelectionsCreateAction> (
    PluginSlotType::Insert, *track, 0, setting, 1));

  /* select it */
  track->select (F_SELECT, true, F_NO_PUBLISH_EVENTS);

  /* move to midi fx */
  MIXER_SELECTIONS->clear ();
  MIXER_SELECTIONS->add_slot (
    *track, PluginSlotType::Insert, 0, F_NO_PUBLISH_EVENTS);
  UNDO_MANAGER->perform (std::make_unique<MixerSelectionsMoveAction> (
    *MIXER_SELECTIONS->gen_full_from_this (), *PORT_CONNECTIONS_MGR,
    PluginSlotType::MidiFx, track, 0));
  REQUIRE_NONNULL (track->channel_->midi_fx_[0]);
  REQUIRE (track->validate ());
  UNDO_MANAGER->undo ();
  REQUIRE (track->validate ());
  UNDO_MANAGER->redo ();
  REQUIRE (track->validate ());
  REQUIRE_NONNULL (track->channel_->midi_fx_[0]);

  /* save and reload the project */
  test_project_save_and_reload ();
  track = TRACKLIST->get_track<MidiTrack> (track_pos);
  REQUIRE (track->validate ());

  test_helper_zrythm_cleanup ();
#endif
}

TEST_CASE ("undo deletion of multiple inserts")
{
  test_helper_zrythm_init ();

  test_plugin_manager_create_tracks_from_plugin (
    TRIPLE_SYNTH_BUNDLE, TRIPLE_SYNTH_URI, true, false, 1);

  Track * ins_track = TRACKLIST->tracks[TRACKLIST->tracks.size () - 1];

  /* add 2 inserts */
  int             slot = 0;
  PluginSetting * setting = test_plugin_manager_get_plugin_setting (
    COMPRESSOR_BUNDLE, COMPRESSOR_URI, false);
  bool ret = mixer_selections_action_perform_create (
    PluginSlotType::Insert, track_get_name_hash (*ins_track), slot, setting, 1,
    nullptr);
  g_assert_true (ret);

  slot = 1;
  setting = test_plugin_manager_get_plugin_setting (
    CUBIC_DISTORTION_BUNDLE, CUBIC_DISTORTION_URI, false);
  ret = mixer_selections_action_perform_create (
    PluginSlotType::Insert, track_get_name_hash (*ins_track), slot, setting, 1,
    nullptr);
  g_assert_true (ret);

  Plugin * compressor = ins_track->channel->inserts[0];
  Plugin * no_delay_line = ins_track->channel->inserts[1];
  g_assert_true (IS_PLUGIN_AND_NONNULL (compressor));
  g_assert_true (IS_PLUGIN_AND_NONNULL (no_delay_line));
  plugin_select (compressor, F_SELECT, F_EXCLUSIVE);
  plugin_select (no_delay_line, F_SELECT, F_NOT_EXCLUSIVE);

  /* delete inserts */
  ret = mixer_selections_action_perform_delete (
    MIXER_SELECTIONS, PORT_CONNECTIONS_MGR, nullptr);
  g_assert_true (ret);

  /* undo deletion */
  UNDO_MANAGER->undo ();

  test_helper_zrythm_cleanup ();
}

static void
_test_replace_instrument (
  PluginProtocol prot,
  const char *   pl_bundle,
  const char *   pl_uri,
  bool           with_carla)
{
#ifdef HAVE_LSP_COMPRESSOR
  PluginSetting * setting = NULL;

  switch (prot)
    {
    case PluginProtocol::LV2:
    case PluginProtocol::VST:
      setting =
        test_plugin_manager_get_plugin_setting (pl_bundle, pl_uri, with_carla);
      z_return_if_fail (setting);
      setting = plugin_setting_clone (setting, F_NO_VALIDATE);
      break;
    default:
      break;
    }
  z_return_if_fail (setting);

  /* create an fx track from a plugin */
  test_plugin_manager_create_tracks_from_plugin (
    LSP_SIDECHAIN_COMPRESSOR_BUNDLE, LSP_SIDECHAIN_COMPRESSOR_URI, false, false,
    1);

  /* create an instrument track */
  Track::create_for_plugin_at_idx_w_action (
    Track::Type::Instrument, setting, TRACKLIST->get_num_tracks ());
  int  src_track_pos = TRACKLIST->get_last_pos ();
  auto src_track = TRACKLIST->get_track<InstrumentTrack> (src_track_pos);
  REQUIRE (src_track->validate ());

  /* let the engine run */
  std::this_thread::sleep_for (std::chrono::seconds (1));

  test_project_save_and_reload ();

  src_track = TRACKLIST->tracks[src_track_pos];
  g_assert_true (IS_PLUGIN_AND_NONNULL (src_track->channel->instrument));

  /* create a port connection */
  int num_port_connection_actions =
    UNDO_MANAGER->undo_stack->num_port_connection_actions;
  int            lsp_track_pos = TRACKLIST->tracks.size () - 2;
  Track *        lsp_track = TRACKLIST->tracks[lsp_track_pos];
  Plugin *       lsp = lsp_track->channel->inserts[0];
  Port *         sidechain_port = NULL;
  PortIdentifier sidechain_port_id = PortIdentifier ();
  for (int i = 0; i < lsp->num_in_ports; i++)
    {
      Port * port = lsp->in_ports[i];
      if (
        ENUM_BITSET_TEST (
          PortIdentifier::Flags, port->id_.flags_,
          PortIdentifier::Flags::SIDECHAIN))
        {
          sidechain_port = port;
          sidechain_port_id = port->id_;
          break;
        }
    }
  z_return_if_fail (IS_PORT_AND_NONNULL (sidechain_port));

  /*#if 0*/
  PortIdentifier helm_l_out_port_id = PortIdentifier ();
  helm_l_out_port_id = src_track->channel->instrument->l_out->id_;
  port_connection_action_perform_connect (
    &src_track->channel->instrument->l_out->id_, &sidechain_port->id_, nullptr);
  g_assert_cmpint (sidechain_port->srcs_.size (), ==, 1);
  g_assert_true (
    helm_l_out_port_id.sym_
    == UNDO_MANAGER->undo_stack
         ->port_connection_actions[num_port_connection_actions]
         ->connection->src_id->sym_);
  /*#endif*/

  /*test_project_save_and_reload ();*/
  src_track = TRACKLIST->tracks[src_track_pos];

  /* get an automation track */
  AutomationTracklist * atl = track_get_automation_tracklist (src_track);
  z_return_if_fail (atl);
  AutomationTrack * at = atl->ats[atl->num_ats - 1];
  g_assert_true (at->port_id.owner_type_ == PortIdentifier::OwnerType::Plugin);
  at->created = true;
  automation_tracklist_set_at_visible (atl, at, true);

  /* create an automation region */
  Position start_pos, end_pos;
  start_pos.set_to_bar (2);
  end_pos.set_to_bar (4);
  Region * region = automation_region_new (
    &start_pos, &end_pos, track_get_name_hash (*src_track), at->index,
    at->num_regions);
  bool success = track_add_region (
    src_track, region, at, -1, F_GEN_NAME, F_NO_PUBLISH_EVENTS, nullptr);
  g_assert_true (success);
  (ArrangerObject *) region->select (true, false, F_NO_PUBLISH_EVENTS);
  arranger_selections_action_perform_create (TL_SELECTIONS, nullptr);
  int num_regions = automation_tracklist_get_num_regions (atl);
  g_assert_cmpint (num_regions, ==, 1);

  /* create some automation points */
  Port * port = Port::find_from_identifier (&at->port_id);
  start_pos.set_to_bar (1);
  AutomationPoint * ap = automation_point_new_float (
    port->deff_, control_port_real_val_to_normalized (port, port->deff_),
    &start_pos);
  automation_region_add_ap (region, ap, F_NO_PUBLISH_EVENTS);
  (ArrangerObject *) ap->select (true, false, F_NO_PUBLISH_EVENTS);
  arranger_selections_action_perform_create (AUTOMATION_SELECTIONS, nullptr);
  num_regions = automation_tracklist_get_num_regions (atl);
  g_assert_cmpint (num_regions, ==, 1);

  int num_ats = atl->num_ats;

  /* replace the instrument with a new instance */
  mixer_selections_action_perform_create (
    PluginSlotType::Instrument, track_get_name_hash (*src_track), -1, setting,
    1, nullptr);
  g_assert_true (track_validate (src_track));

  src_track = TRACKLIST->tracks[src_track_pos];
  atl = track_get_automation_tracklist (src_track);
  z_return_if_fail (atl);

  /* verify automation is gone */
  num_regions = automation_tracklist_get_num_regions (atl);
  g_assert_cmpint (num_regions, ==, 0);

  UNDO_MANAGER->undo ();

  /* verify automation is back */
  atl = track_get_automation_tracklist (src_track);
  z_return_if_fail (atl);
  num_regions = automation_tracklist_get_num_regions (atl);
  g_assert_cmpint (num_regions, ==, 1);

  UNDO_MANAGER->redo ();

  /* verify automation is gone */
  num_regions = automation_tracklist_get_num_regions (atl);
  g_assert_cmpint (num_regions, ==, 0);

  g_assert_cmpint (num_ats, ==, atl->num_ats);
  g_assert_cmpint (sidechain_port->srcs_.size (), ==, 0);
  g_assert_true (
    helm_l_out_port_id.sym_
    == UNDO_MANAGER->undo_stack
         ->port_connection_actions[num_port_connection_actions]
         ->connection->src_id->sym_);

  /* test undo and redo */
  g_assert_true (IS_PLUGIN_AND_NONNULL (src_track->channel->instrument));
  UNDO_MANAGER->undo ();
  g_assert_true (IS_PLUGIN_AND_NONNULL (src_track->channel->instrument));

  /* verify automation is back */
  src_track = TRACKLIST->tracks[src_track_pos];
  atl = track_get_automation_tracklist (src_track);
  z_return_if_fail (atl);
  num_regions = automation_tracklist_get_num_regions (atl);
  g_assert_cmpint (num_regions, ==, 1);

  UNDO_MANAGER->redo ();
  g_assert_true (IS_PLUGIN_AND_NONNULL (src_track->channel->instrument));

  /* let the engine run */
  std::this_thread::sleep_for (std::chrono::seconds (1));

  /*yaml_set_log_level (CYAML_LOG_INFO);*/
  test_project_save_and_reload ();
  /*yaml_set_log_level (CYAML_LOG_WARNING);*/

  src_track = TRACKLIST->tracks[src_track_pos];
  atl = track_get_automation_tracklist (src_track);
  z_return_if_fail (atl);
  num_regions = automation_tracklist_get_num_regions (atl);
  g_assert_cmpint (num_regions, ==, 0);

  UNDO_MANAGER->undo ();

  /* verify automation is back */
  atl = track_get_automation_tracklist (src_track);
  z_return_if_fail (atl);
  num_regions = automation_tracklist_get_num_regions (atl);
  g_assert_cmpint (num_regions, ==, 1);

  UNDO_MANAGER->undo ();
  UNDO_MANAGER->redo ();
  UNDO_MANAGER->redo ();

  /* duplicate the track */
  src_track = TRACKLIST->tracks[src_track_pos];
  track_select (src_track, F_SELECT, true, F_NO_PUBLISH_EVENTS);
  g_assert_true (track_validate (src_track));
  tracklist_selections_action_perform_copy (
    TRACKLIST_SELECTIONS, PORT_CONNECTIONS_MGR, TRACKLIST->tracks.size (),
    nullptr);

  int     dest_track_pos = TRACKLIST->tracks.size () - 1;
  Track * dest_track = TRACKLIST->tracks[dest_track_pos];

  g_assert_true (track_validate (src_track));
  g_assert_true (track_validate (dest_track));

  UNDO_MANAGER->undo ();
  UNDO_MANAGER->undo ();
  UNDO_MANAGER->undo ();
  UNDO_MANAGER->undo ();
  UNDO_MANAGER->undo ();
  /*#if 0*/
  UNDO_MANAGER->undo ();
  UNDO_MANAGER->redo ();
  /*#endif*/
  UNDO_MANAGER->redo ();
  UNDO_MANAGER->redo ();
  UNDO_MANAGER->redo ();
  UNDO_MANAGER->redo ();
  UNDO_MANAGER->redo ();

  z_info ("letting engine run...");

  /* let the engine run */
  std::this_thread::sleep_for (std::chrono::seconds (1));

  test_project_save_and_reload ();

#endif /* HAVE LSP_COMPRESSOR */
}

static void
test_replace_instrument (void)
{
  test_helper_zrythm_init ();

  for (int i = 0; i < 2; i++)
    {
      if (i == 1)
        {
#ifdef HAVE_CARLA
#  ifdef HAVE_NOIZEMAKER
          _test_replace_instrument (
            PluginProtocol::VST, NOIZEMAKER_PATH, nullptr, i);
#  endif
#else
          break;
#endif
        }

      _test_replace_instrument (
        PluginProtocol::LV2, TRIPLE_SYNTH_BUNDLE, TRIPLE_SYNTH_URI, i);
#ifdef HAVE_CARLA_RACK
      _test_replace_instrument (
        PluginProtocol::LV2, CARLA_RACK_BUNDLE, CARLA_RACK_URI, i);
#endif
    }

  test_helper_zrythm_cleanup ();
}

TEST_CASE ("save modulators")
{
  test_helper_zrythm_init ();

#if defined(HAVE_CARLA) && defined(HAVE_GEONKICK)
  PluginSetting * setting = test_plugin_manager_get_plugin_setting (
    GEONKICK_BUNDLE, GEONKICK_URI, false);
  z_return_if_fail (setting);
  bool ret = mixer_selections_action_perform_create (
    PluginSlotType::Modulator, track_get_name_hash (*P_MODULATOR_TRACK),
    P_MODULATOR_TRACK->num_modulators, setting, 1, nullptr);
  g_assert_true (ret);
  plugin_setting_free (setting);

  test_project_save_and_reload ();
#endif

  test_helper_zrythm_cleanup ();
}

int
main (int argc, char * argv[])
{
  g_test_init (&argc, &argv, nullptr);

#define TEST_PREFIX "/actions/mixer_selections_action/"

  g_test_add_func (
    TEST_PREFIX "test move two plugins one slot up",
    (GTestFunc) test_move_two_plugins_one_slot_up);
  g_test_add_func (
    TEST_PREFIX "test create modulator", (GTestFunc) test_create_modulator);
  g_test_add_func (
    TEST_PREFIX "test MIDI fx slot deletion",
    (GTestFunc) test_midi_fx_slot_deletion);
  g_test_add_func (
    TEST_PREFIX "test port and plugin track pos after move",
    (GTestFunc) test_port_and_plugin_track_pos_after_move);
#ifdef HAVE_CARLA
  g_test_add_func (
    TEST_PREFIX "test port and plugin track pos after move with carla",
    (GTestFunc) test_port_and_plugin_track_pos_after_move_with_carla);
#endif
  g_test_add_func (
    TEST_PREFIX "test undoing deletion of multiple inserts",
    (GTestFunc) test_undoing_deletion_of_multiple_inserts);
  g_test_add_func (
    TEST_PREFIX "test save modulators", (GTestFunc) test_save_modulators);
  g_test_add_func (
    TEST_PREFIX "test create plugins", (GTestFunc) test_create_plugins);
#if 0
  /* needs to know if port is sidechain, not
   * implemented in carla yet */
  g_test_add_func (
    TEST_PREFIX
    "test move pl after duplicating track",
    (GTestFunc)
    test_move_pl_after_duplicating_track);
  g_test_add_func (
    TEST_PREFIX "test replace instrument",
    (GTestFunc) test_replace_instrument);
#endif
  g_test_add_func (
    TEST_PREFIX "test copy plugins", (GTestFunc) test_copy_plugins);
  g_test_add_func (
    TEST_PREFIX "test move plugin from inserts to midi fx",
    (GTestFunc) test_move_plugin_from_inserts_to_midi_fx);

  (void) test_move_pl_after_duplicating_track;
  (void) test_replace_instrument;

  return g_test_run ();
}
