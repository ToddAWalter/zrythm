/*
 * Copyright (C) 2019 Alexandros Theodotou <alex at zrythm dot org>
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

#include "audio/track.h"
#include "audio/track_lane.h"
#include "gui/widgets/track_lanelist.h"
#include "gui/widgets/track_lane.h"
#include "utils/gtk.h"
#include "utils/arrays.h"

G_DEFINE_TYPE (
  TrackLanelistWidget, track_lanelist_widget,
  GTK_TYPE_BOX)

#define GET_TRACK(self) Track * track = \
  self->track_lanelist->track

static void
on_resize_end (
  TrackLanelistWidget * self,
  GtkWidget *       child)
{
  if (!Z_IS_TRACK_LANE_WIDGET (child))
    return;

  TrackLaneWidget * tlw =
    Z_TRACK_LANE_WIDGET (child);
  TrackLane * lane = tlw->lane;
  GValue a = G_VALUE_INIT;
  g_value_init (&a, G_TYPE_INT);
  gtk_container_child_get_property (
    GTK_CONTAINER (self),
    GTK_WIDGET (tlw),
    "position",
    &a);
  lane->handle_pos = g_value_get_int (&a);
}

/**
 * Creates and returns the widget.
 */
TrackLanelistWidget *
track_lanelist_widget_new (
  Track * track)
{
  TrackLanelistWidget * self =
    g_object_new (
      TRACK_LANELIST_WIDGET_TYPE,
      NULL);

  self->track = track;
  g_warn_if_fail (track);

  return self;
}

/**
 * Show or hide all automation lane widgets.
 */
void
track_lanelist_widget_refresh (
  TrackLanelistWidget *self)
{
  gtk_widget_set_visible (
    GTK_WIDGET (self),
    self->track->lanes_visible);

  if (!self->track->lanes_visible)
    return;

  /* remove all children */
  z_gtk_container_remove_all_children (
    GTK_CONTAINER (self));

  /* readd children */
  TrackLane * lane;
  for (int i = 0;
       i < self->track->num_lanes;
       i++)
    {
      lane = self->track->lanes[i];

      /*g_message ("lane at %d %d %s", i,*/
                 /*lane->pos, lane->name);*/

      if (!GTK_IS_WIDGET (lane->widget))
        lane->widget =
          track_lane_widget_new (lane);

      track_lane_widget_refresh (
        lane->widget);

      /* add to automation tracklist widget */
      gtk_container_add (
        GTK_CONTAINER (self),
        GTK_WIDGET (lane->widget));
    }

  /* set handle position.
   * this is done because the position resets to -1
   * every
   * time a child is added or deleted */
  GList *children, *iter;
  children =
    gtk_container_get_children (
      GTK_CONTAINER (self));
  for (iter = children;
       iter != NULL;
       iter = g_list_next (iter))
    {
      if (Z_IS_TRACK_LANE_WIDGET (iter->data))
        {
          TrackLaneWidget * lw =
            Z_TRACK_LANE_WIDGET (iter->data);
          lane = lw->lane;
          GValue a = G_VALUE_INIT;
          g_value_init (&a, G_TYPE_INT);
          g_value_set_int (&a, lane->handle_pos);
          gtk_container_child_set_property (
            GTK_CONTAINER (self),
            GTK_WIDGET (lw),
            "position",
            &a);
        }
    }
  g_list_free(children);
}

static void
track_lanelist_widget_init (
  TrackLanelistWidget * self)
{
  gtk_widget_set_vexpand (
    GTK_WIDGET (self), 1);

  /*g_signal_connect (*/
    /*G_OBJECT (self), "resize-drag-end",*/
    /*G_CALLBACK (on_resize_end), NULL);*/
}

static void
track_lanelist_widget_class_init (
  TrackLanelistWidgetClass * klass)
{
}
