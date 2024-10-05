// SPDX-FileCopyrightText: © 2019-2022, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "zrythm-config.h"

/* auto-generated */
#include "gui/widgets/dialogs/about_dialog.h"
#include "utils/resources.h"
#include "zrythm.h"

#include <glib/gi18n.h>

#include "libadwaita_wrapper.h"
#include "translators.h"

/**
 * Creates and displays the about dialog.
 */
GtkWindow *
about_dialog_widget_new (GtkWindow * parent)
{
  const char * artists[] = {
    "Alexandros Theodotou <alex@zrythm.org>", "Daniel Peterson",
    "Carlos Han (C.K. Design)", NULL
  };
  const char * authors[] = {
    "Alexandros Theodotou <alex@zrythm.org>", "Miró Allard",  "Ryan Gonzalez",
    "Sascha Bast <sash@mischkonsum.org>",     "Georg Krause", NULL
  };
  const char * documenters[] = {
    "Alexandros Theodotou <alex@zrythm.org>", NULL
  };
  const char * translators = TRANSLATORS_STR;

  const auto version = Zrythm::get_version (true);
  const auto sys_nfo = Zrythm::get_system_info ();

  AdwAboutWindow * dialog = ADW_ABOUT_WINDOW (adw_about_window_new ());

  adw_about_window_set_artists (dialog, artists);
  adw_about_window_set_developers (dialog, authors);
  adw_about_window_set_documenters (dialog, documenters);
  adw_about_window_set_translator_credits (dialog, translators);
  adw_about_window_set_copyright (
    dialog,
    "Copyright © " COPYRIGHT_YEARS " " COPYRIGHT_NAME
#if !defined(HAVE_CUSTOM_LOGO_AND_SPLASH) || !defined(HAVE_CUSTOM_NAME)
    "\nZrythm and the Zrythm logo are trademarks of Alexandros Theodotou, registered in the UK and other countries"
#endif
  );
  adw_about_window_set_comments (
    dialog, _ ("a highly automated and intuitive digital audio workstation"));
  adw_about_window_set_debug_info (dialog, sys_nfo.c_str ());
  adw_about_window_set_version (dialog, version.c_str ());
  adw_about_window_set_application_icon (dialog, "zrythm");
  adw_about_window_set_application_name (dialog, "Zrythm");
  adw_about_window_set_issue_url (dialog, ISSUE_TRACKER_URL);
  adw_about_window_set_license_type (dialog, GTK_LICENSE_CUSTOM);

  gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);
  gtk_window_set_modal (GTK_WINDOW (dialog), true);

  return GTK_WINDOW (dialog);
}
