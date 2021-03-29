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

#include "audio/track.h"
#include "gui/widgets/track.h"
#include "project.h"
#include "utils/flags.h"
#include "zrythm.h"

#include "tests/helpers/fishbowl.h"
#include "tests/helpers/fishbowl_window.h"
#include "tests/helpers/zrythm.h"

typedef struct
{
  Region * midi_region;
} RegionFixture;

GtkWidget *
fishbowl_creation_func ()
{
  Track * track =
    track_new (
      TRACK_TYPE_INSTRUMENT, "track-label", 1);
  g_return_val_if_fail (track, NULL);
  TrackWidget * itw =
    track_widget_new (track);
  gtk_widget_set_size_request (
    GTK_WIDGET (itw), 300, 120);
  track_widget_force_redraw (
    Z_TRACK_WIDGET (itw));

  return GTK_WIDGET (itw);
}

static void
test_instrument_track_fishbowl ()
{
  guint count =
    fishbowl_window_widget_run (
      fishbowl_creation_func,
      DEFAULT_FISHBOWL_TIME);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  test_helper_zrythm_init ();
  test_helper_zrythm_gui_init (argc, argv);

#define TEST_PREFIX "/gui/widgets/track/"

  g_test_add_func (
    TEST_PREFIX "test instrument track fishbowl",
    (GTestFunc) test_instrument_track_fishbowl);

  return g_test_run ();
}
