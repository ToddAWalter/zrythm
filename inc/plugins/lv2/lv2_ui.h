/*
 * Copyright (C) 2018-2021 Alexandros Theodotou <alex at zrythm dot org>
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

/**
 * \file
 *
 * LV2 UI related code.
 */

#ifndef __PLUGINS_LV2_LV2_UI_H__
#define __PLUGINS_LV2_LV2_UI_H__

#include <stdbool.h>

#include <suil/suil.h>

typedef struct Lv2Plugin Lv2Plugin;

/**
 * @addtogroup lv2
 *
 * @{
 */

/**
 * Returns if the UI of the plugin is resizable.
 */
bool
lv2_ui_is_resizable (
  Lv2Plugin* plugin);

/**
 * Inits the LV2 plugin UI.
 *
 * To be called for generic, suil-wrapped and
 * external UIs.
 */
void
lv2_ui_init (
  Lv2Plugin* plugin);

/**
 * Instantiates the plugin UI.
 */
void
lv2_ui_instantiate (
  Lv2Plugin *  plugin,
  const char * native_ui_type,
  void*        parent);

/**
 * Read and apply control change events from UI,
 * for plugins that have their own UIs.
 *
 * Called in the real-time audio thread during
 * plugin processing.
 *
 * @param nframes Used for event ports.
 */
void
lv2_ui_read_and_apply_events (
  Lv2Plugin * plugin,
  uint32_t nframes);

/**
 * Write events from the plugin's UI to the plugin.
 */
void
lv2_ui_send_event_from_ui_to_plugin (
  Lv2Plugin *    plugin,
  uint32_t       port_index,
  uint32_t       buffer_size,
  uint32_t       protocol, ///< format
  const void*    buffer);

/**
 * Send event to UI, called during the real time
 * audio thread when processing the plugin.
 *
 * @param type Atom type.
 */
int
lv2_ui_send_event_from_plugin_to_ui (
  Lv2Plugin *  plugin,
  uint32_t     port_index,
  uint32_t     type,
  uint32_t     size,
  const void * body);

/**
 * Similar to lv2_ui_send_event_from_plugin_to_ui
 * except that it passes a float instead of an
 * LV2 atom.
 *
 * @param lv2_port The port to pass the value of.
 */
NONNULL
void
lv2_ui_send_control_val_event_from_plugin_to_ui (
  Lv2Plugin *  lv2_plugin,
  Port *       port);

/**
 * @}
 */

#endif
