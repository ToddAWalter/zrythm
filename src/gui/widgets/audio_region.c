/*
 * Copyright (C) 2019 Alexandros Theodotou <alex at zrythm dot org>
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

#include "audio/audio_bus_track.h"
#include "audio/channel.h"
#include "audio/clip.h"
#include "audio/engine.h"
#include "audio/instrument_track.h"
#include "audio/pool.h"
#include "audio/track.h"
#include "gui/widgets/arranger.h"
#include "gui/widgets/center_dock.h"
#include "gui/widgets/main_window.h"
#include "gui/widgets/audio_region.h"
#include "gui/widgets/region.h"
#include "gui/widgets/ruler.h"
#include "gui/widgets/timeline_arranger.h"
#include "project.h"
#include "utils/cairo.h"
#include "utils/ui.h"

G_DEFINE_TYPE (AudioRegionWidget,
               audio_region_widget,
               REGION_WIDGET_TYPE)

static gboolean
audio_region_draw_cb (
  AudioRegionWidget * self,
  cairo_t *cr, gpointer data)
{
  REGION_WIDGET_GET_PRIVATE (data);
  Region * r = rw_prv->region;
  AudioRegion * ar = (AudioRegion *) r;

  GtkStyleContext * context =
    gtk_widget_get_style_context (
      GTK_WIDGET (self));

  int width =
    gtk_widget_get_allocated_width (
      GTK_WIDGET (self));
  int height =
    gtk_widget_get_allocated_height (
      GTK_WIDGET (self));

  gtk_render_background (
    context, cr,
    0, 0, width, height);

  GdkRGBA * color = &r->lane->track->color;
  cairo_set_source_rgba (
    cr, color->red + 0.3, color->green + 0.3,
    color->blue + 0.3, 0.9);
  cairo_set_line_width (cr, 1);

  AudioClip * clip =
    AUDIO_POOL->clips[ar->pool_id];

  long prev_frames = 0;
  long loop_end_frames =
    position_to_frames (&r->loop_end_pos);
  long loop_frames =
    region_get_loop_length_in_frames (r);
  long clip_start_frames =
    position_to_frames (&r->clip_start_pos);
  for (double i = 0.0; i < (double) width; i += 0.6)
    {
      /* current single channel frames */
      long curr_frames =
        ui_px_to_frames_timeline (i, 0);
      curr_frames += clip_start_frames;
      while (curr_frames >= loop_end_frames)
        {
          curr_frames -= loop_frames;
        }
      float min = 0, max = 0;
      for (long j = prev_frames;
           j < curr_frames; j++)
        {
          for (unsigned int k = 0;
               k < clip->channels; k++)
            {
              long index =
                j * clip->channels + k;
              g_warn_if_fail (
                index <
                  clip->num_frames * clip->channels);
              float val =
                clip->frames[index];
              if (val > max)
                max = val;
              if (val < min)
                min = val;
            }
        }
      /* normalize */
      min = (min + 1.f) / 2.f;
      max = (max + 1.f) / 2.f;
      z_cairo_draw_vertical_line (
        cr,
        i,
        MAX (
          (double) min * (double) height, 0.0),
        MIN (
          (double) max * (double) height,
          (double) height));

      prev_frames = curr_frames;
    }

 return FALSE;
}

/**
 * Sets the appropriate cursor.
 */
static void
on_motion (GtkWidget * widget, GdkEventMotion *event)
{
  /*AudioRegionWidget * self = Z_AUDIO_REGION_WIDGET (widget);*/
  GtkAllocation allocation;
  gtk_widget_get_allocation (widget,
                             &allocation);
  /*ARRANGER_WIDGET_GET_PRIVATE (MW_TIMELINE);*/
  /*REGION_WIDGET_GET_PRIVATE (self);*/
}

AudioRegionWidget *
audio_region_widget_new (
  AudioRegion * audio_region)
{
  AudioRegionWidget * self =
    g_object_new (
      AUDIO_REGION_WIDGET_TYPE, NULL);

  region_widget_setup (
    Z_REGION_WIDGET (self), audio_region);

  /* connect signals */
  g_signal_connect (
    G_OBJECT (self), "draw",
    G_CALLBACK (audio_region_draw_cb), self);
  g_signal_connect (
    G_OBJECT (self), "enter-notify-event",
    G_CALLBACK (on_motion),  self);
  g_signal_connect (
    G_OBJECT(self), "leave-notify-event",
    G_CALLBACK (on_motion),  self);
  g_signal_connect (
    G_OBJECT(self), "motion-notify-event",
    G_CALLBACK (on_motion),  self);

  return self;
}

static void
audio_region_widget_class_init (AudioRegionWidgetClass * klass)
{
}

static void
audio_region_widget_init (AudioRegionWidget * self)
{
}

