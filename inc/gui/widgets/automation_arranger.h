// clang-format off
// SPDX-FileCopyrightText: © 2018-2020, 2022 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense
// clang-format on

/**
 * \file
 *
 * Automation arranger API.
 */

#ifndef __GUI_WIDGETS_AUTOMATION_ARRANGER_H__
#define __GUI_WIDGETS_AUTOMATION_ARRANGER_H__

#include "dsp/position.h"
#include "gui/backend/automation_selections.h"
#include "gui/backend/tool.h"
#include "gui/widgets/arranger.h"
#include "gui/widgets/main_window.h"

#include "gtk_wrapper.h"

TYPEDEF_STRUCT (AutomationPoint);
TYPEDEF_STRUCT (AutomationCurve);
TYPEDEF_STRUCT_UNDERSCORED (AutomationPointWidget);
TYPEDEF_STRUCT_UNDERSCORED (AutomationCurveWidget);
TYPEDEF_STRUCT (SnapGrid);
TYPEDEF_STRUCT (AutomationTrack);
TYPEDEF_STRUCT_UNDERSCORED (RegionWidget);

/**
 * @addtogroup widgets
 *
 * @{
 */

#define MW_AUTOMATION_ARRANGER MW_AUTOMATION_EDITOR_SPACE->arranger

/** Padding to leave before and after the usable
 * vertical range for automation. */
#define AUTOMATION_ARRANGER_VPADDING 4

/**
 * Create an AutomationPointat the given Position
 * in the given Track's AutomationTrack.
 *
 * @param pos The pre-snapped position.
 */
void
automation_arranger_widget_create_ap (
  ArrangerWidget * self,
  const Position * pos,
  const double     start_y,
  Region *         region,
  bool             autofilling);

/**
 * Change curviness of selected curves.
 */
void
automation_arranger_widget_resize_curves (ArrangerWidget * self, double offset_y);

/**
 * Generate a context menu at x, y.
 *
 * @param menu A menu to append entries to (optional).
 *
 * @return The given updated menu or a new menu.
 */
GMenu *
automation_arranger_widget_gen_context_menu (
  ArrangerWidget * self,
  GMenu *          menu,
  double           x,
  double           y);

/**
 * Called when using the edit tool.
 *
 * @return Whether an automation point was moved.
 */
bool
automation_arranger_move_hit_aps (ArrangerWidget * self, double x, double y);

/**
 * @}
 */

#endif
