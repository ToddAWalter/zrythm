/*
 * Copyright (C) 2020-2021 Alexandros Theodotou <alex at zrythm dot org>
 *
 * This file is part of Zrythm
 *
 * Zrythm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version approved by
 * Alexandros Theodotou in a public statement of acceptance.
 *
 * Zrythm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Zrythm.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "zrythm-test-config.h"

#include "actions/mixer_selections_action.h"
#include "actions/undoable_action.h"
#include "actions/undo_manager.h"
#include "audio/control_port.h"
#include "plugins/plugin_manager.h"
#include "plugins/carla/carla_discovery.h"
#include "project.h"
#include "utils/flags.h"
#include "zrythm.h"

#include "tests/helpers/plugin_manager.h"
#include "tests/helpers/project.h"

#include <glib.h>

static int num_master_children = 0;

static void
_test_copy_plugins (
  const char * pl_bundle,
  const char * pl_uri,
  bool         is_instrument,
  bool         with_carla)
{
  g_usleep (100);

  /* create the plugin track */
  test_plugin_manager_create_tracks_from_plugin (
    pl_bundle, pl_uri, is_instrument, with_carla, 1);

  num_master_children++;
  g_assert_cmpint (
    P_MASTER_TRACK->num_children, ==,
    num_master_children);
  g_assert_cmpint (
    P_MASTER_TRACK->children[
      num_master_children - 1], ==,
    TRACKLIST->num_tracks - 1);
  g_assert_cmpint (
    P_MASTER_TRACK->children[0], ==, 5);

  /* save and reload the project */
  test_project_save_and_reload ();

  g_assert_cmpint (
    P_MASTER_TRACK->num_children, ==,
    num_master_children);
  g_assert_cmpint (
    P_MASTER_TRACK->children[
      num_master_children - 1], ==,
    TRACKLIST->num_tracks - 1);
  g_assert_cmpint (
    P_MASTER_TRACK->children[0], ==, 5);

  /* select track */
  Track * track =
    TRACKLIST->tracks[TRACKLIST->num_tracks - 1];
  track_select (
    track, F_SELECT, F_EXCLUSIVE,
    F_NO_PUBLISH_EVENTS);

  UndoableAction * ua =
    tracklist_selections_action_new_copy (
      TRACKLIST_SELECTIONS, TRACKLIST->num_tracks);
  undo_manager_perform (UNDO_MANAGER, ua);
  num_master_children++;
  g_assert_cmpint (
    P_MASTER_TRACK->num_children, ==,
    num_master_children);
  g_assert_cmpint (
    P_MASTER_TRACK->children[
      num_master_children - 1], ==,
    TRACKLIST->num_tracks - 1);
  g_assert_cmpint (
    P_MASTER_TRACK->children[0], ==, 5);
  g_assert_cmpint (
    P_MASTER_TRACK->children[1], ==, 6);
  Track * new_track =
    TRACKLIST->tracks[TRACKLIST->num_tracks - 1];

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
          PLUGIN_SLOT_INSERT,
          new_track->pos, 0, descr, 1);
      undo_manager_perform (UNDO_MANAGER, ua);

      mixer_selections_clear (
        MIXER_SELECTIONS, F_NO_PUBLISH_EVENTS);
      mixer_selections_add_slot (
        MIXER_SELECTIONS, new_track,
        PLUGIN_SLOT_INSERT, 0, F_NO_CLONE);
      ua =
        mixer_selections_action_new_copy (
          MIXER_SELECTIONS, PLUGIN_SLOT_INSERT,
          -1, 0);
      undo_manager_perform (UNDO_MANAGER, ua);
      undo_manager_undo (UNDO_MANAGER);
      undo_manager_redo (UNDO_MANAGER);
      undo_manager_undo (UNDO_MANAGER);
#endif
    }
  else
    {
      mixer_selections_clear (
        MIXER_SELECTIONS, F_NO_PUBLISH_EVENTS);
      mixer_selections_add_slot (
        MIXER_SELECTIONS, track,
        PLUGIN_SLOT_INSERT, 0, F_NO_CLONE);
      ua =
        mixer_selections_action_new_copy (
          MIXER_SELECTIONS, PLUGIN_SLOT_INSERT,
          new_track->pos, 1);
      undo_manager_perform (UNDO_MANAGER, ua);
    }

  g_usleep (100);
}

static void
test_copy_plugins (void)
{
  test_helper_zrythm_init ();

#ifdef HAVE_HELM
  _test_copy_plugins (
    HELM_BUNDLE, HELM_URI, true, false);
#ifdef HAVE_CARLA
  _test_copy_plugins (
    HELM_BUNDLE, HELM_URI, true, true);
#endif /* HAVE_CARLA */
#endif /* HAVE_HELM */
#ifdef HAVE_NO_DELAY_LINE
  _test_copy_plugins (
    NO_DELAY_LINE_BUNDLE, NO_DELAY_LINE_URI,
    false, false);
#ifdef HAVE_CARLA
  _test_copy_plugins (
    NO_DELAY_LINE_BUNDLE, NO_DELAY_LINE_URI,
    false, true);
#endif /* HAVE_CARLA */
#endif /* HAVE_NO_DELAY_LINE */

  test_helper_zrythm_cleanup ();
}

