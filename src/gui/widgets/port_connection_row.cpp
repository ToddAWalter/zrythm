// SPDX-FileCopyrightText: © 2019-2021, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "actions/port_connection_action.h"
#include "actions/undo_manager.h"
#include "dsp/port.h"
#include "dsp/port_connection.h"
#include "dsp/port_connections_manager.h"
#include "gui/widgets/bar_slider.h"
#include "gui/widgets/knob.h"
#include "gui/widgets/popovers/port_connections_popover.h"
#include "gui/widgets/port_connection_row.h"
#include "project.h"
#include "utils/gtk.h"
#include "utils/objects.h"
#include "zrythm.h"

#include <glib/gi18n.h>

#include "gtk_wrapper.h"

G_DEFINE_TYPE (PortConnectionRowWidget, port_connection_row_widget, GTK_TYPE_BOX)

static void
on_enable_toggled (GtkToggleButton * btn, PortConnectionRowWidget * self)
{
  try
    {
      if (gtk_toggle_button_get_active (btn))
        {
          UNDO_MANAGER->perform (std::make_unique<PortConnectionEnableAction> (
            self->connection->src_id_, self->connection->dest_id_));
        }
      else
        {
          UNDO_MANAGER->perform (std::make_unique<PortConnectionDisableAction> (
            self->connection->src_id_, self->connection->dest_id_));
        }
    }
  catch (const ZrythmException &e)
    {
      e.handle (_ ("Failed to enable connection"));
    }

  g_return_if_fail (IS_PORT_AND_NONNULL (self->parent->port));
  port_connections_popover_widget_refresh (self->parent, self->parent->port);
}

static void
on_del_clicked (GtkButton * btn, PortConnectionRowWidget * self)
{
  try
    {

      UNDO_MANAGER->perform (std::make_unique<PortConnectionDisconnectAction> (
        self->connection->src_id_, self->connection->dest_id_));
    }
  catch (const ZrythmException &e)
    {
      e.handle (_ ("Failed to disconnect"));
    }

  g_return_if_fail (IS_PORT_AND_NONNULL (self->parent->port));
  port_connections_popover_widget_refresh (self->parent, self->parent->port);
}

static void
finalize (PortConnectionRowWidget * self)
{
  object_delete_and_null (self->connection);

  G_OBJECT_CLASS (port_connection_row_widget_parent_class)
    ->finalize (G_OBJECT (self));
}

/**
 * Creates the popover.
 */
PortConnectionRowWidget *
port_connection_row_widget_new (
  PortConnectionsPopoverWidget * parent,
  const PortConnection *         connection,
  bool                           is_input)
{
  PortConnectionRowWidget * self = Z_PORT_CONNECTION_ROW_WIDGET (
    g_object_new (PORT_CONNECTION_ROW_WIDGET_TYPE, nullptr));

  self->connection = new PortConnection (*connection);

  self->is_input = is_input;
  self->parent = parent;

  /* create the widgets and pack */
  GtkWidget * box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_widget_set_visible (box, 1);

  /* power button */
  GtkToggleButton * btn = z_gtk_toggle_button_new_with_icon ("network-connect");
  gtk_toggle_button_set_active (btn, connection->enabled_);
  gtk_widget_set_visible (GTK_WIDGET (btn), 1);
  gtk_box_append (GTK_BOX (box), GTK_WIDGET (btn));
  gtk_widget_set_tooltip_text (
    GTK_WIDGET (btn), _ ("Enable/disable connection"));
  g_signal_connect (
    G_OBJECT (btn), "toggled", G_CALLBACK (on_enable_toggled), self);

  /* create overlay */
  self->overlay = GTK_OVERLAY (gtk_overlay_new ());
  gtk_widget_set_visible (GTK_WIDGET (self->overlay), 1);
  gtk_box_append (GTK_BOX (box), GTK_WIDGET (self->overlay));

  /* bar slider */
  const auto &port_id = is_input ? connection->dest_id_ : connection->src_id_;
  Port *      port = Port::find_from_identifier (port_id);
  if (!port)
    {
      z_error ("failed to find port for '{}'", port_id.get_label ());
      return nullptr;
    }
  auto designation = port->get_full_designation () + " ";
  self->slider =
    bar_slider_widget_new_port_connection (connection, designation.c_str ());
  gtk_overlay_set_child (GTK_OVERLAY (self->overlay), GTK_WIDGET (self->slider));

  /* delete connection button */
  self->delete_btn = GTK_BUTTON (gtk_button_new_from_icon_name ("edit-delete"));
  gtk_widget_set_visible (GTK_WIDGET (self->delete_btn), 1);
  gtk_box_append (GTK_BOX (box), GTK_WIDGET (self->delete_btn));
  gtk_widget_set_tooltip_text (
    GTK_WIDGET (self->delete_btn), _ ("Delete connection"));
  g_signal_connect (
    G_OBJECT (self->delete_btn), "clicked", G_CALLBACK (on_del_clicked), self);

  gtk_box_append (GTK_BOX (self), box);

  gtk_widget_set_sensitive (box, !connection->locked_);

  return self;
}

static void
port_connection_row_widget_class_init (PortConnectionRowWidgetClass * _klass)
{
  GObjectClass * oklass = G_OBJECT_CLASS (_klass);
  oklass->finalize = (GObjectFinalizeFunc) finalize;
}

static void
port_connection_row_widget_init (PortConnectionRowWidget * self)
{
  gtk_widget_set_visible (GTK_WIDGET (self), 1);
}
