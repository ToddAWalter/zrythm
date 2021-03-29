/*
 * Copyright (C) 2019 Alexandros Theodotou <alex at zrythm dot org>
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

#include <stdlib.h>

#include "utils/env.h"

#include <glib.h>

/**
 * Returns a newly allocated string.
 *
 * @param def Default value to return if not found.
 */
char *
env_get_string (
  const char * key,
  const char * def)
{
  const char * val = g_getenv (key);
  if (!val)
    return g_strdup (def);
  return g_strdup (val);
}

/**
 * Returns an int for the given environment variable
 * if it exists and is valid, otherwise returns the
 * default int.
 *
 * @param def Default value to return if not found.
 */
int
env_get_int (
  const char * key,
  int          def)
{
  const char * str = g_getenv (key);
  if (!str)
    return def;

  int val = atoi (str);
  if (val == 0)
    return def;
  else
    return val;
}
