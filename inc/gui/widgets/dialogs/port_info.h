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
 * Dialog for viewing/editing port info.
 */

#ifndef __GUI_WIDGETS_DIALOGS_PORT_INFO_H__
#define __GUI_WIDGETS_DIALOGS_PORT_INFO_H__

#include <gtk/gtk.h>

#define PORT_INFO_DIALOG_WIDGET_TYPE \
  (port_info_dialog_widget_get_type ())
G_DECLARE_FINAL_TYPE (
  PortInfoDialogWidget,
  port_info_dialog_widget,
  Z, PORT_INFO_DIALOG_WIDGET,
  GtkDialog)

typedef struct Port Port;

/**
 * @addtogroup widgets
 *
 * @{
 */

/**
 * The port_info dialog.
 */
typedef struct _PortInfoDialogWidget
{
  GtkDialog            parent_instance;

  GtkLabel *           name_lbl;
  GtkLabel *           full_designation_lbl;
  GtkLabel *           type_lbl;
  GtkLabel *           range_lbl;
  GtkLabel *           current_val_lbl;
  GtkLabel *           default_value_lbl;

  GtkBox *             flags_box;

  /* TODO */
  GtkBox *             scale_points_box;

  /** The port this is about. */
  Port *               port;
} PortInfoDialogWidget;

/**
 * Creates an port_info dialog widget and displays it.
 */
PortInfoDialogWidget *
port_info_dialog_widget_new (
  Port * port);

/**
 * @}
 */

#endif
