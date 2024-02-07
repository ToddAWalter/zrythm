/*
 * SPDX-FileCopyrightText: © 2020 Alexandros Theodotou <alex@zrythm.org>
 *
 * SPDX-License-Identifier: LicenseRef-ZrythmLicense
 */

/**
 * \file
 *
 * Guile subsystem.
 */

#ifndef __GUILE_GUILE_H__
#define __GUILE_GUILE_H__

#include <stdbool.h>

/**
 * @addtogroup guile Guile scripting interface.
 *
 * @{
 */

typedef enum GuileScriptLanguage
{
  GUILE_SCRIPT_LANGUAGE_SCHEME,
  GUILE_SCRIPT_LANGUAGE_ECMASCRIPT,
  // GUILE_SCRIPT_LANGUAGE_ELISP,
  NUM_GUILE_SCRIPT_LANGUAGES,
} GuileScriptLanguage;

const char *
guile_get_script_language_str (GuileScriptLanguage lang);

const char *
guile_get_script_language_canonical_str (GuileScriptLanguage lang);

GuileScriptLanguage
guile_get_script_language_from_str (const char * str);

/**
 * Inits the guile subsystem.
 */
int
guile_init (int argc, char ** argv);

/**
 * Defines all available modules to be used
 * by scripts.
 *
 * This must be called in guile mode.
 */
void
guile_define_modules (void);

/**
 * Runs the script and returns the output message
 * in Pango markup.
 *
 * @param script The script to run as text.
 * @param lang The language of the script.
 */
char *
guile_run_script (const char * script, GuileScriptLanguage lang);

/**
 * Returns whether the script succeeded based on
 * the markup.
 */
bool
guile_script_succeeded (const char * pango_markup);

/**
 * @}
 */

#endif
