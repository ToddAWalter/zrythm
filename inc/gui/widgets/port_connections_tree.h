/*
 * Copyright (C) 2019-2020 Alexandros Theodotou <alex at zrythm dot org>
 *
 * This file is part of Zrythm
 *
 * Zrythm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, version 3.
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
 * Port connections tree.
 */

#ifndef __GUI_WIDGETS_PORT_CONNECTIONS_TREE_H__
#define __GUI_WIDGETS_PORT_CONNECTIONS_TREE_H__

#include <gtk/gtk.h>

typedef struct Port Port;

#define PORT_CONNECTIONS_TREE_WIDGET_TYPE \
  (port_connections_tree_widget_get_type ())
G_DECLARE_FINAL_TYPE (
  PortConnectionsTreeWidget,
  port_connections_tree_widget,
  Z, PORT_CONNECTIONS_TREE_WIDGET,
  GtkScrolledWindow)

/**
 * @addtogroup widgets
 *
 * @{
 */

typedef struct _PortConnectionsTreeWidget
{
  GtkScrolledWindow    parent_instance;

  /* The tree views */
  GtkTreeView *        tree;
  GtkTreeModel *       tree_model;

  /** Temporary storage. */
  Port *               src_port;
  Port *               dest_port;
} PortConnectionsTreeWidget;

/**
 * Refreshes the tree model.
 */
void
port_connections_tree_widget_refresh (
  PortConnectionsTreeWidget * self);

/**
 * Instantiates a new PortConnectionsTreeWidget.
 */
PortConnectionsTreeWidget *
port_connections_tree_widget_new (void);

/**
 * @}
 */

#endif
