// SPDX-FileCopyrightText: © 2021-2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "actions/port_connection_action.h"
#include "actions/tracklist_selections.h"
#include "dsp/tracklist.h"
#include "gui/widgets/main_window.h"
#include "io/serialization/plugin.h"
#include "plugins/plugin_descriptor.h"
#include "plugins/plugin_manager.h"
#include "project.h"
#include "settings/g_settings_manager.h"
#include "settings/plugin_settings.h"
#include "settings/settings.h"
#include "utils/error.h"
#include "utils/file.h"
#include "utils/flags.h"
#include "utils/gtk.h"
#include "utils/objects.h"
#include "utils/string.h"
#include "zrythm.h"
#include "zrythm_app.h"

#include <glib/gi18n.h>

#define PLUGIN_SETTINGS_JSON_TYPE "Zrythm Plugin Settings"
#define PLUGIN_SETTINGS_JSON_FILENAME "plugin-settings.json"
#define PLUGIN_SETTINGS_JSON_FORMAT 6

PluginSetting *
plugin_setting_new_default (const PluginDescriptor * descr)
{
  PluginSetting * existing = NULL;
  if (S_PLUGIN_SETTINGS && !ZRYTHM_TESTING)
    {
      existing = plugin_settings_find (S_PLUGIN_SETTINGS, descr);
    }

  PluginSetting * self = NULL;
  if (existing)
    {
      self = plugin_setting_clone (existing, true);
    }
  else
    {
      self = object_new (PluginSetting);
      self->descr = plugin_descriptor_clone (descr);
      /* bridge all plugins by default */
      self->bridge_mode = CarlaBridgeMode::Full;
      plugin_setting_validate (self, false);
    }

  return self;
}

PluginSetting *
plugin_setting_clone (const PluginSetting * src, bool validate)
{
  PluginSetting * new_setting = object_new (PluginSetting);

  *new_setting = *src;
  new_setting->descr = plugin_descriptor_clone (src->descr);
  new_setting->last_instantiated_time = src->last_instantiated_time;
  new_setting->num_instantiations = src->num_instantiations;

#ifndef HAVE_CARLA
  if (new_setting->open_with_carla)
    {
      g_message ("disabling open_with_carla for %s", new_setting->descr->name);
      new_setting->open_with_carla = false;
    }
#endif

  if (validate)
    {
      plugin_setting_validate (new_setting, false);
    }

  return new_setting;
}

bool
plugin_setting_is_equal (const PluginSetting * a, const PluginSetting * b)
{
  /* TODO */
  g_return_val_if_reached (false);
}

void
plugin_setting_print (const PluginSetting * self)
{
  g_message (
    "[PluginSetting]\n"
    "descr.uri=%s, "
    "open_with_carla=%d, "
    "force_generic_ui=%d, "
    "bridge_mode=%s, "
    "last_instantiated_time=%" G_GINT64_FORMAT
    ", "
    "num_instantiations=%d",
    self->descr->uri, self->open_with_carla, self->force_generic_ui,
    ENUM_NAME (self->bridge_mode), self->last_instantiated_time,
    self->num_instantiations);
}

/**
 * Makes sure the setting is valid in the current
 * run and changes any fields to make it conform.
 *
 * For example, if the setting is set to not open
 * with carla but the descriptor is for a VST plugin,
 * this will set \ref PluginSetting.open_with_carla
 * to true.
 */
