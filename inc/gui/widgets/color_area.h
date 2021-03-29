/*
 * Copyright (C) 2018-2019 Alexandros Theodotou <alex at zrythm dot org>
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

#ifndef __GUI_WIDGETS_COLOR_AREA_H__
#define __GUI_WIDGETS_COLOR_AREA_H__

/**
 * \file
 *
 * Color picker for a channel strip.
 */

#include <gtk/gtk.h>

#define COLOR_AREA_WIDGET_TYPE \
  (color_area_widget_get_type ())
G_DECLARE_FINAL_TYPE (
  ColorAreaWidget,
  color_area_widget,
  Z, COLOR_AREA_WIDGET,
  GtkDrawingArea)

/**
 * Type of ColorAreaWidget this is. */
typedef enum ColorAreaType
{
  /** Generic, only fill with color. */
  COLOR_AREA_TYPE_GENERIC,

  /**
   * Track, for use in TrackWidget implementations.
   *
   * It will show an icon and an index inside the
   * color box.
   */
  COLOR_AREA_TYPE_TRACK,
} ColorAreaType;

typedef struct Track Track;

typedef struct _ColorAreaWidget
{
  GtkDrawingArea    parent_instance;

  /** Color pointer to set/read value. */
  GdkRGBA           color;

  /** The type. */
  ColorAreaType     type;

  /** Track, if track. */
  Track *           track;

  /** Set to 1 to redraw. */
  int               redraw;

  bool              hovered;

  cairo_t *         cached_cr;

  cairo_surface_t * cached_surface;
} ColorAreaWidget;

/**
 * Creates a generic color widget using the given
 * color pointer.
 *
 * FIXME currently not used, should be used instead
 * of manually changing the color.
 */
void
color_area_widget_setup_generic (
  ColorAreaWidget * self,
  GdkRGBA * color);

/**
 * Creates a ColorAreaWidget for use inside
 * TrackWidget implementations.
 */
void
color_area_widget_setup_track (
  ColorAreaWidget * self,
  Track *           track);

/**
 * Changes the color.
 *
 * Track types don't need to do this since the
 * color is read directly from the Track.
 */
void
color_area_widget_set_color (
  ColorAreaWidget * widget,
  GdkRGBA * color);

#endif
