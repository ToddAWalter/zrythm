/*
 * plugins/plugin_manager.c - Manages plugins
 *
 * Copyright (C) 2018 Alexandros Theodotou
 * Copyright (C) 2008-2012 Paul Davis
 * Copyright (C) David Robillard
 *
 * This file is part of Zrythm
 *
 * Zrythm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zrythm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Zrythm.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <ctype.h>

#include "gui/widgets/main_window.h"
#include "plugins/plugin.h"
#include "plugins/plugin_manager.h"
#include "plugins/lv2_plugin.h"
#include "utils/string.h"
#include "utils/ui.h"
#include "zrythm.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <lv2/lv2plug.in/ns/ext/event/event.h>

/**
 * If category not already set in the categories, add it.
 */
static void
add_category (char * _category)
{
  if (!string_is_ascii (_category))
    {
      g_warning ("Invalid LV2 category name, skipping...");
    }
  for (int i = 0; i < PLUGIN_MANAGER->num_plugin_categories; i++)
    {
      char * category = PLUGIN_MANAGER->plugin_categories[i];
      if (!strcmp (category, _category))
        {
          return;
        }
    }
    PLUGIN_MANAGER->plugin_categories[
      PLUGIN_MANAGER->num_plugin_categories++] =
        g_strdup (_category);
}

int sort_category_func (const void *a, const void *b) {
    char * pa = *(char * const *) a, * pb = *(char * const *) b;
    int r = strcasecmp(pa, pb);
    if (r)
      return r;

    /* if equal ignoring case, use opposite of strcmp() result to get
     * lower before upper */
    return -strcmp(pa, pb); /* aka: return strcmp(b, a); */
}

int sort_plugin_func (const void *a, const void *b) {
    PluginDescriptor * pa = *(PluginDescriptor * const *) a,
                      * pb = *(PluginDescriptor * const *) b;
    int r = strcasecmp(pa->name, pb->name);
    if (r)
      return r;

    /* if equal ignoring case, use opposite of strcmp() result to get
     * lower before upper */
    return -strcmp(pa->name, pb->name); /* aka: return strcmp(b, a); */
}

void
print_plugins ()
{
  for (int i = 0; i < PLUGIN_MANAGER->num_plugins; i++)
    {
      PluginDescriptor * descr =
        PLUGIN_MANAGER->plugin_descriptors[i];

      g_message ("[%d] %s (%s - %s)",
                 i,
                 descr->name,
                 descr->uri,
                 descr->category);
    }
}

/**
 * scans for plugins.
 */
static void
scan_plugins (PluginManager * self)
{
  g_message ("scanning plugins...");

  /* load all plugins with lilv */
  LilvWorld * world = LILV_WORLD;
  const LilvPlugins * plugins =
    lilv_world_get_all_plugins (world);
  /*const LilvPluginClasses * plugin_classes =*/
                              /*lilv_world_get_plugin_classes (world);*/
  LV2_NODES.lilv_plugins = plugins;


  /* iterate plugins */
  LILV_FOREACH(plugins, i, plugins)
    {
      const LilvPlugin* p = lilv_plugins_get(plugins, i);

      PluginDescriptor * descriptor =
        lv2_create_descriptor_from_lilv (p);

      if (descriptor)
        {
          PLUGIN_MANAGER->plugin_descriptors[PLUGIN_MANAGER->num_plugins++] =
            descriptor;
          add_category (descriptor->category);
        }
    }

  /* sort alphabetically */
  qsort (PLUGIN_MANAGER->plugin_descriptors,
         PLUGIN_MANAGER->num_plugins,
         sizeof (Plugin *),
         sort_plugin_func);
  qsort (PLUGIN_MANAGER->plugin_categories,
         PLUGIN_MANAGER->num_plugin_categories,
         sizeof (char *),
         sort_category_func);

  g_message ("%d Plugins scanned.",
             PLUGIN_MANAGER->num_plugins);

  /*print_plugins ();*/
}

/**
 * Initializes plugin manager.
 */
