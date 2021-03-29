/*
 * Copyright (C) 2020 Alexandros Theodotou <alex at zrythm dot org>
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
 * Plugin strip expander widget.
 */

#ifndef __GUI_WIDGETS_PLUGIN_STRIP_EXPANDER_H__
#define __GUI_WIDGETS_PLUGIN_STRIP_EXPANDER_H__

#include "gui/widgets/expander_box.h"

#include <gtk/gtk.h>

#define PLUGIN_STRIP_EXPANDER_WIDGET_TYPE \
  (plugin_strip_expander_widget_get_type ())
G_DECLARE_FINAL_TYPE (
  PluginStripExpanderWidget,
  plugin_strip_expander_widget,
  Z, PLUGIN_STRIP_EXPANDER_WIDGET,
  ExpanderBoxWidget);

typedef struct Track Track;
typedef struct _ChannelSlotWidget ChannelSlotWidget;

/**
 * @addtogroup widgets
 *
 * @{
 */

typedef enum PluginStripExpanderPosition
{
  PSE_POSITION_CHANNEL,
  PSE_POSITION_INSPECTOR,
} PluginStripExpanderPosition;

/**
 * A TwoColExpanderBoxWidget for showing the ports
 * in the InspectorWidget.
 */
typedef struct _PluginStripExpanderWidget
{
  ExpanderBoxWidget parent_instance;

  PluginSlotType    slot_type;
  PluginStripExpanderPosition position;

  /** Scrolled window for the vbox inside. */
  GtkScrolledWindow * scroll;
  GtkViewport *     viewport;

  /** VBox containing each slot. */
  GtkBox *          box;

  /** 1 box for each item. */
  GtkBox *          strip_boxes[STRIP_SIZE];

  /** Channel slots, if type is inserts. */
  ChannelSlotWidget * slots[STRIP_SIZE];

  /** Owner track. */
  Track *           track;
} PluginStripExpanderWidget;

/**
 * Queues a redraw of the given slot.
 */
void
plugin_strip_expander_widget_redraw_slot (
  PluginStripExpanderWidget * self,
  int                         slot);

/**
 * Unsets state flags and redraws the widget at the
 * given slot.
 *
 * @param slot Slot, or -1 for all slots.
 * @param set True to set, false to unset.
 */
void
plugin_strip_expander_widget_set_state_flags (
  PluginStripExpanderWidget * self,
  int                         slot,
  GtkStateFlags               flags,
  bool                        set);

/**
 * Refreshes each field.
 */
void
plugin_strip_expander_widget_refresh (
  PluginStripExpanderWidget * self);

/**
 * Sets up the PluginStripExpanderWidget.
 */
void
plugin_strip_expander_widget_setup (
  PluginStripExpanderWidget * self,
  PluginSlotType              type,
  PluginStripExpanderPosition position,
  Track *                     track);

/**
 * @}
 */

#endif