void
plugin_setting_validate (PluginSetting * self, bool print_result)
{
  const PluginDescriptor * descr = self->descr;

  /* force carla */
  self->open_with_carla = true;

#ifndef HAVE_CARLA
  if (self->open_with_carla)
    {
      g_critical (
        "Requested to open with carla - carla "
        "functionality is disabled");
      self->open_with_carla = false;
      return;
    }
#endif

  if (
    descr->protocol == ZPluginProtocol::Z_PLUGIN_PROTOCOL_VST
    || descr->protocol == ZPluginProtocol::Z_PLUGIN_PROTOCOL_VST3
    || descr->protocol == ZPluginProtocol::Z_PLUGIN_PROTOCOL_AU
    || descr->protocol == ZPluginProtocol::Z_PLUGIN_PROTOCOL_SFZ
    || descr->protocol == ZPluginProtocol::Z_PLUGIN_PROTOCOL_SF2
    || descr->protocol == ZPluginProtocol::Z_PLUGIN_PROTOCOL_DSSI
    || descr->protocol == ZPluginProtocol::Z_PLUGIN_PROTOCOL_LADSPA
    || descr->protocol == ZPluginProtocol::Z_PLUGIN_PROTOCOL_JSFX
    || descr->protocol == ZPluginProtocol::Z_PLUGIN_PROTOCOL_CLAP)
    {
      self->open_with_carla = true;
#ifndef HAVE_CARLA
      g_critical (
        "Invalid setting requested: open non-LV2 "
        "plugin, carla not installed");
#endif
    }

#if defined(_WIN32) && defined(HAVE_CARLA)
  /* open all LV2 plugins with custom UIs using
   * carla */
  if (descr->has_custom_ui && !self->force_generic_ui)
    {
      self->open_with_carla = true;
    }
#endif

#ifdef HAVE_CARLA
  /* in wayland open all LV2 plugins with custom UIs
   * using carla */
  if (z_gtk_is_wayland () && descr->has_custom_ui)
    {
      self->open_with_carla = true;
    }
#endif

#ifdef HAVE_CARLA
  /* if no bridge mode specified, calculate the bridge mode here */
  /*g_debug ("%s: recalculating bridge mode...", __func__);*/
  if (self->bridge_mode == CarlaBridgeMode::None)
    {
      self->bridge_mode = descr->min_bridge_mode;
      if (self->bridge_mode == CarlaBridgeMode::None)
        {
#  if 0
          /* bridge if plugin is not whitelisted */
          if (!plugin_descriptor_is_whitelisted (descr))
            {
              self->open_with_carla = true;
              self->bridge_mode = CarlaBridgeMode::Full;
              /*g_debug (*/
              /*"plugin descriptor not whitelisted - will bridge full");*/
            }
#  endif
        }
      else
        {
          self->open_with_carla = true;
        }
    }
  else
    {
      self->open_with_carla = true;
      CarlaBridgeMode mode = descr->min_bridge_mode;

      if (mode == CarlaBridgeMode::Full)
        {
          self->bridge_mode = mode;
        }
    }

#  if 0
  /* force bridge mode */
  if (self->bridge_mode == CarlaBridgeMode::None)
    {
      self->bridge_mode = CarlaBridgeMode::Full;
    }
#  endif

    /*g_debug ("done recalculating bridge mode");*/
#endif

  /* if no custom UI, force generic */
  /*g_debug ("checking if plugin has custom UI...");*/
  if (!descr->has_custom_ui)
    {
      /*g_debug ("plugin %s has no custom UI", descr->name);*/
      self->force_generic_ui = true;
    }
  /*g_debug ("done checking if plugin has custom UI");*/

  if (print_result)
    {
      g_debug ("plugin setting validated. new setting:");
      plugin_setting_print (self);
    }
}

/**
 * Finish activating a plugin setting.
 */
