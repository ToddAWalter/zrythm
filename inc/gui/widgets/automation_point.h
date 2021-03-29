/*
 * Copyright (C) 2018-2020 Alexandros Theodotou <alex at zrythm dot org>
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
 * Automation Point.
 */

#ifndef __GUI_WIDGETS_AUTOMATION_POINT_H__
#define __GUI_WIDGETS_AUTOMATION_POINT_H__

#include "audio/automation_point.h"
#include "utils/ui.h"

#include <gtk/gtk.h>

/**
 * @addtogroup widgets
 *
 * @{
 */

#define AP_WIDGET_POINT_SIZE 6

/**
 * Returns if the automation point (circle) is hit.
 *
 * This function assumes that the point is already
 * inside the full rect of the automation point.
 *
 * @param x X, or -1 to not check x.
 * @param y Y, or -1 to not check y.
 *
 * @note the transient is also checked.
 */
bool
automation_point_is_point_hit (
  AutomationPoint * self,
  double            x,
  double            y);

/**
 * Returns if the automation curve is hit.
 *
 * This function assumes that the point is already
 * inside the full rect of the automation point.
 *
 * @param x X in global coordinates.
 * @param y Y in global coordinates.
 * @param delta_from_curve Allowed distance from the
 *   curve.
 *
 * @note the transient is also checked.
 */
bool
automation_point_is_curve_hit (
  AutomationPoint * self,
  double            x,
  double            y,
  double            delta_from_curve);

/**
 * Draws the AutomationPoint in the given cairo
 * context in absolute coordinates.
 *
 * @param cr The cairo context of the arranger.
 * @param rect Arranger rectangle.
 * @param layout Pango layout to draw text with.
 */
void
automation_point_draw (
  AutomationPoint * ap,
  cairo_t *         cr,
  GdkRectangle *    rect,
  PangoLayout *     layout);

/**
 * @}
 */

#endif
