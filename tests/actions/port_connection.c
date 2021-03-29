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

#include "actions/port_connection_action.h"
#include "actions/undoable_action.h"
#include "actions/undo_manager.h"
#include "audio/audio_region.h"
#include "audio/automation_region.h"
#include "audio/chord_region.h"
#include "audio/control_port.h"
#include "audio/master_track.h"
#include "audio/midi_note.h"
#include "audio/region.h"
#include "plugins/plugin_manager.h"
#include "project.h"
#include "utils/flags.h"
#include "zrythm.h"

#include "tests/helpers/plugin_manager.h"
#include "tests/helpers/project.h"

#include <glib.h>

#ifdef HAVE_CARLA
#ifdef HAVE_AMS_LFO
static void
test_modulator_connection (
  const char * pl_bundle,
  const char * pl_uri,
  bool         is_instrument,
  bool         with_carla)
{
  PluginSetting * setting =
    test_plugin_manager_get_plugin_setting (
      pl_bundle, pl_uri, with_carla);

  /* fix the descriptor (for some reason lilv
   * reports it as Plugin instead of Instrument if
   * you don't do lilv_world_load_all) */
  if (is_instrument)
    {
      setting->descr->category = PC_INSTRUMENT;
    }
  g_free (setting->descr->category_str);
  setting->descr->category_str =
    plugin_descriptor_category_to_string (
      setting->descr->category);

  UndoableAction * ua = NULL;

  /* create a modulator */
  ua =
    mixer_selections_action_new_create (
      PLUGIN_SLOT_MODULATOR, P_MODULATOR_TRACK->pos,
      0, setting, 1);
  undo_manager_perform (UNDO_MANAGER, ua);
  plugin_setting_free (setting);

  ModulatorMacroProcessor * macro =
    P_MODULATOR_TRACK->modulator_macros[0];
  Plugin * pl = P_MODULATOR_TRACK->modulators[0];
  Port * pl_cv_port = NULL;
  Port * pl_control_port = NULL;
  size_t max_size = 0;
  Port ** ports = NULL;
  int num_ports = 0;
  plugin_append_ports (
    pl, &ports, &max_size, true, &num_ports);
  for (int i = 0; i < num_ports; i++)
    {
      Port * port = ports[i];
      if (port->id.type == TYPE_CV &&
          port->id.flow == FLOW_OUTPUT)
        {
          pl_cv_port = port;
          if (pl_control_port)
            {
              break;
            }
        }
      else if (port->id.type == TYPE_CONTROL &&
               port->id.flow == FLOW_INPUT)
        {
          pl_control_port = port;
          if (pl_cv_port)
            {
              break;
            }
        }
    }
  object_zero_and_free_if_nonnull (ports);

  /* connect the plugin's CV out to the macro
   * button */
  ua =
    port_connection_action_new_connect (
      &pl_cv_port->id, &macro->cv_in->id);
  int ret = undo_manager_perform (UNDO_MANAGER, ua);
  g_assert_cmpint (ret, ==, 0);

  /* expect messages */
  LOG->use_structured_for_console = false;
  LOG->min_log_level_for_test_console =
    G_LOG_LEVEL_WARNING;
  g_test_expect_message (
    G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
    "*ports cannot be connected*");
  g_test_expect_message (
    G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
    "*action not performed*");

  /* connect the macro button to the plugin's
   * control input */
  ua =
    port_connection_action_new_connect (
      &macro->cv_out->id, &pl_control_port->id);
  ret = undo_manager_perform (UNDO_MANAGER, ua);
  g_assert_cmpint (ret, !=, 0);

  /* let the engine run */
  g_usleep (1000000);

  /* assert expected messages */
  g_test_assert_expected_messages ();
}