static void
activate_finish (
  const PluginSetting * self,
  bool                  autoroute_multiout,
  bool                  has_stereo_outputs)
{
  bool has_errors = false;

  TrackType type = track_get_type_from_plugin_descriptor (self->descr);

  /* stop the engine so it doesn't restart all the time
   * until all the actions are performed */
  EngineState state;
  engine_wait_for_pause (AUDIO_ENGINE, &state, Z_F_NO_FORCE, true);

  if (autoroute_multiout)
    {
      int num_pairs = self->descr->num_audio_outs;
      if (has_stereo_outputs)
        num_pairs = num_pairs / 2;
      int num_actions = 0;

      /* create group */
      GError * err = NULL;
      bool     ret = track_create_empty_with_action (
        TrackType::TRACK_TYPE_AUDIO_GROUP, &err);
      if (!ret)
        {
          HANDLE_ERROR (err, "%s", _ ("Failed to create track"));
          has_errors = true;
        }
      num_actions++;

      Track * group = TRACKLIST->tracks[TRACKLIST->tracks.size () - 1];

      /* create the plugin track */
      err = NULL;
      ret = track_create_for_plugin_at_idx_w_action (
        type, self, TRACKLIST->tracks.size (), &err);
      if (!ret)
        {
          HANDLE_ERROR (err, "%s", _ ("Failed to create track"));
          has_errors = true;
        }
      num_actions++;

      Track * pl_track = TRACKLIST->tracks[TRACKLIST->tracks.size () - 1];

      Plugin * pl = pl_track->channel->instrument;

      /* move the plugin track inside the group */
      track_select (pl_track, F_SELECT, F_EXCLUSIVE, F_NO_PUBLISH_EVENTS);
      err = NULL;
      ret = tracklist_selections_action_perform_move_inside (
        TRACKLIST_SELECTIONS, PORT_CONNECTIONS_MGR, group->pos, &err);
      if (!ret)
        {
          HANDLE_ERROR (err, "%s", _ ("Failed to move track"));
          has_errors = true;
        }
      num_actions++;

      /* route to nowhere */
      track_select (pl_track, F_SELECT, F_EXCLUSIVE, F_NO_PUBLISH_EVENTS);
      err = NULL;
      ret = tracklist_selections_action_perform_set_direct_out (
        TRACKLIST_SELECTIONS, PORT_CONNECTIONS_MGR, NULL, &err);
      if (!ret)
        {
          HANDLE_ERROR (err, "%s", _ ("Failed to set direct out"));
          has_errors = true;
        }
      num_actions++;

      /* rename group */
      char name[1200];
      sprintf (name, _ ("%s Output"), self->descr->name);
      err = NULL;
      ret = tracklist_selections_action_perform_edit_rename (
        group, PORT_CONNECTIONS_MGR, name, &err);
      if (!ret)
        {
          HANDLE_ERROR (err, "%s", _ ("Failed to rename track"));
          has_errors = true;
        }
      num_actions++;

      GPtrArray * pl_audio_outs = g_ptr_array_new ();
      for (int j = 0; j < pl->num_out_ports; j++)
        {
          Port * cur_port = pl->out_ports[j];
          if (cur_port->id_.type_ != PortType::Audio)
            continue;

          g_ptr_array_add (pl_audio_outs, cur_port);
        }

      for (int i = 0; i < num_pairs; i++)
        {
          /* create the audio fx track */
          err = NULL;
          ret = track_create_empty_with_action (
            TrackType::TRACK_TYPE_AUDIO_BUS, &err);
          if (!ret)
            {
              HANDLE_ERROR (err, "%s", _ ("Failed to create track"));
              has_errors = true;
            }
          num_actions++;

          Track * fx_track = TRACKLIST->tracks[TRACKLIST->tracks.size () - 1];

          /* rename fx track */
          sprintf (name, _ ("%s %d"), self->descr->name, i + 1);
          err = NULL;
          ret = tracklist_selections_action_perform_edit_rename (
            fx_track, PORT_CONNECTIONS_MGR, name, &err);
          if (!ret)
            {
              HANDLE_ERROR (err, "%s", _ ("Failed to rename track"));
              has_errors = true;
            }
          num_actions++;

          /* move the fx track inside the group */
          track_select (fx_track, F_SELECT, F_EXCLUSIVE, F_NO_PUBLISH_EVENTS);
          err = NULL;
          ret = tracklist_selections_action_perform_move_inside (
            TRACKLIST_SELECTIONS, PORT_CONNECTIONS_MGR, group->pos, &err);
          if (!ret)
            {
              HANDLE_ERROR (err, "%s", _ ("Failed to move track"));
              has_errors = true;
            }
          num_actions++;

          /* move the fx track to the end */
          track_select (fx_track, F_SELECT, F_EXCLUSIVE, F_NO_PUBLISH_EVENTS);
          err = NULL;
          ret = tracklist_selections_action_perform_move (
            TRACKLIST_SELECTIONS, PORT_CONNECTIONS_MGR,
            TRACKLIST->tracks.size (), &err);
          if (!ret)
            {
              HANDLE_ERROR (err, "%s", _ ("Failed to move track"));
              has_errors = true;
            }
          num_actions++;

          /* route to group */
          track_select (fx_track, F_SELECT, F_EXCLUSIVE, F_NO_PUBLISH_EVENTS);
          err = NULL;
          ret = tracklist_selections_action_perform_set_direct_out (
            TRACKLIST_SELECTIONS, PORT_CONNECTIONS_MGR, group, &err);
          if (!ret)
            {
              HANDLE_ERROR (err, "%s", _ ("Failed to set direct out"));
              has_errors = true;
            }
          num_actions++;

          int    l_index = has_stereo_outputs ? i * 2 : i;
          Port * port = (Port *) g_ptr_array_index (pl_audio_outs, l_index);

          /* route left port to audio fx */
          err = NULL;
          ret = port_connection_action_perform_connect (
            &port->id_, &fx_track->processor->stereo_in->get_l ().id_, &err);
          if (!ret)
            {
              HANDLE_ERROR (err, "%s", _ ("Failed to connect L port"));
              has_errors = true;
            }
          num_actions++;

          int r_index = has_stereo_outputs ? i * 2 + 1 : i;
          port = (Port *) g_ptr_array_index (pl_audio_outs, r_index);

          /* route right port to audio fx */
          err = NULL;
          ret = port_connection_action_perform_connect (
            &port->id_, &fx_track->processor->stereo_in->get_r ().id_, &err);
          if (!ret)
            {
              HANDLE_ERROR (err, "%s", _ ("Failed to connect R port"));
              has_errors = true;
            }
          num_actions++;
        }

      g_ptr_array_unref (pl_audio_outs);

      UndoableAction * ua = undo_manager_get_last_action (UNDO_MANAGER);
      ua->num_actions = num_actions;
    }
  else /* else if not autoroute multiout */
    {
      GError * err = NULL;
      bool     ret = track_create_for_plugin_at_idx_w_action (
        type, self, TRACKLIST->tracks.size (), &err);
      if (!ret)
        {
          HANDLE_ERROR (err, "%s", _ ("Failed to create track"));
          has_errors = true;
        }
    }

  engine_resume (AUDIO_ENGINE, &state);

  if (!has_errors)
    {
      PluginSetting * setting_clone = plugin_setting_clone (self, F_NO_VALIDATE);
      plugin_setting_increment_num_instantiations (setting_clone);
      plugin_setting_free (setting_clone);
    }
}

