/*
 * Copyright (C) 2018-2021 Alexandros Theodotou <alex at zrythm dot org>
 *
 * This file is part of Zrythm
 *
 * Zrythm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zrythm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Zrythm.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "audio/automation_region.h"
#include "audio/automation_track.h"
#include "audio/audio_bus_track.h"
#include "audio/channel.h"
#include "audio/instrument_track.h"
#include "audio/track.h"
#include "gui/widgets/arranger.h"
#include "gui/widgets/bot_bar.h"
#include "gui/widgets/main_window.h"
#include "gui/widgets/automation_point.h"
#include "gui/widgets/ruler.h"
#include "project.h"
#include "settings/settings.h"
#include "utils/cairo.h"
#include "utils/math.h"
#include "utils/ui.h"
#include "zrythm_app.h"

/**
 * Draws the AutomationPoint in the given cairo
 * context in absolute coordinates.
 *
 * @param rect Arranger rectangle.
 * @param layout Pango layout to draw text with.
 */
void
automation_point_draw (
  AutomationPoint * ap,
  GtkSnapshot *     snapshot,
  GdkRectangle *    rect,
  PangoLayout *     layout)
{
  ArrangerObject * obj =
    (ArrangerObject *) ap;
  ZRegion * region =
    arranger_object_get_region (obj);
  AutomationPoint * next_ap =
    automation_region_get_next_ap (
      region, ap, true, true);
  ArrangerObject * next_obj =
    (ArrangerObject *) next_ap;
  ArrangerWidget * arranger =
    arranger_object_get_arranger (obj);

  Track * track =
    arranger_object_get_track (
      (ArrangerObject *) ap);
  g_return_if_fail (track);

  /* get color */
  GdkRGBA color = track->color;
  ui_get_arranger_object_color (
    &color,
    arranger->hovered_object == obj,
    automation_point_is_selected (ap), false,
    false);

  /* compile the shader */
  if (arranger->ap_shader &&
      !arranger->ap_shader_compiled)
    {
      GtkNative * native =
        gtk_widget_get_native (
          GTK_WIDGET (arranger));
      GskRenderer * renderer =
        gtk_native_get_renderer (native);
      GError * err = NULL;
      bool ret =
        gsk_gl_shader_compile (
          arranger->ap_shader, renderer,
          &err);
      if (!ret)
        {
          g_warning (
            "failed to compile shader: %s",
            err->message);
          return;
        }
      arranger->ap_shader_compiled = true;
    }

  graphene_vec4_t color_vec4;
  graphene_vec4_init (
    &color_vec4, color.red, color.green,
    color.blue, color.alpha);
  float yvals[20] = {
    1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 8.f, 9.f, 10.f,
    11.f, 12.f, 13.f, 14.f, 15.f, 16.f, 18.f, 19.f, 20.f };
  gtk_snapshot_push_gl_shader (
    snapshot, arranger->ap_shader,
    &GRAPHENE_RECT_INIT (
      obj->full_rect.x - 1, obj->full_rect.y - 1,
      obj->full_rect.width + 2,
      obj->full_rect.height + 2),
    gsk_gl_shader_format_args (
      arranger->ap_shader,
      "color", &color_vec4,
      "yvals", yvals,
      NULL));

  gtk_snapshot_pop (snapshot);

  return;

  cairo_t * cr =
    gtk_snapshot_append_cairo (
      snapshot,
      &GRAPHENE_RECT_INIT (
        obj->full_rect.x - 1, obj->full_rect.y - 1,
        obj->full_rect.width + 2,
        obj->full_rect.height + 2));
  gdk_cairo_set_source_rgba (cr, &color);

  cairo_set_line_width (cr, 2);

  int upslope =
    next_ap && ap->fvalue < next_ap->fvalue;
  (void) upslope;
  (void) next_obj;

  if (next_ap)
    {
      /* TODO use cairo translate to add padding */

      /* ignore the space between the center
       * of each point and the edges (2 of them
       * so a full AP_WIDGET_POINT_SIZE) */
      double width_for_curve =
        obj->full_rect.width - (double) AP_WIDGET_POINT_SIZE;
      double height_for_curve =
        obj->full_rect.height -
          AP_WIDGET_POINT_SIZE;

      double step = 0.1;
      double this_y = 0;
      for (double l = 0.0;
           l <= width_for_curve - step / 2.0;
           l += step)
        {
          double next_y =
            /* in pixels, higher values are lower */
            1.0 -
            automation_point_get_normalized_value_in_curve (
              ap,
              CLAMP (
                (l + step) / width_for_curve, 0.0, 1.0));
          next_y *= height_for_curve;

          if (math_doubles_equal (l, 0.0))
            {
              this_y =
                /* in pixels, higher values are lower */
                1.0 -
                automation_point_get_normalized_value_in_curve (
                  ap, 0.0);
              this_y *= height_for_curve;
              cairo_move_to (
                cr,
                (l + AP_WIDGET_POINT_SIZE / 2 +
                   obj->full_rect.x),
                (this_y + AP_WIDGET_POINT_SIZE / 2 +
                   obj->full_rect.y));
            }
          else
            {
              cairo_line_to (
                cr,
                (l + step +
                 AP_WIDGET_POINT_SIZE / 2 +
                 obj->full_rect.x),
                (next_y +
                  AP_WIDGET_POINT_SIZE / 2 +
                  obj->full_rect.y));
            }
          this_y = next_y;
        }

      cairo_stroke (cr);
    }
  else /* no next ap */
    {
    }

  /* draw circle */
  cairo_arc (
    cr,
    obj->full_rect.x + AP_WIDGET_POINT_SIZE / 2,
    upslope
    ?
      ((obj->full_rect.y + obj->full_rect.height) -
       AP_WIDGET_POINT_SIZE / 2)
    : (obj->full_rect.y + AP_WIDGET_POINT_SIZE / 2),
    AP_WIDGET_POINT_SIZE / 2,
    0, 2 * G_PI);
  cairo_set_source_rgba (cr, 0, 0, 0, 1);
  cairo_fill_preserve (cr);
  gdk_cairo_set_source_rgba (cr, &color);
  cairo_stroke (cr);

  if (DEBUGGING)
    {
      char text[500];
      sprintf (
        text, "%d/%d (%f)",
        ap->index,
        region->num_aps,
        (double) ap->normalized_val);
      if (arranger->action != UI_OVERLAY_ACTION_NONE
          && !obj->transient)
        {
          strcat (text, " - t");
        }
      cairo_set_source_rgba (cr, 1, 1, 1, 1);
      cairo_move_to (
        cr,
      (obj->full_rect.x + AP_WIDGET_POINT_SIZE / 2),
      upslope ?
        ((obj->full_rect.y + obj->full_rect.height) -
         AP_WIDGET_POINT_SIZE / 2) :
        (obj->full_rect.y + AP_WIDGET_POINT_SIZE / 2));
      cairo_show_text (cr, text);
    }
  else if (g_settings_get_boolean (
             S_UI, "show-automation-values") &&
           !(arranger->action !=
               UI_OVERLAY_ACTION_NONE
             && !obj->transient))
    {
      char text[50];
      sprintf (
        text, "%f", (double) ap->fvalue);
      cairo_set_source_rgba (cr, 1, 1, 1, 1);
      z_cairo_draw_text_full (
        cr, GTK_WIDGET (arranger), layout, text,
        (obj->full_rect.x +
           AP_WIDGET_POINT_SIZE / 2),
        upslope ?
          ((obj->full_rect.y +
              obj->full_rect.height) -
                AP_WIDGET_POINT_SIZE / 2) :
          (obj->full_rect.y +
             AP_WIDGET_POINT_SIZE / 2));
    }

  cairo_destroy (cr);
}

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
  double            y)
{
  ArrangerObject * obj = (ArrangerObject *) self;

  bool x_ok =
    (obj->full_rect.x - x) < AP_WIDGET_POINT_SIZE;

  if (y < 0)
    {
      return x_ok;
    }
  else
    {
      bool curves_up =
        automation_point_curves_up (self);
      if (x_ok &&
          curves_up ?
            (obj->full_rect.y + obj->full_rect.height) -
              y < AP_WIDGET_POINT_SIZE :
            y - obj->full_rect.y <
               AP_WIDGET_POINT_SIZE)
        return true;
    }

  return false;
}

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
  double            delta_from_curve)
{
  ArrangerObject * obj = (ArrangerObject *) self;

  /* subtract from 1 because cairo draws in the
   * opposite direction */
  double curve_val =
    1.0 -
    automation_point_get_normalized_value_in_curve (
      self,
      (x - obj->full_rect.x) /
        obj->full_rect.width);
  curve_val =
    obj->full_rect.y +
    curve_val * obj->full_rect.height;

  if (fabs (curve_val - y) <= delta_from_curve)
    return true;

  return false;
}
