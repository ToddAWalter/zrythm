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

#include "audio/channel.h"
#include "audio/track.h"
#include "gui/widgets/channel_send.h"
#include "gui/widgets/channel_sends_expander.h"
#include "project.h"
#include "utils/gtk.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#define CHANNEL_SENDS_EXPANDER_WIDGET_TYPE \
  (channel_sends_expander_widget_get_type ())
G_DEFINE_TYPE (
  ChannelSendsExpanderWidget,
  channel_sends_expander_widget,
  EXPANDER_BOX_WIDGET_TYPE)

/**
 * Refreshes each field.
 */
void
channel_sends_expander_widget_refresh (
  ChannelSendsExpanderWidget * self)
{
  for (int i = 0; i < STRIP_SIZE; i++)
    {
      gtk_widget_queue_draw (
        GTK_WIDGET (self->slots[i]));
    }
}

/**
 * Sets up the ChannelSendsExpanderWidget.
 */
void
channel_sends_expander_widget_setup (
  ChannelSendsExpanderWidget * self,
  ChannelSendsExpanderPosition position,
  Track *                      track)
{
  /* set name and icon */
  expander_box_widget_set_label (
    Z_EXPANDER_BOX_WIDGET (self),
    _("Sends"));

  if (track)
    {
      switch (track->out_signal_type)
        {
        case TYPE_AUDIO:
          expander_box_widget_set_icon_name (
            Z_EXPANDER_BOX_WIDGET (self),
            "audio-send");
          break;
        case TYPE_EVENT:
          expander_box_widget_set_icon_name (
            Z_EXPANDER_BOX_WIDGET (self),
            "midi-send");
          break;
        default:
          break;
        }
    }

  if (track != self->track ||
      position != self->position)
    {
      /* remove children */
      z_gtk_container_destroy_all_children (
        GTK_CONTAINER (self->box));

      Channel * ch =
        track_get_channel (track);
      g_return_if_fail (ch);
      for (int i = 0; i < STRIP_SIZE; i++)
        {
          GtkBox * strip_box =
            GTK_BOX (
              gtk_box_new (
                GTK_ORIENTATION_HORIZONTAL, 0));
          self->strip_boxes[i] = strip_box;
          ChannelSendWidget * csw =
            channel_send_widget_new (ch->sends[i]);
          self->slots[i] = csw;
          gtk_box_pack_start (
            strip_box, GTK_WIDGET (csw), 1, 1, 0);

          gtk_box_pack_start (
            self->box,
            GTK_WIDGET (strip_box), 0, 1, 0);
          gtk_widget_show_all (
            GTK_WIDGET (strip_box));
        }
    }

  self->track = track;
  self->position = position;

  channel_sends_expander_widget_refresh (self);
}

static void
channel_sends_expander_widget_class_init (
  ChannelSendsExpanderWidgetClass * klass)
{
}

static void
channel_sends_expander_widget_init (
  ChannelSendsExpanderWidget * self)
{
  self->scroll =
    GTK_SCROLLED_WINDOW (
      gtk_scrolled_window_new (
        NULL, NULL));
  gtk_widget_set_vexpand (
    GTK_WIDGET (self->scroll), 1);
  gtk_widget_set_visible (
    GTK_WIDGET (self->scroll), 1);
  gtk_scrolled_window_set_shadow_type (
    self->scroll, GTK_SHADOW_ETCHED_IN);
  gtk_widget_set_size_request (
    GTK_WIDGET (self->scroll), -1, 124);

  self->viewport =
    GTK_VIEWPORT (
      gtk_viewport_new (NULL, NULL));
  gtk_widget_set_visible (
    GTK_WIDGET (self->viewport), 1);
  gtk_container_add (
    GTK_CONTAINER (self->scroll),
    GTK_WIDGET (self->viewport));

  self->box =
    GTK_BOX (
      gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  gtk_widget_set_visible (
    GTK_WIDGET (self->box), 1);
  gtk_container_add (
    GTK_CONTAINER (self->viewport),
    GTK_WIDGET (self->box));

  expander_box_widget_add_content (
    Z_EXPANDER_BOX_WIDGET (self),
    GTK_WIDGET (self->scroll));
}
