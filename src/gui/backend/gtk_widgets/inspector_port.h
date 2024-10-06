// SPDX-FileCopyrightText: © 2019-2021, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

/**
 * @file
 *
 * Inspector port widget.
 */

#ifndef __GUI_WIDGETS_INSPECTOR_PORT_H__
#define __GUI_WIDGETS_INSPECTOR_PORT_H__

#include "common/utils/types.h"
#include "gui/backend/gtk_widgets/gtk_wrapper.h"

#define INSPECTOR_PORT_WIDGET_TYPE (inspector_port_widget_get_type ())
G_DECLARE_FINAL_TYPE (
  InspectorPortWidget,
  inspector_port_widget,
  Z,
  INSPECTOR_PORT_WIDGET,
  GtkWidget)

TYPEDEF_STRUCT_UNDERSCORED (BarSliderWidget);
TYPEDEF_STRUCT_UNDERSCORED (PortConnectionsPopoverWidget);
class Meter;
class Port;

/**
 * @addtogroup widgets
 *
 * @{
 */

/**
 * A Port to show in the inspector for Plugin's.
 */
using InspectorPortWidget = struct _InspectorPortWidget
{
  GtkWidget parent_instance;

  GtkOverlay * overlay;

  /** The bar slider. */
  BarSliderWidget * bar_slider;

  /** Last MIDI event trigger time, for MIDI ports. */
  gint64 last_midi_trigger_time;

  /**
   * Last time the tooltip changed.
   *
   * Used to avoid excessive updating of the
   * tooltip text.
   */
  gint64 last_tooltip_change;

  /** Caches from the port. */
  float minf;
  float maxf;
  float zerof_;

  /** Normalized value at the start of an action. */
  float normalized_init_port_val;

  /** Port name cache. */
  std::string port_str;

  /** Port this is for. */
  Port * port;

  /**
   * Caches of last real value and its corresponding
   * normalized value.
   *
   * This is used to avoid re-calculating a normalized value
   * for the same real value.
   *
   * TODO move this optimization on Port struct.
   */
  float last_real_val;
  float last_normalized_val;
  bool  last_port_val_set;

  /** Meter for this widget. */
  std::unique_ptr<Meter> meter;

  /** Jack button to expose port to jack. */
  GtkToggleButton * jack;

  /** MIDI button to select MIDI CC sources. */
  GtkToggleButton * midi;

  /** Multipress guesture for double click. */
  GtkGestureClick * double_click_gesture;

  /** Multipress guesture for right click. */
  GtkGestureClick * right_click_gesture;

  std::string hex_color;

  /** Cache of port's last drawn number of
   * connetions (srcs or dests). */
  int last_num_connections;

  /** Popover to be reused for context menus. */
  GtkPopoverMenu * popover_menu;

  PortConnectionsPopoverWidget * connections_popover;
};

void
inspector_port_widget_refresh (InspectorPortWidget * self);

/**
 * Creates a new widget.
 */
InspectorPortWidget *
inspector_port_widget_new (Port * port);

/**
 * @}
 */

#endif
