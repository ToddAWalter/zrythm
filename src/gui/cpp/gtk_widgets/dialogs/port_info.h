// SPDX-FileCopyrightText: © 2020, 2023 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

/**
 * @file
 *
 * Dialog for viewing/editing port info.
 */

#ifndef __GUI_WIDGETS_DIALOGS_PORT_INFO_H__
#define __GUI_WIDGETS_DIALOGS_PORT_INFO_H__

#include "gui/cpp/gtk_widgets/gtk_wrapper.h"

#define PORT_INFO_DIALOG_WIDGET_TYPE (port_info_dialog_widget_get_type ())
G_DECLARE_FINAL_TYPE (
  PortInfoDialogWidget,
  port_info_dialog_widget,
  Z,
  PORT_INFO_DIALOG_WIDGET,
  GtkWindow)

class Port;

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
  GtkWindow parent_instance;

  /* TODO */
  GtkBox * scale_points_box;

  /** The port this is about. */
  Port * port;
} PortInfoDialogWidget;

/**
 * Creates an port_info dialog widget and displays it.
 */
PortInfoDialogWidget *
port_info_dialog_widget_new (Port * port);

/**
 * @}
 */

#endif
