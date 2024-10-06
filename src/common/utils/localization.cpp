// SPDX-FileCopyrightText: © 2019-2022 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

/**
 * @file
 *
 * Localization utils.
 *
 */

#include "zrythm-config.h"

#include "common/utils/directory_manager.h"
#include "common/utils/io.h"
#include "common/utils/localization.h"
#include "common/utils/string.h"
#include "common/utils/ui.h"
#include "gui/backend/backend/settings/g_settings_manager.h"
#include "gui/backend/backend/settings/settings.h"
#include "gui/backend/backend/zrythm.h"
#include "gui/backend/gtk_widgets/main_window.h"
#include "gui/backend/gtk_widgets/zrythm_app.h"

#include <glib/gi18n.h>

#include "zrythm-locales.h" // note: auto-generated
#include <locale.h>

#ifdef _WIN32
#  define CODESET "1252"
#  define ALT_CODESET "1252"
#else
#  define CODESET "UTF-8"
#  define ALT_CODESET "UTF8"
#endif

/**
 * Returns the localized language name (e.g.
 * "Ελληνικά").
 */
const char *
localization_get_localized_name (LocalizationLanguage lang)
{
  z_return_val_if_fail (lang >= 0 && lang < NUM_LL_LANGUAGES, nullptr);

  return language_strings[lang];
}

/**
 * Returns the character string code for the
 * language (e.g. "fr").
 */
const char *
localization_get_string_code (LocalizationLanguage lang)
{
  z_return_val_if_fail (lang >= 0 && lang < NUM_LL_LANGUAGES, nullptr);

  return language_codes[lang];
}

/**
 * Returns the localized language name with the
 * code (e.g. "Ελληνικά [el]").
 */
const char *
localization_get_string_w_code (LocalizationLanguage lang)
{
  z_return_val_if_fail (lang >= 0 && lang < NUM_LL_LANGUAGES, nullptr);

  return language_strings_w_codes[lang];
}

const char **
localization_get_language_codes (void)
{
  return language_codes;
}

const char **
localization_get_language_strings (void)
{
  return language_strings;
}

const char **
localization_get_language_strings_w_codes (void)
{
  return language_strings_w_codes;
}

static char *
get_match (
  char **      installed_locales,
  int          num_installed_locales,
  const char * prefix,
  const char * codeset)
{
  GString * codeset_gstring = g_string_new (codeset);
  char *    upper = g_string_free (g_string_ascii_up (codeset_gstring), 0);
  z_return_val_if_fail (upper, nullptr);
  codeset_gstring = g_string_new (codeset);
  char * lower = g_string_free (g_string_ascii_down (codeset_gstring), 0);
  z_return_val_if_fail (lower, nullptr);
  char * first_upper = g_strdup (lower);
  first_upper[0] = g_ascii_toupper (lower[0]);

  char * ret = NULL;
  for (int i = 0; i < num_installed_locales; i++)
    {
      char * installed_locale = installed_locales[i];
      if (g_str_has_prefix (installed_locale, prefix))
        {
          if (
            g_str_has_suffix (installed_locale, upper)
            || g_str_has_suffix (installed_locale, lower)
            || g_str_has_suffix (installed_locale, first_upper))
            {
              ret = installed_locales[i];
              break;
            }
        }
    }
  g_free (upper);
  g_free (lower);
  g_free (first_upper);

  return ret;
}

std::string
localization_locale_exists (LocalizationLanguage lang)
{
#ifdef _WIN32
  const char * _code = localization_get_string_code (lang);
  return g_strdup (_code);
#else

  /* get available locales on the system */
  FILE * fp;
  char   path[1035];
  char * installed_locales[8000];
  int    num_installed_locales = 0;

  /* Open the command for reading. */
  fp = popen ("locale -a", "r");
  if (fp == nullptr)
    {
      g_error ("localization_init: popen failed");
      exit (1);
    }

  /* Read the output a line at a time - output it. */
  while (fgets (path, sizeof (path) - 1, fp) != nullptr)
    {
      installed_locales[num_installed_locales++] =
        g_strdup_printf ("%s", g_strchomp (path));
    }

  /* close */
  pclose (fp);

#  define IS_MATCH(caps, code) \
  case LL_##caps: \
    match = \
      get_match (installed_locales, num_installed_locales, code, CODESET); \
    if (!match) \
      { \
        match = get_match ( \
          installed_locales, num_installed_locales, code, ALT_CODESET); \
      } \
    break

  char * match = NULL;
  switch (lang)
    {
      IS_MATCH (AF_ZA, "af_ZA");
      IS_MATCH (AR, "ar_");
      IS_MATCH (CA, "ca_");
      /*IS_MATCH (CS, "cs_");*/
      /*IS_MATCH (DA, "da_");*/
      IS_MATCH (DE, "de_");
      /* break order here */
      IS_MATCH (EN_GB, "en_GB");
      IS_MATCH (EN, "en_");
      IS_MATCH (EL, "el_");
      IS_MATCH (ES, "es_");
      /*IS_MATCH (ET, "et_");*/
      IS_MATCH (FA, "fa_");
      /*IS_MATCH (FI, "fi_");*/
      IS_MATCH (FR, "fr_");
      /*IS_MATCH (GD, "gd_");*/
      IS_MATCH (GL, "gl_");
      IS_MATCH (HE, "he_");
      IS_MATCH (HI, "hi_");
      IS_MATCH (HU, "hu_");
      IS_MATCH (ID, "id_");
      IS_MATCH (IT, "it_");
      IS_MATCH (JA, "ja_");
      IS_MATCH (KO, "ko_");
      IS_MATCH (MK, "mk_");
      IS_MATCH (NB_NO, "nb_NO");
      IS_MATCH (NL, "nl_");
      IS_MATCH (PL, "pl_");
      /* break order here */
      IS_MATCH (PT_BR, "pt_BR");
      IS_MATCH (PT, "pt_");
      IS_MATCH (RU, "ru_");
      IS_MATCH (TH, "th_");
      IS_MATCH (TR, "tr_");
      IS_MATCH (SV, "sv_");
      IS_MATCH (UK, "uk_");
      IS_MATCH (VI, "vi_");
      IS_MATCH (ZH_CN, "zh_CN");
      IS_MATCH (ZH_TW, "zh_TW");
    case NUM_LL_LANGUAGES:
      z_return_val_if_reached ("");
    }

#  undef IS_MATCH

  match = g_strdup (match);
  for (int i = 0; i < num_installed_locales; i++)
    {
      g_free (installed_locales[i]);
    }

  return match;
#endif
}