void
plugin_manager_init (PluginManager * self)
{
  g_message ("Initializing plugin manager...");
  self->num_plugins = 0;
  self->num_plugin_categories = 0;

  /* init lv2 settings */
  g_message ("Creating Lilv World...");
  LilvWorld * world = lilv_world_new ();
  self->lv2_nodes.lilv_world = world;

  /* load all installed plugins on system */
  lilv_world_load_all (world);

  /*load bundled plugins */
  GError * err;
  const char * path =
    CONFIGURE_LIBDIR "/zrythm/lv2";
  if (g_file_test (path,
                   G_FILE_TEST_EXISTS |
                   G_FILE_TEST_IS_DIR))
    {
      GDir * bundle_lv2_dir =
        g_dir_open (path, 0, &err);
      if (bundle_lv2_dir)
        {
          const char * dir;
          char * str;
          while ((dir = g_dir_read_name (bundle_lv2_dir)))
            {
              str =
                g_strdup_printf (
                  "file://%s%s%s%smanifest.ttl",
                  path,
                  G_DIR_SEPARATOR_S,
                  dir,
                  G_DIR_SEPARATOR_S);
              LilvNode * uri =
                lilv_new_uri (world, str);
              lilv_world_load_bundle (
                world, uri);
              g_message ("Loaded bundled plugin at %s",
                         str);
              g_free (str);
              lilv_node_free (uri);
            }
        }
      else
        {
          char * msg =
            g_strdup_printf ("%s%s",
            _("Error loading LV2 bundle dir: "),
            err->message);
          ui_show_error_message (
            MAIN_WINDOW ? MAIN_WINDOW : NULL,
            msg);
          g_free (msg);
        }
    }

  g_message ("Initializing LV2 settings...");
  LV2_Defaults * opts = &self->lv2_nodes.opts;
  opts->uuid = NULL;
  opts->buffer_size = 0;
  opts->controls = NULL;
  opts->update_rate = 0.0;
  opts->dump = 1;
  opts->trace = 1;
  opts->generic_ui = 0;
  opts->show_hidden = 1;
  opts->no_menu = 0;
  opts->show_ui = 1;
  opts->print_controls = 1;
  opts->non_interactive = 1;
  /* TODO add option in preferences */
  /*opts->update_rate = 40.f;*/

  /* Cache URIs */
  Lv2Nodes * nodes = &self->lv2_nodes;
#define ADD_LV2_NODE(key,val) \
  nodes->key = lilv_new_uri (world, val);

  /* in alphabetical order */
  ADD_LV2_NODE (
    atom_AtomPort,
    LV2_ATOM__AtomPort);
  ADD_LV2_NODE (
    atom_bufferType,
    LV2_ATOM__bufferType);
  ADD_LV2_NODE (
    atom_Chunk,
    LV2_ATOM__Chunk);
  ADD_LV2_NODE (
    atom_eventTransfer,
    LV2_ATOM__eventTransfer);
  ADD_LV2_NODE (
    atom_Float,
    LV2_ATOM__Float);
  ADD_LV2_NODE (
    atom_Path,
    LV2_ATOM__Path);
  ADD_LV2_NODE (
    atom_Sequence,
    LV2_ATOM__Sequence);
  ADD_LV2_NODE (
    atom_supports,
    LV2_ATOM__supports);
  ADD_LV2_NODE (
    bufz_coarseBlockLength,
    "http://lv2plug.in/ns/ext/buf-size#coarseBlockLength");
  ADD_LV2_NODE (
    bufz_fixedBlockLength,
    LV2_BUF_SIZE__fixedBlockLength);
  ADD_LV2_NODE (
    bufz_powerOf2BlockLength,
    LV2_BUF_SIZE__powerOf2BlockLength);
  ADD_LV2_NODE (
    bufz_nominalBlockLength,
    "http://lv2plug.in/ns/ext/buf-size#nominalBlockLength");
  ADD_LV2_NODE (
    core_AudioPort,
    LV2_CORE__AudioPort);
  ADD_LV2_NODE (
    core_connectionOptional,
    LV2_CORE__connectionOptional);
  ADD_LV2_NODE (
    core_control,
    LV2_CORE__control);
  ADD_LV2_NODE (
    core_ControlPort,
    LV2_CORE__ControlPort);
  ADD_LV2_NODE (
    core_CVPort,
    LV2_CORE__CVPort);
  ADD_LV2_NODE (
    core_default,
    LV2_CORE__default);
  ADD_LV2_NODE (
    core_designation,
    LV2_CORE__designation);
  ADD_LV2_NODE (
    core_enumeration,
    LV2_CORE__enumeration);
  ADD_LV2_NODE (
    core_freeWheeling,
    LV2_CORE__freeWheeling);
  ADD_LV2_NODE (
    core_index,
    LV2_CORE__index);
  ADD_LV2_NODE (
    core_inPlaceBroken,
    LV2_CORE__inPlaceBroken);
  ADD_LV2_NODE (
    core_InputPort,
    LV2_CORE__InputPort);
  ADD_LV2_NODE (
    core_integer,
    LV2_CORE__integer);
  ADD_LV2_NODE (
    core_isSideChain,
    LV2_CORE_PREFIX "isSideChain");
  ADD_LV2_NODE (
    core_maximum,
    LV2_CORE__maximum);
  ADD_LV2_NODE (
    core_minimum,
    LV2_CORE__minimum);
  ADD_LV2_NODE (
    core_name,
    LV2_CORE__name);
  ADD_LV2_NODE (
    core_OutputPort,
    LV2_CORE__OutputPort);
  ADD_LV2_NODE (
    core_reportsLatency,
    LV2_CORE__reportsLatency);
  ADD_LV2_NODE (
    core_sampleRate,
    LV2_CORE__sampleRate);
  ADD_LV2_NODE (
    core_symbol,
    LV2_CORE__symbol);
  ADD_LV2_NODE (
    core_toggled,
    LV2_CORE__toggled);
  ADD_LV2_NODE (
    ev_EventPort,
    LV2_EVENT__EventPort);
  ADD_LV2_NODE (
    patch_Message,
    LV2_PATCH__Message);
  ADD_LV2_NODE (
    patch_writable,
    LV2_PATCH__writable);
  ADD_LV2_NODE (
    midi_MidiEvent,
    LV2_MIDI__MidiEvent);
  ADD_LV2_NODE (
    pg_element,
    LV2_PORT_GROUPS__element);
  ADD_LV2_NODE (
    pg_group,
    LV2_PORT_GROUPS__group);
  ADD_LV2_NODE (
    pprops_causesArtifacts,
    LV2_PORT_PROPS__causesArtifacts);
  ADD_LV2_NODE (
    pprops_expensive,
    LV2_PORT_PROPS__expensive);
  ADD_LV2_NODE (
    pprops_logarithmic,
    LV2_PORT_PROPS__logarithmic);
  ADD_LV2_NODE (
    pprops_notAutomatic,
    LV2_PORT_PROPS__notAutomatic);
  ADD_LV2_NODE (
    pprops_notOnGUI,
    LV2_PORT_PROPS__notOnGUI);
  ADD_LV2_NODE (
    pprops_rangeSteps,
    LV2_PORT_PROPS__rangeSteps);
  ADD_LV2_NODE (
    pset_bank,
    LV2_PRESETS__bank);
  ADD_LV2_NODE (
    pset_Preset,
    LV2_PRESETS__Preset);
  ADD_LV2_NODE (
    rdfs_comment,
    LILV_NS_RDFS "comment");
  ADD_LV2_NODE (
    rdfs_label,
    LILV_NS_RDFS "label");
  ADD_LV2_NODE (
    rdfs_range,
    LILV_NS_RDFS "range");
  ADD_LV2_NODE (
    rsz_minimumSize,
    LV2_RESIZE_PORT__minimumSize);
  ADD_LV2_NODE (
    state_threadSafeRestore,
    LV2_STATE__threadSafeRestore);
  ADD_LV2_NODE (
    time_position,
    LV2_TIME__Position);
  ADD_LV2_NODE (
    ui_external,
    "http://lv2plug.in/ns/extensions/ui#external");
  ADD_LV2_NODE (
    ui_externalkx,
    "http://kxstudio.sf.net/ns/lv2ext/external-ui#Widget");
  ADD_LV2_NODE (
    ui_Gtk3UI,
    LV2_UI__Gtk3UI);
  ADD_LV2_NODE (
    ui_GtkUI,
    LV2_UI__GtkUI);
  ADD_LV2_NODE (
    units_db,
    LV2_UNITS__db);
  ADD_LV2_NODE (
    units_hz,
    LV2_UNITS__hz);
  ADD_LV2_NODE (
    units_midiNote,
    LV2_UNITS__midiNote);
  ADD_LV2_NODE (
    units_render,
    LV2_UNITS__render);
  ADD_LV2_NODE (
    units_unit,
    LV2_UNITS__unit);
  ADD_LV2_NODE (
    work_interface,
    LV2_WORKER__interface);
  ADD_LV2_NODE (
    work_schedule,
    LV2_WORKER__schedule);
#ifdef LV2_EXTENDED
  /*nodes->auto_can_write_automatation = lilv_new_uri(world,LV2_AUTOMATE_URI__can_write);*/
  /*nodes->auto_automation_control     = lilv_new_uri(world,LV2_AUTOMATE_URI__control);*/
  /*nodes->auto_automation_controlled  = lilv_new_uri(world,LV2_AUTOMATE_URI__controlled);*/
  /*nodes->auto_automation_controller  = lilv_new_uri(world,LV2_AUTOMATE_URI__controller);*/
  /*nodes->inline_display_in_gui       = lilv_new_uri(world,LV2_INLINEDISPLAY__in_gui);*/
#endif
  nodes->end = NULL;
#undef ADD_LV2_NODE

}

void
plugin_manager_scan_plugins (PluginManager * self)
{
  scan_plugins (self);
}

void
plugin_manager_free (
  PluginManager * self)
{
}
