// SPDX-FileCopyrightText: © 2020-2021 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "common/utils/gtk.h"
#include "common/utils/resources.h"
#include "gui/cpp/backend/project.h"
#include "gui/cpp/gtk_widgets/center_dock.h"
#include "gui/cpp/gtk_widgets/gtk_wrapper.h"
#include "gui/cpp/gtk_widgets/left_dock_edge.h"
#include "gui/cpp/gtk_widgets/main_window.h"
#include "gui/cpp/gtk_widgets/port_connections.h"
#include "gui/cpp/gtk_widgets/port_connections_tree.h"

#include <glib/gi18n.h>

G_DEFINE_TYPE (PortConnectionsWidget, port_connections_widget, GTK_TYPE_BOX)

/**
 * Refreshes the port_connections widget.
 */
void
port_connections_widget_refresh (PortConnectionsWidget * self)
{
  port_connections_tree_widget_refresh (self->bindings_tree);
}

PortConnectionsWidget *
port_connections_widget_new (void)
{
  PortConnectionsWidget * self = Z_PORT_CONNECTIONS_WIDGET (
    g_object_new (PORT_CONNECTIONS_WIDGET_TYPE, nullptr));

  self->bindings_tree = port_connections_tree_widget_new ();
  gtk_box_append (GTK_BOX (self), GTK_WIDGET (self->bindings_tree));
  gtk_widget_set_vexpand (GTK_WIDGET (self->bindings_tree), true);

  return self;
}

static void
port_connections_widget_class_init (PortConnectionsWidgetClass * _klass)
{
  GtkWidgetClass * klass = GTK_WIDGET_CLASS (_klass);
  gtk_widget_class_set_css_name (klass, "port-connections");
}

static void
port_connections_widget_init (PortConnectionsWidget * self)
{
}
