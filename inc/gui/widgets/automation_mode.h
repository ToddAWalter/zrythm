// clang-format off
// SPDX-FileCopyrightText: © 2019-2020, 2022 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense
// clang-format on

/**
 * \file
 *
 * Automation mode.
 */

#ifndef __GUI_WIDGETS_AUTOMATION_MODE_H__
#define __GUI_WIDGETS_AUTOMATION_MODE_H__

#include "dsp/automation_track.h"
#include "gui/widgets/custom_button.h"

#include <gtk/gtk.h>

typedef struct AutomationTrack AutomationTrack;

/**
 * @addtogroup widgets
 *
 * @{
 */

#define AUTOMATION_MODE_HPADDING 3
#define AUTOMATION_MODE_HSEPARATOR_SIZE 1

/**
 * Custom button group to be drawn inside drawing
 * areas.
 */
typedef struct AutomationModeWidget
{
  /** X/y relative to parent drawing area. */
  double x;
  double y;

  /** Total width/height. */
  int width;
  int height;

  /** Width of each button, including padding. */

  int text_widths[static_cast<int> (AutomationMode::NUM_AUTOMATION_MODES)];
  int text_heights[static_cast<int> (AutomationMode::NUM_AUTOMATION_MODES)];
  int max_text_height;

  int has_hit_mode;

  /** Currently hit mode. */
  AutomationMode hit_mode;

  /** Default color. */
  GdkRGBA def_color;

  /** Hovered color. */
  GdkRGBA hovered_color;

  /** Toggled color. */
  GdkRGBA toggled_colors[static_cast<int> (AutomationMode::NUM_AUTOMATION_MODES)];

  /** Held color (used after clicking and before
   * releasing). */
  GdkRGBA held_colors[static_cast<int> (AutomationMode::NUM_AUTOMATION_MODES)];

  /** Aspect ratio for the rounded rectangle. */
  double aspect;

  /** Corner curvature radius for the rounded
   * rectangle. */
  double corner_radius;

  /** Used to update caches if state changed. */
  CustomButtonWidgetState
    last_states[static_cast<int> (AutomationMode::NUM_AUTOMATION_MODES)];

  /** Used during drawing. */
  CustomButtonWidgetState
    current_states[static_cast<int> (AutomationMode::NUM_AUTOMATION_MODES)];

  /** Used during transitions. */
  GdkRGBA last_colors[static_cast<int> (AutomationMode::NUM_AUTOMATION_MODES)];

  /** Cache layout for drawing text. */
  PangoLayout * layout;

  /** Owner. */
  AutomationTrack * owner;

  /** Frames left for a transition in color. */
  int transition_frames;

} AutomationModeWidget;

/**
 * Creates a new track widget from the given track.
 */
AutomationModeWidget *
automation_mode_widget_new (
  int               height,
  PangoLayout *     layout,
  AutomationTrack * owner);

void
automation_mode_widget_init (AutomationModeWidget * self);

void
automation_mode_widget_draw (
  AutomationModeWidget *  self,
  GtkSnapshot *           snapshot,
  double                  x,
  double                  y,
  double                  x_cursor,
  CustomButtonWidgetState state);

void
automation_mode_widget_free (AutomationModeWidget * self);

/**
 * @}
 */

#endif