static void
on_outputs_stereo_response (
  AdwMessageDialog * dialog,
  char *             response,
  PluginSetting *    self)
{
  if (string_is_equal (response, "close"))
    return;

  bool stereo = string_is_equal (response, "yes");
  activate_finish (self, true, stereo);
}

static void
on_contains_multiple_outputs_response (
  AdwMessageDialog * dialog,
  char *             response,
  PluginSetting *    self)
{
  if (string_is_equal (response, "yes"))
    {
      GtkWidget * stereo_dialog = adw_message_dialog_new (
        GTK_WINDOW (MAIN_WINDOW), _ ("Stereo?"), _ ("Are the outputs stereo?"));
      gtk_window_set_modal (GTK_WINDOW (stereo_dialog), true);
      adw_message_dialog_add_responses (
        ADW_MESSAGE_DIALOG (stereo_dialog), "yes", _ ("_Yes"), "no", _ ("_No"),
        NULL);
      adw_message_dialog_set_default_response (
        ADW_MESSAGE_DIALOG (stereo_dialog), "yes");
      PluginSetting * setting_clone = plugin_setting_clone (self, false);
      g_signal_connect_data (
        stereo_dialog, "response", G_CALLBACK (on_outputs_stereo_response),
        setting_clone, plugin_setting_free_closure, (GConnectFlags) 0);
      gtk_window_present (GTK_WINDOW (stereo_dialog));
    }
  else if (string_is_equal (response, "no"))
    {
      activate_finish (self, false, false);
    }
  else
    {
      /* do nothing */
    }
}

/**
 * Creates necessary tracks at the end of the tracklist.
 *
 * This may happen asynchronously so the caller should not
 * expect the setting to be activated on return.
 */
void
plugin_setting_activate (const PluginSetting * self)
{
  TrackType type = track_get_type_from_plugin_descriptor (self->descr);

  if (
    self->descr->num_audio_outs > 2 && type == TrackType::TRACK_TYPE_INSTRUMENT)
    {
      AdwMessageDialog * dialog = ADW_MESSAGE_DIALOG (adw_message_dialog_new (
        GTK_WINDOW (MAIN_WINDOW), _ ("Auto-route?"),
        _ (
          "This plugin contains multiple audio outputs. Would you like to auto-route each output to a separate FX track?")));
      adw_message_dialog_add_responses (
        dialog, "cancel", _ ("_Cancel"), "no", _ ("_No"), "yes", _ ("_Yes"),
        NULL);
      adw_message_dialog_set_close_response (dialog, "cancel");
      adw_message_dialog_set_response_appearance (
        dialog, "yes", ADW_RESPONSE_SUGGESTED);
      PluginSetting * setting_clone = plugin_setting_clone (self, false);
      g_signal_connect_data (
        dialog, "response", G_CALLBACK (on_contains_multiple_outputs_response),
        setting_clone, plugin_setting_free_closure, (GConnectFlags) 0);
      gtk_window_present (GTK_WINDOW (dialog));
    }
  else
    {
      activate_finish (self, false, false);
    }
}

