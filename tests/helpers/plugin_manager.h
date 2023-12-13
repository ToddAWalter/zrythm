// SPDX-FileCopyrightText: © 2020-2021 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

/**
 * \file
 *
 * Plugin manager helper.
 */

#ifndef __TEST_HELPERS_PLUGIN_MANAGER_H__
#define __TEST_HELPERS_PLUGIN_MANAGER_H__

#include "zrythm-test-config.h"

#include "actions/tracklist_selections.h"
#include "plugins/lv2_plugin.h"
#include "plugins/plugin_manager.h"
#include "project.h"
#include "utils/flags.h"
#include "utils/objects.h"
#include "utils/string.h"
#include "utils/ui.h"
#include "zrythm.h"

#include <glib.h>

/**
 * @addtogroup tests
 *
 * @{
 */

void
test_plugin_manager_reload_lilv_world_w_path (const char * path);

/**
 * Get a plugin setting clone from the given
 * URI in the given bundle.
 */
PluginSetting *
test_plugin_manager_get_plugin_setting (
  const char * pl_bundle,
  const char * pl_uri,
  bool         with_carla);

/**
 * Creates \ref num_tracks tracks for the given
 * plugin.
 *
 * @param num_tracks Number of tracks to create.
 *
 * @return The index of the last track created.
 */
int
test_plugin_manager_create_tracks_from_plugin (
  const char * pl_bundle,
  const char * pl_uri,
  bool         is_instrument,
  bool         with_carla,
  int          num_tracks);

void
test_plugin_manager_reload_lilv_world_w_path (const char * path)
{
  LilvWorld * world = lilv_world_new ();
  PLUGIN_MANAGER->lilv_world = world;

  lilv_world_load_specifications (world);
  lilv_world_load_plugin_classes (world);
  LilvNode * lv2_path = lilv_new_string (LILV_WORLD, path);
  lilv_world_set_option (world, LILV_OPTION_LV2_PATH, lv2_path);
  lilv_world_load_all (world);
  object_free_w_func_and_null (lilv_node_free, lv2_path);
  plugin_manager_clear_plugins (PLUGIN_MANAGER);
  plugin_manager_scan_plugins (PLUGIN_MANAGER, 1.0, NULL);
}

/**
 * Get a plugin setting clone from the given URI in the given
 * bundle.
 *
 * If non-LV2, the path should be passed to pl_bundle.
 */
PluginSetting *
test_plugin_manager_get_plugin_setting (
  const char * pl_bundle,
  const char * pl_uri,
  bool         with_carla)
{
  if (pl_uri)
    {
      LilvNode * path = lilv_new_uri (LILV_WORLD, pl_bundle);
      g_assert_nonnull (path);
      lilv_world_load_bundle (LILV_WORLD, path);
      lilv_node_free (path);
    }
  else
    {
      char * basename = g_path_get_basename (pl_bundle);
      char * tmpdir = g_dir_make_tmp ("zrythm_vst_XXXXXX", NULL);
      char * dest_path = g_build_filename (tmpdir, basename, NULL);
      if (g_str_has_suffix (pl_bundle, "vst3"))
        {
          g_assert_true (io_copy_dir (dest_path, pl_bundle, true, true, NULL));
        }
      else
        {
          GFile * pl_bundle_file = g_file_new_for_path (pl_bundle);
          GFile * pl_bundle_file_in_tmp = g_file_new_for_path (dest_path);
          g_assert_true (g_file_copy (
            pl_bundle_file, pl_bundle_file_in_tmp, G_FILE_COPY_NONE, NULL, NULL,
            NULL, NULL));
          // g_object_unref (pl_bundle_file);
          // g_object_unref (pl_bundle_file_in_tmp);
        }
      g_setenv ("VST3_PATH", tmpdir, true);
      g_setenv ("VST_PATH", tmpdir, true);
      g_free (tmpdir);
      g_free (dest_path);
      g_free (basename);
    }

  plugin_manager_clear_plugins (PLUGIN_MANAGER);
  plugin_manager_scan_plugins (PLUGIN_MANAGER, 1.0, NULL);
  g_assert_cmpuint (PLUGIN_MANAGER->plugin_descriptors->len, >, 0);

  PluginDescriptor * descr = NULL;
  for (size_t i = 0; i < PLUGIN_MANAGER->plugin_descriptors->len; i++)
    {
      PluginDescriptor * cur_descr =
        g_ptr_array_index (PLUGIN_MANAGER->plugin_descriptors, i);
      if (pl_uri)
        {
          if (string_is_equal (cur_descr->uri, pl_uri))
            {
              descr = plugin_descriptor_clone (cur_descr);
            }
        }
      else if (cur_descr->protocol != Z_PLUGIN_PROTOCOL_LV2)
        {
          char * basename = g_path_get_basename (pl_bundle);
          char * descr_basename = g_path_get_basename (cur_descr->path);
          if (string_is_equal (descr_basename, basename))
            {
              descr = plugin_descriptor_clone (cur_descr);
            }
          g_free (basename);
          g_free (descr_basename);
        }
    }
  g_return_val_if_fail (descr, NULL);

  PluginSetting * setting = plugin_setting_new_default (descr);

  /* always open with carla */
  setting->open_with_carla = true;
#if 0
  /* open with carla if requested */
  setting->open_with_carla = with_carla;
#endif

  plugin_setting_validate (setting, true);

  /* run the logger to avoid too many messages
   * being queued */
  log_idle_cb (LOG);

  return setting;
}

/**
 * Creates \ref num_tracks tracks for the given
 * plugin.
 *
 * @param num_tracks Number of tracks to create.
 *
 * @return The index of the last track created.
 */
int
test_plugin_manager_create_tracks_from_plugin (
  const char * pl_bundle,
  const char * pl_uri,
  bool         is_instrument,
  bool         with_carla,
  int          num_tracks)
{
  PluginSetting * setting =
    test_plugin_manager_get_plugin_setting (pl_bundle, pl_uri, with_carla);
  g_return_val_if_fail (setting, -1);

  TrackType track_type = TRACK_TYPE_AUDIO_BUS;
  if (is_instrument)
    {
      /* fix the descriptor (for some reason lilv
       * reports it as Plugin instead of Instrument if
       * you don't do lilv_world_load_all) */
      setting->descr->category = PC_INSTRUMENT;
      g_free (setting->descr->category_str);
      setting->descr->category_str =
        plugin_descriptor_category_to_string (setting->descr->category);
      track_type = TRACK_TYPE_INSTRUMENT;
    }

  /* create a track from the plugin */
  bool ret = track_create_with_action (
    track_type, setting, NULL, NULL, TRACKLIST->num_tracks, num_tracks, -1,
    NULL, NULL);
  g_assert_true (ret);

  return TRACKLIST->num_tracks - 1;
}

/**
 * @}
 */

#endif
