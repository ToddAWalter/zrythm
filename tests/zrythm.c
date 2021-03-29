/*
 * Copyright (C) 2021 Alexandros Theodotou <alex at zrythm dot org>
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

#include "zrythm-test-config.h"

#include "audio/track.h"
#include "audio/tempo_track.h"
#include "project.h"
#include "utils/flags.h"
#include "zrythm.h"

#include "helpers/zrythm.h"

#include <glib.h>
#include <locale.h>

static void
test_dummy ()
{
  test_helper_zrythm_init ();
  test_helper_zrythm_cleanup ();
}

static void
test_fetch_latest_release ()
{
#ifdef PHONE_HOME
  char * ver =
    zrythm_fetch_latest_release_ver ();
  g_assert_nonnull (ver);
  g_assert_cmpuint (strlen (ver), <, 20);
#endif
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

#define TEST_PREFIX "/zrythm/"

  g_test_add_func (
    TEST_PREFIX "test dummy",
    (GTestFunc) test_dummy);
  g_test_add_func (
    TEST_PREFIX "test fetch latest release",
    (GTestFunc) test_fetch_latest_release);

  return g_test_run ();
}