/**
 * Increments the number of times this plugin has been
 * instantiated.
 *
 * @note This also serializes all plugin settings.
 */
void
plugin_setting_increment_num_instantiations (PluginSetting * self)
{
  self->last_instantiated_time = g_get_real_time ();
  self->num_instantiations++;

  plugin_settings_set (S_PLUGIN_SETTINGS, self, true);
}

/**
 * Frees the plugin setting.
 */
void
plugin_setting_free (PluginSetting * self)
{
  object_free_w_func_and_null (plugin_descriptor_free, self->descr);

  object_zero_and_free (self);
}

void
plugin_setting_free_closure (void * self, GClosure * closure)
{
  plugin_setting_free ((PluginSetting *) self);
}

static PluginSettings *
create_new (void)
{
  PluginSettings * self = object_new (PluginSettings);
  self->settings =
    g_ptr_array_new_full (10, (GDestroyNotify) plugin_setting_free);
  return self;
}

static char *
get_plugin_settings_file_path (void)
{
  auto * dir_mgr = ZrythmDirectoryManager::getInstance ();
  char * zrythm_dir = dir_mgr->get_dir (USER_TOP);
  g_return_val_if_fail (zrythm_dir, NULL);

  return g_build_filename (zrythm_dir, PLUGIN_SETTINGS_JSON_FILENAME, NULL);
}

static char *
serialize_to_json_str (const PluginSettings * self, GError ** error)
{
  /* create a mutable doc */
  yyjson_mut_doc * doc = yyjson_mut_doc_new (NULL);
  yyjson_mut_val * root = yyjson_mut_obj (doc);
  if (!root)
    {
      g_set_error_literal (error, 0, 0, "Failed to create root obj");
      return NULL;
    }
  yyjson_mut_doc_set_root (doc, root);

  yyjson_mut_obj_add_str (doc, root, "type", PLUGIN_SETTINGS_JSON_TYPE);
  yyjson_mut_obj_add_int (doc, root, "format", PLUGIN_SETTINGS_JSON_FORMAT);
  yyjson_mut_val * settings_arr = yyjson_mut_obj_add_arr (doc, root, "settings");
  for (size_t i = 0; i < self->settings->len; i++)
    {
      const PluginSetting * setting =
        (PluginSetting *) g_ptr_array_index (self->settings, i);
      yyjson_mut_val * setting_obj = yyjson_mut_arr_add_obj (doc, settings_arr);
      plugin_setting_serialize_to_json (doc, setting_obj, setting, error);
    }

  char * json = yyjson_mut_write (doc, YYJSON_WRITE_PRETTY_TWO_SPACES, NULL);
  g_message ("done writing json to string");

  yyjson_mut_doc_free (doc);

  return json;
}

static PluginSettings *
deserialize_from_json_str (const char * json, GError ** error)
{
  yyjson_doc * doc =
    yyjson_read_opts ((char *) json, strlen (json), 0, NULL, NULL);
  yyjson_val * root = yyjson_doc_get_root (doc);
  if (!root)
    {
      g_set_error_literal (error, 0, 0, "Failed to create root obj");
      return NULL;
    }

  yyjson_obj_iter it = yyjson_obj_iter_with (root);
  if (!yyjson_equals_str (
        yyjson_obj_iter_get (&it, "type"), PLUGIN_SETTINGS_JSON_TYPE))
    {
      g_set_error_literal (error, 0, 0, "Not a valid plugin collections file");
      return NULL;
    }

  int format = yyjson_get_int (yyjson_obj_iter_get (&it, "format"));
  if (format != PLUGIN_SETTINGS_JSON_FORMAT)
    {
      g_set_error_literal (error, 0, 0, "Invalid version");
      return NULL;
    }

  PluginSettings * self = create_new ();

  yyjson_val *    settings_arr = yyjson_obj_iter_get (&it, "settings");
  yyjson_arr_iter settings_it = yyjson_arr_iter_with (settings_arr);
  yyjson_val *    setting_obj = NULL;
  while ((setting_obj = yyjson_arr_iter_next (&settings_it)))
    {
      PluginSetting * setting = object_new (PluginSetting);
      g_ptr_array_add (self->settings, setting);
      plugin_setting_deserialize_from_json (doc, setting_obj, setting, error);
    }

  yyjson_doc_free (doc);

  return self;
}

