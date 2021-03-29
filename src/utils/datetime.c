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

#include "utils/datetime.h"

#include <gtk/gtk.h>

#include <time.h>

/**
 * Returns the current datetime as a string.
 *
 * Must be free()'d by caller.
 */
char *
datetime_get_current_as_string ()
{

  time_t t = time (NULL);
#ifdef HAVE_LOCALTIME_R
  struct tm tm;
  struct tm * ret = localtime_r (&t, &tm);
  g_return_val_if_fail (ret, NULL);
#else
  struct tm tm = *localtime (&t);
#endif

  char * str =
    g_strdup_printf (
      "%d-%02d-%02d %02d:%02d:%02d",
      tm.tm_year + 1900,
      tm.tm_mon + 1,
      tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

  return str;
}

/**
 * Get the current datetime to be used in filenames,
 * eg, for the log file.
 */
char *
datetime_get_for_filename (void)
{
  GDateTime * datetime =
    g_date_time_new_now_local ();
  char * str_datetime =
    g_date_time_format (
      datetime,
      "%F_%H-%M-%S");
  g_date_time_unref (datetime);

  return str_datetime;
}