static void
test_midi_fx_slot_deletion (void)
{
  test_helper_zrythm_init ();

  /* create MIDI track */
  UndoableAction * ua =
    tracklist_selections_action_new_create_midi (
      TRACKLIST->num_tracks, 1);
  undo_manager_perform (UNDO_MANAGER, ua);

#ifdef HAVE_MIDI_CC_MAP
  /* add plugin to slot */
  int slot = 0;
  PluginSetting * setting =
    test_plugin_manager_get_plugin_setting (
      MIDI_CC_MAP_BUNDLE, MIDI_CC_MAP_URI, false);
  ua =
    mixer_selections_action_new_create (
      PLUGIN_SLOT_MIDI_FX,
      TRACKLIST->num_tracks - 1, slot, setting, 1);
  undo_manager_perform (UNDO_MANAGER, ua);

  Track * track =
    TRACKLIST->tracks[TRACKLIST->num_tracks - 1];
  Plugin * pl = track->channel->midi_fx[slot];

  /* set the value to check if it is brought
   * back on undo */
  Port * port =
    plugin_get_port_by_symbol (pl, "ccin");
  port_set_control_value (
    port, 120.f, F_NOT_NORMALIZED, false);

  /* delete slot */
  plugin_select (pl, F_SELECT, F_EXCLUSIVE);
  ua =
    mixer_selections_action_new_delete (
      MIXER_SELECTIONS);
  undo_manager_perform (UNDO_MANAGER, ua);

  /* undo and check port value is restored */
  undo_manager_undo (UNDO_MANAGER);
  pl = track->channel->midi_fx[slot];
  port = plugin_get_port_by_symbol (pl, "ccin");
  g_assert_cmpfloat_with_epsilon (
    port->control, 120.f, 0.0001f);

  undo_manager_redo (UNDO_MANAGER);
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
  PluginSetting * setting = NULL;

  switch (prot)
    {
    case PROT_LV2:
      setting =
        plugin_setting_clone (
          test_plugin_manager_get_plugin_setting (
            pl_bundle, pl_uri, with_carla),
          F_NO_VALIDATE);
      break;
    case PROT_VST:
#ifdef HAVE_CARLA
        {
          PluginDescriptor ** descriptors =
            z_carla_discovery_create_descriptors_from_file (
              pl_bundle, ARCH_64, PROT_VST);
          setting =
            plugin_setting_new_default (
              descriptors[0]);
          free (descriptors);
        }
#endif
      break;
    default:
      break;
    }

  UndoableAction * ua = NULL;
  if (is_instrument)
    {
      /* create an instrument track from helm */
      ua =
        tracklist_selections_action_new_create (
          TRACK_TYPE_INSTRUMENT, setting, NULL,
          TRACKLIST->num_tracks, NULL, 1);
      undo_manager_perform (UNDO_MANAGER, ua);
    }
  else
    {
      /* create an audio fx track and add the
       * plugin */
      ua =
        tracklist_selections_action_new_create (
          TRACK_TYPE_AUDIO_BUS, NULL, NULL,
          TRACKLIST->num_tracks, NULL, 1);
      undo_manager_perform (UNDO_MANAGER, ua);
      ua =
        mixer_selections_action_new_create (
          PLUGIN_SLOT_INSERT,
          TRACKLIST->num_tracks - 1, 0, setting, 1);
      undo_manager_perform (UNDO_MANAGER, ua);

      if (string_is_equal (
            pl_uri,
            "http://open-music-kontrollers.ch/lv2/sherlock#atom_inspector") &&
          !with_carla)
        {
          Plugin * pl =
            TRACKLIST->tracks[
              TRACKLIST->num_tracks - 1]->channel->
                inserts[0];
          g_assert_true (pl->lv2->want_position);
        }
    }

  plugin_setting_free (setting);

  /* let the engine run */
  g_usleep (1000000);

  test_project_save_and_reload ();

  int src_track_pos = TRACKLIST->num_tracks - 1;
  Track * src_track =
    TRACKLIST->tracks[src_track_pos];

  if (is_instrument)
    {
      g_assert_true (src_track->channel->instrument);
    }
  else
    {
      g_assert_true (src_track->channel->inserts[0]);
    }

  /* duplicate the track */
  track_select (
    src_track, F_SELECT, true, F_NO_PUBLISH_EVENTS);
  ua =
    tracklist_selections_action_new_copy (
      TRACKLIST_SELECTIONS, TRACKLIST->num_tracks);
  g_assert_true (
    track_validate (src_track));
  undo_manager_perform (UNDO_MANAGER, ua);

  int dest_track_pos = TRACKLIST->num_tracks - 1;
  Track * dest_track =
    TRACKLIST->tracks[dest_track_pos];

  g_assert_true (
    track_validate (src_track));
  g_assert_true (
    track_validate (dest_track));

  undo_manager_undo (UNDO_MANAGER);
  undo_manager_undo (UNDO_MANAGER);
  undo_manager_redo (UNDO_MANAGER);
  undo_manager_redo (UNDO_MANAGER);

  g_message ("letting engine run...");

  /* let the engine run */
  g_usleep (1000000);

  test_project_save_and_reload ();

  g_message ("done");
}

