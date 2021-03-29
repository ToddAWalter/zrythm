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

/**
 * \file
 *
 * Widget for showing info about the current
 * selection.
 */

#ifndef __GUI_WIDGETS_SELECTION_INFO_H__
#define __GUI_WIDGETS_SELECTION_INFO_H__

#include "utils/ui.h"

#include <gtk/gtk.h>

#define SELECTION_INFO_WIDGET_TYPE \
  (selection_info_widget_get_type ())
G_DECLARE_DERIVABLE_TYPE (
  SelectionInfoWidget,
  selection_info_widget,
  Z, SELECTION_INFO_WIDGET,
  GtkGrid)

#define SELECTION_INFO_WIDGET_GET_PRIVATE(self) \
  SelectionInfoWidgetPrivate * sel_inf_prv = \
    selection_info_widget_get_private ( \
      Z_SELECTION_INFO_WIDGET (self));

/**
 * A widget to display info about the current
 * arranger selection, to be used in the
 * TimelineArrangerWidget and in the
 * and PianoRoll.
 */
typedef struct _SelectionInfoWidgetPrivate
{
  GtkWidget *   labels[14];
  GtkWidget *   widgets[14];
  int           num_items;
} SelectionInfoWidgetPrivate;

typedef struct _SelectionInfoWidgetClass
{
  GtkGridClass parent_class;
} SelectionInfoWidgetClass;

#define selection_info_widget_add_info_with_text( \
  _self,_txt,_widget) \
  GtkWidget * _lbl = gtk_label_new (_txt); \
  gtk_widget_set_visible (_lbl, 1); \
  selection_info_widget_add_info ( \
    _self, _lbl, GTK_WIDGET (_widget));

/**
 * Adds a piece of info to the grid.
 *
 * @param label The label widget to place on top.
 *
 *   Usually this will be GtkLabel.
 * @param widget The Widget to place on the bot that
 *   will reflect the information.
 */
void
selection_info_widget_add_info (
  SelectionInfoWidget * self,
  GtkWidget *           label,
  GtkWidget *           widget);

/**
 * Destroys all children.
 */
void
selection_info_widget_clear (
  SelectionInfoWidget * self);

/**
 * Returns the private.
 */
SelectionInfoWidgetPrivate *
selection_info_widget_get_private (
  SelectionInfoWidget * self);

#endif
