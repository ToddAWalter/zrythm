/*
 * Copyright (C) 2020-2021 Alexandros Theodotou <alex at zrythm dot org>
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <string.h>

#include "guile/modules.h"

#ifndef SNARF_MODE
#include "zrythm-config.h"
#include "plugins/plugin_manager.h"
#include "project.h"
#include "zrythm.h"
#endif

/**
 * Guile function to get the zrythm pointer.
 */
#if 0
static SCM
get_ptr (void)
{
  return scm_from_pointer (ZRYTHM, NULL);
}
#endif

SCM_DEFINE (
  s_zrythm_get_ver, "zrythm-get-ver", 0, 0, 0,
  (),
  "Return the "
#ifdef SNARF_MODE
  "Zrythm"
#else
  PROGRAM_NAME
#endif
  " version as a string.")
#define FUNC_NAME s_
{
  char ver[1000];
  zrythm_get_version_with_capabilities (ver);
  return
    scm_from_stringn (
      ver, strlen (ver), "UTF8",
      SCM_FAILED_CONVERSION_QUESTION_MARK);
}
#undef FUNC_NAME

SCM_DEFINE (
  s_zrythm_get_plugin_manager,
  "zrythm-get-plugin-manager", 0, 0, 0,
  (),
  "Return the PluginManager instance.")
#define FUNC_NAME s_
{
  return
    scm_from_pointer (PLUGIN_MANAGER, NULL);
}
#undef FUNC_NAME

SCM_DEFINE (
  s_zrythm_get_project,
  "zrythm-get-project", 0, 0, 0,
  (),
  "Return the currently loaded Project instance.")
#define FUNC_NAME s_
{
  return
    scm_from_pointer (PROJECT, NULL);
}
#undef FUNC_NAME

SCM_DEFINE (
  s_zrythm_null,
  "zrythm-null", 0, 0, 0,
  (),
  "Returns a NULL pointer.")
#define FUNC_NAME s_
{
  return NULL;
}
#undef FUNC_NAME

SCM_DEFINE (
  s_zrythm_message,
  "zrythm-message", 1, 0, 0,
  (SCM message),
  "Writes the message to the log.")
#define FUNC_NAME s_
{
  g_message ("%s", scm_to_locale_string (message));

  return SCM_BOOL_T;
}
#undef FUNC_NAME

static void
init_module (void * data)
{
#ifndef SNARF_MODE
#include "zrythm.x"
#endif
  scm_c_export (
    "zrythm-get-ver",
    "zrythm-get-plugin-manager",
    "zrythm-get-project",
    "zrythm-message",
    "zrythm-null",
    NULL);
}

void
guile_zrythm_define_module (void)
{
  scm_c_define_module (
    "zrythm", init_module, NULL);
}