static void
test_create_plugins (void)
{
  test_helper_zrythm_init ();

  for (int i = 0; i < 2; i++)
    {
      if (i == 1)
        {
#ifdef HAVE_CARLA
#ifdef HAVE_NOIZEMAKER
          _test_create_plugins (
            PROT_VST, NOIZEMAKER_PATH, NULL,
            true, i);
#endif
#else
          break;
#endif
        }

#ifdef HAVE_HELM
      _test_create_plugins (
        PROT_LV2, HELM_BUNDLE, HELM_URI, true, i);
#endif
#ifdef HAVE_LSP_COMPRESSOR
      _test_create_plugins (
        PROT_LV2, LSP_COMPRESSOR_BUNDLE,
        LSP_COMPRESSOR_URI, false, i);
#endif
#ifdef HAVE_SHERLOCK_ATOM_INSPECTOR
      _test_create_plugins (
        PROT_LV2, SHERLOCK_ATOM_INSPECTOR_BUNDLE,
        SHERLOCK_ATOM_INSPECTOR_URI,
        false, i);
#endif
#ifdef HAVE_CARLA_RACK
      _test_create_plugins (
        PROT_LV2, CARLA_RACK_BUNDLE, CARLA_RACK_URI,
        true, i);
#endif
#if defined (HAVE_UNLIMITED_MEM) && \
    defined (HAVE_CALF_COMPRESSOR)
      _test_create_plugins (
        PROT_LV2, CALF_COMPRESSOR_BUNDLE,
        CALF_COMPRESSOR_URI,
        true, i);
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
  PluginSetting * setting =
    test_plugin_manager_get_plugin_setting (
      pl_bundle, pl_uri, with_carla);

  /* create an instrument track from helm */
  UndoableAction * ua =
    tracklist_selections_action_new_create (
      TRACK_TYPE_AUDIO_BUS, setting, NULL,
      TRACKLIST->num_tracks, NULL, 1);
  undo_manager_perform (UNDO_MANAGER, ua);

  plugin_setting_free (setting);

  int src_track_pos = TRACKLIST->num_tracks - 1;
  int dest_track_pos = TRACKLIST->num_tracks;

  /* select it */
  Track * src_track =
    TRACKLIST->tracks[src_track_pos];
  track_select (
    src_track, F_SELECT, true, F_NO_PUBLISH_EVENTS);

  /* get an automation track */
  AutomationTracklist * atl =
    track_get_automation_tracklist (src_track);
  AutomationTrack * at = atl->ats[atl->num_ats - 1];
  at->created = true;
  at->visible = true;

  /* create an automation region */
  Position start_pos, end_pos;
  position_set_to_bar (&start_pos, 2);
  position_set_to_bar (&end_pos, 4);
  ZRegion * region =
    automation_region_new (
      &start_pos, &end_pos, src_track->pos,
      at->index, at->num_regions);
  track_add_region  (
    src_track, region, at, -1, F_GEN_NAME,
    F_NO_PUBLISH_EVENTS);
  arranger_object_select (
    (ArrangerObject *) region, true, false, F_NO_PUBLISH_EVENTS);
  ua =
    arranger_selections_action_new_create (
      TL_SELECTIONS);
  undo_manager_perform (UNDO_MANAGER, ua);

  /* create some automation points */
  Port * port = automation_track_get_port (at);
  position_set_to_bar (&start_pos, 1);
  AutomationPoint * ap =
    automation_point_new_float (
      port->deff,
      control_port_real_val_to_normalized (
        port, port->deff),
      &start_pos);
  automation_region_add_ap (
    region, ap, F_NO_PUBLISH_EVENTS);
  arranger_object_select (
    (ArrangerObject *) ap, true, false, F_NO_PUBLISH_EVENTS);
  ua =
    arranger_selections_action_new_create (
      AUTOMATION_SELECTIONS);
  undo_manager_perform (UNDO_MANAGER, ua);

  /* duplicate it */
  ua =
    tracklist_selections_action_new_copy (
      TRACKLIST_SELECTIONS, TRACKLIST->num_tracks);

  g_assert_true (
    track_validate (src_track));

  undo_manager_perform (UNDO_MANAGER, ua);

  Track * dest_track =
    TRACKLIST->tracks[dest_track_pos];

  g_assert_true (
    track_validate (src_track));
  g_assert_true (
    track_validate (dest_track));

  /* move plugin from 1st track to 2nd track and
   * undo/redo */
  mixer_selections_clear (
    MIXER_SELECTIONS, F_NO_PUBLISH_EVENTS);
  mixer_selections_add_slot (
    MIXER_SELECTIONS, src_track,
    PLUGIN_SLOT_INSERT, 0, F_NO_CLONE);
  ua =
    mixer_selections_action_new_move (
      MIXER_SELECTIONS, PLUGIN_SLOT_INSERT,
      dest_track->pos, 1);

  undo_manager_perform (UNDO_MANAGER, ua);

  /* let the engine run */
  g_usleep (1000000);

  g_assert_true (
    track_validate (src_track));
  g_assert_true (
    track_validate (dest_track));

  undo_manager_undo (UNDO_MANAGER);

  g_assert_true (
    track_validate (src_track));
  g_assert_true (
    track_validate (dest_track));

  undo_manager_redo (UNDO_MANAGER);

  g_assert_true (
    track_validate (src_track));
  g_assert_true (
    track_validate (dest_track));

  undo_manager_undo (UNDO_MANAGER);

  /* move plugin from 1st slot to the 2nd slot and
   * undo/redo */
  mixer_selections_clear (
    MIXER_SELECTIONS, F_NO_PUBLISH_EVENTS);
  mixer_selections_add_slot (
    MIXER_SELECTIONS, src_track,
    PLUGIN_SLOT_INSERT, 0, F_NO_CLONE);
  ua =
    mixer_selections_action_new_move (
      MIXER_SELECTIONS, PLUGIN_SLOT_INSERT,
      src_track->pos, 1);
  undo_manager_perform (UNDO_MANAGER, ua);
  undo_manager_undo (UNDO_MANAGER);
  undo_manager_redo (UNDO_MANAGER);

  /* let the engine run */
  g_usleep (1000000);

  /* move the plugin to a new track */
  mixer_selections_clear (
    MIXER_SELECTIONS, F_NO_PUBLISH_EVENTS);
  src_track = TRACKLIST->tracks[src_track_pos];
  mixer_selections_add_slot (
    MIXER_SELECTIONS, src_track,
    PLUGIN_SLOT_INSERT, 1, F_NO_CLONE);
  ua =
    mixer_selections_action_new_move (
      MIXER_SELECTIONS, PLUGIN_SLOT_INSERT,
      -1, 0);
  undo_manager_perform (UNDO_MANAGER, ua);
  undo_manager_undo (UNDO_MANAGER);
  undo_manager_redo (UNDO_MANAGER);

  /* let the engine run */
  g_usleep (1000000);

  /* go back to the start */
  undo_manager_undo (UNDO_MANAGER);
  undo_manager_undo (UNDO_MANAGER);
  undo_manager_undo (UNDO_MANAGER);
  undo_manager_undo (UNDO_MANAGER);
  undo_manager_undo (UNDO_MANAGER);
}
#endif

static void
test_port_and_plugin_track_pos_after_move (void)
{
  return;
  test_helper_zrythm_init ();

#ifdef HAVE_LSP_COMPRESSOR
  _test_port_and_plugin_track_pos_after_move (
    LSP_COMPRESSOR_BUNDLE, LSP_COMPRESSOR_URI,
    false);
#endif

  test_helper_zrythm_cleanup ();
}

#ifdef HAVE_CARLA
static void
test_port_and_plugin_track_pos_after_move_with_carla (void)
{
  return;
  test_helper_zrythm_init ();

#ifdef HAVE_LSP_COMPRESSOR
  _test_port_and_plugin_track_pos_after_move (
    LSP_COMPRESSOR_BUNDLE, LSP_COMPRESSOR_URI,
    true);
#endif

  test_helper_zrythm_cleanup ();
}
#endif

static void
test_move_two_plugins_one_slot_up (void)
{
  test_helper_zrythm_init ();

#ifdef HAVE_LSP_COMPRESSOR
  /* create a track with an insert */
  PluginSetting * setting =
    test_plugin_manager_get_plugin_setting (
      LSP_COMPRESSOR_BUNDLE,
      LSP_COMPRESSOR_URI, false);
  UndoableAction * ua =
    tracklist_selections_action_new_create (
      TRACK_TYPE_AUDIO_BUS, setting, NULL,
      TRACKLIST->num_tracks, NULL, 1);
  undo_manager_perform (UNDO_MANAGER, ua);
  undo_manager_undo (UNDO_MANAGER);
  undo_manager_redo (UNDO_MANAGER);
  plugin_setting_free (setting);

  int track_pos = TRACKLIST->num_tracks - 1;

  /* select it */
  Track * track =
    TRACKLIST->tracks[track_pos];
  track_select (
    track, F_SELECT, true, F_NO_PUBLISH_EVENTS);

  /* save and reload the project */
  test_project_save_and_reload ();
  track = TRACKLIST->tracks[track_pos];
  g_assert_true (track_validate (track));

  /* get an automation track */
  AutomationTracklist * atl =
    track_get_automation_tracklist (track);
  AutomationTrack * at = atl->ats[atl->num_ats - 1];
  g_message (
    "automation track %s", at->port_id.label);
  at->created = true;
  at->visible = true;

  /* create an automation region */
  Position start_pos, end_pos;
  position_set_to_bar (&start_pos, 2);
  position_set_to_bar (&end_pos, 4);
  ZRegion * region =
    automation_region_new (
      &start_pos, &end_pos, track->pos,
      at->index, at->num_regions);
  track_add_region  (
    track, region, at, -1, F_GEN_NAME,
    F_NO_PUBLISH_EVENTS);
  arranger_object_select (
    (ArrangerObject *) region, true, false, F_NO_PUBLISH_EVENTS);
  ua =
    arranger_selections_action_new_create (
      TL_SELECTIONS);
  undo_manager_perform (UNDO_MANAGER, ua);
  undo_manager_undo (UNDO_MANAGER);
  undo_manager_redo (UNDO_MANAGER);

  /* save and reload the project */
  test_project_save_and_reload ();
  track = TRACKLIST->tracks[track_pos];
  g_assert_true (track_validate (track));
  atl = track_get_automation_tracklist (track);
  at = atl->ats[atl->num_ats - 1];

  /* create some automation points */
  Port * port = automation_track_get_port (at);
  position_set_to_bar (&start_pos, 1);
  atl = track_get_automation_tracklist (track);
  at = atl->ats[atl->num_ats - 1];
  g_assert_cmpint (at->num_regions, >, 0);
  region = at->regions[0];
  AutomationPoint * ap =
    automation_point_new_float (
      port->deff,
      control_port_real_val_to_normalized (
        port, port->deff),
      &start_pos);
  automation_region_add_ap (
    region, ap, F_NO_PUBLISH_EVENTS);
  arranger_object_select (
    (ArrangerObject *) ap, true, false, F_NO_PUBLISH_EVENTS);
  ua =
    arranger_selections_action_new_create (
      AUTOMATION_SELECTIONS);
  undo_manager_perform (UNDO_MANAGER, ua);
  undo_manager_undo (UNDO_MANAGER);
  undo_manager_redo (UNDO_MANAGER);

  /* save and reload the project */
  test_project_save_and_reload ();
  track = TRACKLIST->tracks[track_pos];
  g_assert_true (track_validate (track));

  /* duplicate the plugin to the 2nd slot */
  mixer_selections_clear (
    MIXER_SELECTIONS, F_NO_PUBLISH_EVENTS);
  mixer_selections_add_slot (
    MIXER_SELECTIONS, track,
    PLUGIN_SLOT_INSERT, 0, F_NO_CLONE);
  ua =
    mixer_selections_action_new_copy (
      MIXER_SELECTIONS, PLUGIN_SLOT_INSERT,
      track->pos, 1);
  undo_manager_perform (UNDO_MANAGER, ua);
  undo_manager_undo (UNDO_MANAGER);
  undo_manager_redo (UNDO_MANAGER);

  /* at this point we have a plugin at slot#0 and
   * its clone at slot#1 */

  /* remove slot #0 and undo */
  mixer_selections_clear (
    MIXER_SELECTIONS, F_NO_PUBLISH_EVENTS);
  mixer_selections_add_slot (
    MIXER_SELECTIONS, track,
    PLUGIN_SLOT_INSERT, 0, F_NO_CLONE);
  ua =
    mixer_selections_action_new_delete (
      MIXER_SELECTIONS);
  undo_manager_perform (UNDO_MANAGER, ua);
  undo_manager_undo (UNDO_MANAGER);
  undo_manager_redo (UNDO_MANAGER);
  undo_manager_undo (UNDO_MANAGER);

  /* save and reload the project */
  test_project_save_and_reload ();
  track = TRACKLIST->tracks[track_pos];
  g_assert_true (track_validate (track));

  /* move the 2 plugins to start at slot#1 (2nd
   * slot) */
  mixer_selections_clear (
    MIXER_SELECTIONS, F_NO_PUBLISH_EVENTS);
  mixer_selections_add_slot (
    MIXER_SELECTIONS, track,
    PLUGIN_SLOT_INSERT, 0, F_NO_CLONE);
  mixer_selections_add_slot (
    MIXER_SELECTIONS, track,
    PLUGIN_SLOT_INSERT, 1, F_NO_CLONE);
  ua =
    mixer_selections_action_new_move (
      MIXER_SELECTIONS, PLUGIN_SLOT_INSERT,
      track->pos, 1);
  undo_manager_perform (UNDO_MANAGER, ua);
  g_assert_true (
    track_validate (track));
  undo_manager_undo (UNDO_MANAGER);
  g_assert_true (
    track_validate (track));
  undo_manager_redo (UNDO_MANAGER);
  g_assert_true (
    track_validate (track));

  /* save and reload the project */
  test_project_save_and_reload ();
  track = TRACKLIST->tracks[track_pos];
  g_assert_true (track_validate (track));

  /* move the 2 plugins to start at slot 2 (3rd
   * slot) */
  mixer_selections_clear (
    MIXER_SELECTIONS, F_NO_PUBLISH_EVENTS);
  mixer_selections_add_slot (
    MIXER_SELECTIONS, track,
    PLUGIN_SLOT_INSERT, 1, F_NO_CLONE);
  mixer_selections_add_slot (
    MIXER_SELECTIONS, track,
    PLUGIN_SLOT_INSERT, 2, F_NO_CLONE);
  ua =
    mixer_selections_action_new_move (
      MIXER_SELECTIONS, PLUGIN_SLOT_INSERT,
      track->pos, 2);
  undo_manager_perform (UNDO_MANAGER, ua);
  g_assert_true (
    track_validate (track));
  undo_manager_undo (UNDO_MANAGER);
  g_assert_true (
    track_validate (track));
  undo_manager_redo (UNDO_MANAGER);
  g_assert_true (
    track_validate (track));

  /* save and reload the project */
  test_project_save_and_reload ();
  track = TRACKLIST->tracks[track_pos];
  g_assert_true (track_validate (track));

  /* move the 2 plugins to start at slot 1 (2nd
   * slot) */
  mixer_selections_clear (
    MIXER_SELECTIONS, F_NO_PUBLISH_EVENTS);
  mixer_selections_add_slot (
    MIXER_SELECTIONS, track,
    PLUGIN_SLOT_INSERT, 2, F_NO_CLONE);
  mixer_selections_add_slot (
    MIXER_SELECTIONS, track,
    PLUGIN_SLOT_INSERT, 3, F_NO_CLONE);
  ua =
    mixer_selections_action_new_move (
      MIXER_SELECTIONS, PLUGIN_SLOT_INSERT,
      track->pos, 1);
  undo_manager_perform (UNDO_MANAGER, ua);
  g_assert_true (
    track_validate (track));
  undo_manager_undo (UNDO_MANAGER);
  g_assert_true (
    track_validate (track));
  undo_manager_redo (UNDO_MANAGER);
  g_assert_true (
    track_validate (track));

  /* save and reload the project */
  test_project_save_and_reload ();
  track = TRACKLIST->tracks[track_pos];
  g_assert_true (track_validate (track));

  /* move the 2 plugins to start back at slot 0 (1st
   * slot) */
  mixer_selections_clear (
    MIXER_SELECTIONS, F_NO_PUBLISH_EVENTS);
  mixer_selections_add_slot (
    MIXER_SELECTIONS, track,
    PLUGIN_SLOT_INSERT, 2, F_NO_CLONE);
  mixer_selections_add_slot (
    MIXER_SELECTIONS, track,
    PLUGIN_SLOT_INSERT, 1, F_NO_CLONE);
  ua =
    mixer_selections_action_new_move (
      MIXER_SELECTIONS, PLUGIN_SLOT_INSERT,
      track->pos, 0);
  undo_manager_perform (UNDO_MANAGER, ua);
  undo_manager_undo (UNDO_MANAGER);
  undo_manager_redo (UNDO_MANAGER);

  g_assert_true (
    track_validate (track));
  g_assert_true (
    IS_PLUGIN (track->channel->inserts[0]));
  g_assert_true (
    IS_PLUGIN (track->channel->inserts[1]));

  /* move 2nd plugin to 1st plugin (replacing it) */
  mixer_selections_clear (
    MIXER_SELECTIONS, F_NO_PUBLISH_EVENTS);
  mixer_selections_add_slot (
    MIXER_SELECTIONS, track,
    PLUGIN_SLOT_INSERT, 1, F_NO_CLONE);
  ua =
    mixer_selections_action_new_move (
      MIXER_SELECTIONS, PLUGIN_SLOT_INSERT,
      track->pos, 0);
  undo_manager_perform (UNDO_MANAGER, ua);

  /* verify that first plugin was replaced by 2nd
   * plugin */
  g_assert_true (
    IS_PLUGIN_AND_NONNULL (
      track->channel->inserts[0]));
  g_assert_null (track->channel->inserts[1]);

  /* undo and verify that both plugins are back */
  undo_manager_undo (UNDO_MANAGER);
  g_assert_true (
    IS_PLUGIN (track->channel->inserts[0]));
  g_assert_true (
    IS_PLUGIN (track->channel->inserts[1]));
  undo_manager_redo (UNDO_MANAGER);
  undo_manager_undo (UNDO_MANAGER);
  g_assert_true (
    IS_PLUGIN (track->channel->inserts[0]));
  g_assert_cmpstr (
    track->channel->inserts[0]->setting->descr->uri, ==,
    LSP_COMPRESSOR_URI);
  g_assert_true (
    IS_PLUGIN (track->channel->inserts[1]));

  test_project_save_and_reload ();
  track = TRACKLIST->tracks[track_pos];

  /* TODO verify that custom connections are back */

#ifdef HAVE_MIDI_CC_MAP
  /* add plugin to slot 0 (replacing current) */
  setting =
    test_plugin_manager_get_plugin_setting (
      MIDI_CC_MAP_BUNDLE, MIDI_CC_MAP_URI, false);
  ua =
    mixer_selections_action_new_create (
      PLUGIN_SLOT_INSERT, track->pos, 0, setting, 1);
  undo_manager_perform (UNDO_MANAGER, ua);

  /* undo and verify that the original plugin is
   * back */
  undo_manager_undo (UNDO_MANAGER);
  g_assert_true (
    IS_PLUGIN (track->channel->inserts[0]));
  g_assert_cmpstr (
    track->channel->inserts[0]->setting->descr->uri, ==,
    LSP_COMPRESSOR_URI);
  g_assert_true (
    IS_PLUGIN (track->channel->inserts[1]));

  /* redo */
  undo_manager_redo (UNDO_MANAGER);
  g_assert_true (
    IS_PLUGIN (track->channel->inserts[0]));
  g_assert_cmpstr (
    track->channel->inserts[0]->setting->descr->uri, ==,
    setting->descr->uri);
  g_assert_true (
    IS_PLUGIN (track->channel->inserts[1]));

  Plugin * pl = track->channel->inserts[0];

  /* set the value to check if it is brought
   * back on undo */
  port =
    plugin_get_port_by_symbol (pl, "ccin");
  port_set_control_value (
    port, 120.f, F_NOT_NORMALIZED, false);

  /* move 2nd plugin to 1st plugin (replacing it) */
  mixer_selections_clear (
    MIXER_SELECTIONS, F_NO_PUBLISH_EVENTS);
  mixer_selections_add_slot (
    MIXER_SELECTIONS, track,
    PLUGIN_SLOT_INSERT, 1, F_NO_CLONE);
  ua =
    mixer_selections_action_new_move (
      MIXER_SELECTIONS, PLUGIN_SLOT_INSERT,
      track->pos, 0);
  undo_manager_perform (UNDO_MANAGER, ua);

  test_project_save_and_reload ();
  track = TRACKLIST->tracks[track_pos];

  g_assert_true (
    IS_PLUGIN (track->channel->inserts[0]));
  g_assert_null (track->channel->inserts[1]);

  /* undo and check plugin and port value are
   * restored */
  undo_manager_undo (UNDO_MANAGER);
  pl = track->channel->inserts[0];
  g_assert_cmpstr (pl->setting->descr->uri, ==, setting->descr->uri);
  port = plugin_get_port_by_symbol (pl, "ccin");
  g_assert_cmpfloat_with_epsilon (
    port->control, 120.f, 0.0001f);

  g_assert_true (
    IS_PLUGIN (track->channel->inserts[0]));
  g_assert_true (
    IS_PLUGIN (track->channel->inserts[1]));

  test_project_save_and_reload ();
  track = TRACKLIST->tracks[track_pos];

  undo_manager_redo (UNDO_MANAGER);
#endif

  g_assert_true (
    track_validate (track));

  /* let the engine run */
  g_usleep (1000000);

  test_project_save_and_reload ();

  undo_manager_undo (UNDO_MANAGER);
  undo_manager_redo (UNDO_MANAGER);

  undo_manager_undo (UNDO_MANAGER);
  undo_manager_undo (UNDO_MANAGER);
  undo_manager_undo (UNDO_MANAGER);
  undo_manager_undo (UNDO_MANAGER);
#endif

  test_helper_zrythm_cleanup ();
}

static void
test_create_modulator (void)
{
  test_helper_zrythm_init ();

#ifdef HAVE_AMS_LFO
#ifdef HAVE_CARLA
  /* create a track with an insert */
  PluginSetting * setting =
    test_plugin_manager_get_plugin_setting (
      AMS_LFO_BUNDLE, AMS_LFO_URI, false);
  UndoableAction * ua =
    mixer_selections_action_new_create (
      PLUGIN_SLOT_MODULATOR,
      P_MODULATOR_TRACK->pos,
      P_MODULATOR_TRACK->num_modulators, setting, 1);
  undo_manager_perform (UNDO_MANAGER, ua);
  undo_manager_undo (UNDO_MANAGER);
  undo_manager_redo (UNDO_MANAGER);
  undo_manager_undo (UNDO_MANAGER);
  undo_manager_redo (UNDO_MANAGER);

  /* create another one */
  ua =
    mixer_selections_action_new_create (
      PLUGIN_SLOT_MODULATOR,
      P_MODULATOR_TRACK->pos,
      P_MODULATOR_TRACK->num_modulators, setting, 1);
  undo_manager_perform (UNDO_MANAGER, ua);

  /* connect a cv output from the first modulator
   * to a control of the 2nd */
  Plugin * p1 =
    P_MODULATOR_TRACK->modulators[
      P_MODULATOR_TRACK->num_modulators - 2];
  Plugin * p2 =
    P_MODULATOR_TRACK->modulators[
      P_MODULATOR_TRACK->num_modulators - 1];
  Port * cv_out = NULL;
  for (int i = 0; i < p1->num_out_ports; i++)
    {
      Port * p = p1->out_ports[i];
      if (p->id.type != TYPE_CV)
        continue;
      cv_out = p;
    }
  Port * ctrl_in =
    plugin_get_port_by_symbol (
      p2, "freq");
  g_assert_nonnull (cv_out);
  g_assert_nonnull (ctrl_in);
  PortIdentifier cv_out_id = cv_out->id;
  PortIdentifier ctrl_in_id = ctrl_in->id;

  /* connect the ports */
  ua =
    port_connection_action_new_connect (
      &cv_out->id, &ctrl_in->id);
  undo_manager_perform (UNDO_MANAGER, ua);
  undo_manager_undo (UNDO_MANAGER);
  undo_manager_redo (UNDO_MANAGER);

  MixerSelections * sel = mixer_selections_new ();
  mixer_selections_add_slot (
    sel, P_MODULATOR_TRACK, PLUGIN_SLOT_MODULATOR,
    P_MODULATOR_TRACK->num_modulators - 2,
    F_NO_CLONE);
  ua = mixer_selections_action_new_delete (sel);
  undo_manager_perform (UNDO_MANAGER, ua);
  undo_manager_undo (UNDO_MANAGER);

  /* verify port connection is back */
  cv_out = port_find_from_identifier (&cv_out_id);
  ctrl_in = port_find_from_identifier (&ctrl_in_id);
  g_assert_true (ports_connected (cv_out, ctrl_in));

  undo_manager_redo (UNDO_MANAGER);
  mixer_selections_free (sel);

  plugin_setting_free (setting);
#endif /* HAVE_CARLA */
#endif /* HAVE_AMS_LFO */

  test_helper_zrythm_cleanup ();
}

static void
test_move_pl_after_duplicating_track (void)
{
  test_helper_zrythm_init ();

#if defined (HAVE_LSP_SIDECHAIN_COMPRESSOR) && \
  defined (HAVE_HELM)

  test_plugin_manager_create_tracks_from_plugin (
      LSP_SIDECHAIN_COMPRESSOR_BUNDLE,
      LSP_SIDECHAIN_COMPRESSOR_URI, false, false,
      1);
  test_plugin_manager_create_tracks_from_plugin (
      HELM_BUNDLE, HELM_URI, true, false, 1);

  Track * ins_track =
    TRACKLIST->tracks[TRACKLIST->num_tracks - 1];
  Track * lsp_track =
    TRACKLIST->tracks[TRACKLIST->num_tracks - 2];
  Plugin * lsp =
    lsp_track->channel->inserts[0];

  Port * sidechain_port = NULL;
  for (int i = 0; i < lsp->num_in_ports; i++)
    {
      Port * port = lsp->in_ports[i];
      if (port->id.flags & PORT_FLAG_SIDECHAIN)
        {
          sidechain_port = port;
          break;
        }
    }
  g_assert_nonnull (sidechain_port);

  UndoableAction * ua =
    port_connection_action_new_connect (
      &ins_track->channel->fader->stereo_out->
        l->id,
      &sidechain_port->id);
  undo_manager_perform (UNDO_MANAGER, ua);

  track_select (
    ins_track, F_SELECT, F_EXCLUSIVE,
    F_NO_PUBLISH_EVENTS);
  ua =
    tracklist_selections_action_new_copy (
      TRACKLIST_SELECTIONS, TRACKLIST->num_tracks);
  undo_manager_perform (UNDO_MANAGER, ua);

  Track * dest_track =
    TRACKLIST->tracks[TRACKLIST->num_tracks - 1];

  mixer_selections_clear (
    MIXER_SELECTIONS, F_NO_PUBLISH_EVENTS);
  mixer_selections_add_slot (
    MIXER_SELECTIONS, lsp_track,
    PLUGIN_SLOT_INSERT, 0, F_NO_CLONE);
  ua =
    mixer_selections_action_new_move (
      MIXER_SELECTIONS, PLUGIN_SLOT_INSERT,
      dest_track->pos, 1);
  undo_manager_perform (UNDO_MANAGER, ua);

#endif

  test_helper_zrythm_cleanup ();
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

#define TEST_PREFIX "/actions/mixer_selections_action/"

  g_test_add_func (
    TEST_PREFIX
    "test move two plugins one slot up",
    (GTestFunc)
    test_move_two_plugins_one_slot_up);
  g_test_add_func (
    TEST_PREFIX
    "test move pl after duplicating track",
    (GTestFunc)
    test_move_pl_after_duplicating_track);
  g_test_add_func (
    TEST_PREFIX "test create modulator",
    (GTestFunc) test_create_modulator);
  g_test_add_func (
    TEST_PREFIX "test MIDI fx slot deletion",
    (GTestFunc) test_midi_fx_slot_deletion);
  g_test_add_func (
    TEST_PREFIX "test copy plugins",
    (GTestFunc) test_copy_plugins);
  g_test_add_func (
    TEST_PREFIX "test create plugins",
    (GTestFunc) test_create_plugins);
  g_test_add_func (
    TEST_PREFIX
    "test port and plugin track pos after move",
    (GTestFunc)
    test_port_and_plugin_track_pos_after_move);
#ifdef HAVE_CARLA
  g_test_add_func (
    TEST_PREFIX
    "test port and plugin track pos after move with carla",
    (GTestFunc)
    test_port_and_plugin_track_pos_after_move_with_carla);
#endif

  return g_test_run ();
}