/**
 * Sets the locale to the currently selected one and
 * inits gettext.
 *
 * @param use_locale Use the user's local instead of
 *   the Zrythm settings.
 * @param print_debug_messages Set to false to
 *   silence messages.
 *
 * Returns if a locale for the selected language
 * exists on the system or not.
 */
bool
localization_init (
  bool use_locale,
  bool print_debug_messages,
  bool queue_error_if_not_installed)
{
  char *               code = NULL;
  LocalizationLanguage lang;
  if (use_locale)
    {
      code = g_strdup (setlocale (LC_ALL, nullptr));
      z_info ("Initing localization with system locale {}", code);
    }
  else
    {
      /* get selected locale */
      GSettings * prefs =
        g_settings_new (GSETTINGS_ZRYTHM_PREFIX ".preferences.ui.general");
      lang = ENUM_INT_TO_VALUE (
        LocalizationLanguage, g_settings_get_enum (prefs, "language"));
      g_object_unref (G_OBJECT (prefs));

      if (print_debug_messages)
        {
          z_info (
            "preferred lang: '{}' ({})", language_strings[lang],
            language_codes[lang]);
        }

      if (lang == LL_EN)
        {
          if (print_debug_messages)
            {
              z_info ("setting locale to default");
            }
          setlocale (LC_ALL, "C");
          return 1;
        }

      code = g_strdup (localization_locale_exists (lang).c_str ());
      z_debug ("code is {}", code);
    }

  char * match = NULL;
  if (code)
    {
      match = setlocale (LC_ALL, code);
      if (print_debug_messages)
        {
          z_info ("setting locale to {} (found {})", code, match);
        }
#if defined(_WIN32) || defined(__APPLE__)
      char buf[120];
      sprintf (buf, "LANG=%s", code);
      putenv (buf);
#endif
      g_free (code);
    }
  else
    {
      if (!use_locale)
        {
          std::string msg = fmt::format (
            "No locale for \"{}\" is installed, using default",
            language_strings[lang]);
          if (queue_error_if_not_installed)
            {
              std::lock_guard<std::mutex> lock (
                zrythm_app->startup_error_queue_mutex_);
              zrythm_app->startup_error_queue_.push (msg);
            }
          z_warning ("{}", msg);
        }
      setlocale (LC_ALL, "C");
    }

  /* if LC_ALL passed, use that value instead of logic above */
  const char * lc_all_from_env = g_getenv ("LC_ALL");
  if (lc_all_from_env)
    {
      setlocale (LC_ALL, lc_all_from_env);
    }

    /* bind text domain */
#if defined(_WIN32) && ZRYTHM_IS_INSTALLER_VER
  const char * windows_localedir = "share/locale";
  bindtextdomain (GETTEXT_PACKAGE, windows_localedir);
  bindtextdomain ("libadwaita", windows_localedir);
#else
  auto * dir_mgr = DirectoryManager::getInstance ();
  auto   localedir =
    dir_mgr->get_dir (DirectoryManager::DirectoryType::SYSTEM_LOCALEDIR);
  bindtextdomain (GETTEXT_PACKAGE, localedir.c_str ());
  bindtextdomain ("libadwaita", localedir.c_str ());
  z_debug ("setting textdomain: {}, {}", GETTEXT_PACKAGE, localedir);
#endif

  /* set domain codeset */
  bind_textdomain_codeset (GETTEXT_PACKAGE, CODESET);

  /* set current domain */
  textdomain (GETTEXT_PACKAGE);

  return match != NULL;
}