void
plugin_settings_serialize_to_file (PluginSettings * self)
{
  g_message ("Serializing plugin settings...");
  GError * err = NULL;
  char *   json = serialize_to_json_str (self, &err);
  if (!json)
    {
      HANDLE_ERROR_LITERAL (err, _ ("Failed to serialize plugin settings"));
      return;
    }
  err = NULL;
  char * path = get_plugin_settings_file_path ();
  g_return_if_fail (path && strlen (path) > 2);
  g_message ("Writing plugin settings to %s...", path);
  bool success = g_file_set_contents (path, json, -1, &err);
  if (!success)
    {
      HANDLE_ERROR_LITERAL (err, _ ("Failed to write plugin settings file"));
      g_free (path);
      g_free (json);
      return;
    }
  g_free (path);
  g_free (json);
}

/**
 * Reads the file and fills up the object.
 */
PluginSettings *
plugin_settings_read_or_new (void)
{
  GError * err = NULL;
  char *   path = get_plugin_settings_file_path ();
  if (!file_exists (path))
    {
      g_message ("Plugin settings file at %s does not exist", path);
return_new_instance:
      g_free (path);
      PluginSettings * self = create_new ();
      return self;
    }
  char * json = NULL;
  g_file_get_contents (path, &json, NULL, &err);
  if (err != NULL)
    {
      g_critical ("Failed to create PluginSettings from %s", path);
      g_error_free (err);
      g_free (json);
      g_free (path);
      return NULL;
    }

  PluginSettings * self = deserialize_from_json_str (json, &err);
  if (!self)
    {
      g_message (
        "Found invalid plugin settings file (error: %s). "
        "Purging file and creating a new one.",
        err->message);
      g_error_free (err);
      GFile * file = g_file_new_for_path (path);
      g_file_delete (file, NULL, NULL);
      g_object_unref (file);
      g_free (json);
      goto return_new_instance;
    }
  g_free (json);
  g_free (path);

  return self;
}

/**
 * Finds a setting for the given plugin descriptor.
 *
 * @return The found setting or NULL.
 */
PluginSetting *
plugin_settings_find (PluginSettings * self, const PluginDescriptor * descr)
{
  for (size_t i = 0; i < self->settings->len; i++)
    {
      PluginSetting * cur_setting =
        (PluginSetting *) g_ptr_array_index (self->settings, i);
      PluginDescriptor * cur_descr = cur_setting->descr;
      if (plugin_descriptor_is_same_plugin (cur_descr, descr))
        {
          return cur_setting;
        }
    }

  return NULL;
}

/**
 * Replaces a setting or appends a setting to the
 * cache.
 *
 * This clones the setting before adding it.
 *
 * @param serialize Whether to serialize the updated
 *   cache now.
 */
void
plugin_settings_set (
  PluginSettings * self,
  PluginSetting *  setting,
  bool             _serialize)
{
  g_message ("Saving plugin setting");

  PluginSetting * own_setting =
    plugin_settings_find (S_PLUGIN_SETTINGS, setting->descr);

  if (own_setting)
    {
      own_setting->force_generic_ui = setting->force_generic_ui;
      own_setting->open_with_carla = setting->open_with_carla;
      own_setting->bridge_mode = setting->bridge_mode;
      own_setting->last_instantiated_time = setting->last_instantiated_time;
      own_setting->num_instantiations = setting->num_instantiations;
    }
  else
    {
      g_ptr_array_add (
        self->settings, plugin_setting_clone (setting, F_VALIDATE));
    }

  if (_serialize)
    {
      plugin_settings_serialize_to_file (self);
    }
}

void
plugin_settings_free (PluginSettings * self)
{
  object_free_w_func_and_null (g_ptr_array_unref, self->settings);

  object_zero_and_free (self);
}