static void
_test_port_connection (
  const char * pl_bundle,
  const char * pl_uri,
  bool         is_instrument,
  bool         with_carla)
{
  PluginSetting * setting =
    test_plugin_manager_get_plugin_setting (
      pl_bundle, pl_uri, with_carla);

  /* fix the descriptor (for some reason lilv
   * reports it as Plugin instead of Instrument if
   * you don't do lilv_world_load_all) */
  if (is_instrument)
    {
      setting->descr->category = PC_INSTRUMENT;
    }
  g_free (setting->descr->category_str);
  setting->descr->category_str =
    plugin_descriptor_category_to_string (
      setting->descr->category);

  UndoableAction * ua = NULL;

  /* create an extra track */
  ua =
    tracklist_selections_action_new_create (
      TRACK_TYPE_AUDIO_BUS, NULL, NULL,
      TRACKLIST->num_tracks, NULL, 1);
  undo_manager_perform (UNDO_MANAGER, ua);
  Track * target_track =
    TRACKLIST->tracks[TRACKLIST->num_tracks - 1];

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
      /* create an audio fx track and add the plugin */
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
    }

  plugin_setting_free (setting);

  Track * src_track =
    TRACKLIST->tracks[TRACKLIST->num_tracks - 1];

  /* connect a plugin CV out to the track's
   * balance */
  Port * src_port1 = NULL;
  Port * src_port2 = NULL;
  size_t max_size = 0;
  Port ** ports = NULL;
  int num_ports = 0;
  track_append_all_ports (
    src_track, &ports, &num_ports, true, &max_size,
    true);
  for (int i = 0; i < num_ports; i++)
    {
      Port * port = ports[i];
      if (port->id.owner_type ==
            PORT_OWNER_TYPE_PLUGIN &&
          port->id.type == TYPE_CV &&
          port->id.flow == FLOW_OUTPUT)
        {
          if (src_port1)
            {
              src_port2 = port;
              break;
            }
          else
            {
              src_port1 = port;
              continue;
            }
        }
    }
  g_assert_nonnull (src_port1);
  g_assert_nonnull (src_port2);
  object_zero_and_free_if_nonnull (ports);

  Port * dest_port = NULL;
  max_size = 0;
  ports = NULL;
  num_ports = 0;
  track_append_all_ports (
    target_track, &ports, &num_ports, true, &max_size,
    true);
  g_assert_nonnull (ports);
  for (int i = 0; i < num_ports; i++)
    {
      Port * port = ports[i];
      if (port->id.owner_type ==
            PORT_OWNER_TYPE_FADER &&
          port->id.flags & PORT_FLAG_STEREO_BALANCE)
        {
          dest_port = port;
          break;
        }
    }
  object_zero_and_free_if_nonnull (ports);

  g_assert_nonnull (dest_port);
  g_assert_true (src_port1->is_project);
  g_assert_true (src_port2->is_project);
  g_assert_true (dest_port->is_project);

  ua =
    port_connection_action_new_connect (
      &src_port1->id, &dest_port->id);
  undo_manager_perform (UNDO_MANAGER, ua);

  g_assert_cmpint (dest_port->num_srcs, ==, 1);
  g_assert_cmpint (src_port1->num_dests, ==, 1);

  undo_manager_undo (UNDO_MANAGER);

  g_assert_cmpint (dest_port->num_srcs, ==, 0);
  g_assert_cmpint (src_port1->num_dests, ==, 0);

  undo_manager_redo (UNDO_MANAGER);

  g_assert_cmpint (dest_port->num_srcs, ==, 1);
  g_assert_cmpint (src_port1->num_dests, ==, 1);

  ua =
    port_connection_action_new_connect (
      &src_port2->id, &dest_port->id);
  undo_manager_perform (UNDO_MANAGER, ua);

  g_assert_cmpint (dest_port->num_srcs, ==, 2);
  g_assert_cmpint (src_port1->num_dests, ==, 1);
  g_assert_cmpint (src_port2->num_dests, ==, 1);
  g_assert_true (
    port_identifier_is_equal (
      &src_port1->dest_ids[0], &dest_port->id));
  g_assert_true (
    port_identifier_is_equal (
      &dest_port->src_ids[0], &src_port1->id));
  g_assert_true (
    port_identifier_is_equal (
      &src_port2->dest_ids[0], &dest_port->id));
  g_assert_true (
    port_identifier_is_equal (
      &dest_port->src_ids[1], &src_port2->id));
  g_assert_true (
    dest_port->srcs[0] == src_port1);
  g_assert_true (
    dest_port == src_port1->dests[0]);
  g_assert_true (
    dest_port->srcs[1] == src_port2);
  g_assert_true (
    dest_port == src_port2->dests[0]);

  undo_manager_undo (UNDO_MANAGER);
  undo_manager_redo (UNDO_MANAGER);

  /* let the engine run */
  g_usleep (1000000);
}
#endif /* HAVE_AMS_LFO */
#endif /* HAVE_CARLA */

static void
test_port_connection (void)
{
  test_helper_zrythm_init ();

#ifdef HAVE_AMS_LFO
#ifdef HAVE_CARLA
  _test_port_connection (
    AMS_LFO_BUNDLE, AMS_LFO_URI, true, false);
  test_modulator_connection (
    AMS_LFO_BUNDLE, AMS_LFO_URI, true, false);
#endif /* HAVE_CARLA */
#endif /* HAVE_AMS_LFO */

  test_helper_zrythm_cleanup ();
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

#define TEST_PREFIX "/actions/port_connection/"

  g_test_add_func (
    TEST_PREFIX "test_port_connection",
    (GTestFunc) test_port_connection);

  return g_test_run ();
}

