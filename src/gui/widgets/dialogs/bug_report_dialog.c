/*
 * Copyright (C) 2020 Alexandros Theodotou <alex at zrythm dot org>
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

#include "zrythm-config.h"

#include "actions/undo_manager.h"
#include "actions/undo_stack.h"
#include "gui/widgets/dialogs/bug_report_dialog.h"
#include "project.h"
#include "utils/gtk.h"
#include "utils/log.h"
#include "zrythm.h"

#include <glib/gi18n.h>

/**
 * Creates and displays the about dialog.
 */
GtkWidget *
bug_report_dialog_new (
  GtkWindow *  parent,
  const char * msg_prefix,
  const char * backtrace)
{
  GtkDialogFlags flags =
    GTK_DIALOG_DESTROY_WITH_PARENT;
  GtkWidget * dialog =
    gtk_message_dialog_new_with_markup (
      parent, flags,
      GTK_MESSAGE_ERROR,
      GTK_BUTTONS_CLOSE,
      NULL);

  char * log =
    log_get_last_n_lines (LOG, 60);
  char * undo_stack =
    undo_stack_get_as_string (
      UNDO_MANAGER->undo_stack, 12);

  /* %23 is hash, %0A is new line */
  char ver_with_caps[2000];
  zrythm_get_version_with_capabilities (
    ver_with_caps);
  char * report_template =
    g_strdup_printf (
      "# Steps to reproduce\n"
      "> Write a list of steps to reproduce the "
      "bug\n\n"
      "# What happens?\n"
      "> Please tell us what happened\n\n"
      "# What is expected?\n"
      "> What is expected to happen?\n\n"
      "# Version\n```\n%s```\n\n"
      "# Other info\n"
      "> Context, distro, etc.\n\n"
      "# Backtrace\n```\n%s```\n\n"
      "# Action stack\n```\n%s```\n\n"
      "# Log\n```\n%s```",
      ver_with_caps, backtrace, undo_stack, log);
  char * report_template_escaped =
    g_markup_escape_text (report_template, -1);
  char * report_template_escaped_for_uri =
    g_uri_escape_string (
      report_template, NULL, FALSE);
  char * atag =
    g_strdup_printf (
      "<a href=\"%s\">",
      NEW_ISSUE_URL);
  char * atag_email =
    g_strdup_printf (
      "<a href=\"mailto:%s?body=%s\">",
      NEW_ISSUE_EMAIL,
      report_template_escaped_for_uri);
  char * markup =
    g_strdup_printf (
      _("%sPlease help us fix "
        "this by "
        "%ssubmitting a bug report%s "
        "using the template below or by "
        "%ssending an email%s."),
      msg_prefix, atag, "</a>",
      atag_email, "</a>");

  gtk_message_dialog_set_markup (
    GTK_MESSAGE_DIALOG (dialog),
    markup);
  gtk_message_dialog_format_secondary_markup (
    GTK_MESSAGE_DIALOG (dialog),
    "%s", report_template_escaped);
  GtkLabel * label =
    z_gtk_message_dialog_get_label (
      GTK_MESSAGE_DIALOG (dialog), 1);
  gtk_label_set_selectable (
    label, 1);

  /* wrap the backtrace in a scrolled window */
  GtkBox * box =
    GTK_BOX (
      gtk_message_dialog_get_message_area (
        GTK_MESSAGE_DIALOG (dialog)));
  GtkWidget * secondary_area =
    z_gtk_container_get_nth_child (
      GTK_CONTAINER (box), 1);
  gtk_container_remove (
    GTK_CONTAINER (box), secondary_area);
  GtkWidget * scrolled_window =
    gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_min_content_height (
    GTK_SCROLLED_WINDOW (scrolled_window), 360);
  gtk_container_add (
    GTK_CONTAINER (scrolled_window),
    secondary_area);
  gtk_container_add (
    GTK_CONTAINER (box), scrolled_window);
  gtk_widget_show_all (GTK_WIDGET (box));

  g_free (report_template);
  g_free (atag);
  g_free (markup);
  g_free (report_template_escaped);
  g_free (report_template_escaped_for_uri);
  g_free (log);
  g_free (undo_stack);

  return dialog;
}
