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

#include "actions/tracklist_selections.h"
#include "audio/midi_region.h"
#include "audio/region.h"
#include "audio/transport.h"
#include "project.h"
#include "utils/flags.h"
#include "utils/io.h"
#include "zrythm.h"

#include "tests/helpers/project.h"
#include "tests/helpers/zrythm.h"

static void
test_get_chord_at_pos (void)
{
  test_helper_zrythm_init ();

  Position p1, p2, loop;

  test_project_rebootstrap_timeline (&p1, &p2);

  position_set_to_bar (&p1, 12);
  position_set_to_bar (&p2, 24);
  position_set_to_bar (&loop, 5);

  ZRegion * r = P_CHORD_TRACK->chord_regions[0];
  ChordObject * co1 = r->chord_objects[0];
  ChordObject * co2 = r->chord_objects[1];
  co1->chord_index = 0;
  co2->chord_index = 2;
  arranger_object_set_end_pos_full_size (
    (ArrangerObject *) r, &p2);
  arranger_object_set_start_pos_full_size (
    (ArrangerObject *) r, &p1);
  arranger_object_loop_end_pos_setter (
    (ArrangerObject *) r, &loop);

  region_print (r);

  Position pos;
  ChordObject * co;

  position_init (&pos);
  co =
    chord_track_get_chord_at_pos (
      P_CHORD_TRACK, &pos);
  g_assert_null (co);

  position_set_to_bar (&pos, 2);
  co =
    chord_track_get_chord_at_pos (
      P_CHORD_TRACK, &pos);
  g_assert_null (co);

  position_set_to_bar (&pos, 3);
  co =
    chord_track_get_chord_at_pos (
      P_CHORD_TRACK, &pos);
  g_assert_null (co);

  position_set_to_bar (&pos, 12);
  co =
    chord_track_get_chord_at_pos (
      P_CHORD_TRACK, &pos);
  g_assert_null (co);

  position_set_to_bar (&pos, 13);
  co =
    chord_track_get_chord_at_pos (
      P_CHORD_TRACK, &pos);
  g_assert_true (co == co1);

  position_set_to_bar (&pos, 14);
  co =
    chord_track_get_chord_at_pos (
      P_CHORD_TRACK, &pos);
  g_assert_true (co == co1);

  position_set_to_bar (&pos, 15);
  co =
    chord_track_get_chord_at_pos (
      P_CHORD_TRACK, &pos);
  g_assert_true (co == co2);

  position_set_to_bar (&pos, 16);
  co =
    chord_track_get_chord_at_pos (
      P_CHORD_TRACK, &pos);
  g_assert_null (co);

  position_set_to_bar (&pos, 17);
  co =
    chord_track_get_chord_at_pos (
      P_CHORD_TRACK, &pos);
  g_assert_true (co == co1);

  position_set_to_bar (&pos, 18);
  co =
    chord_track_get_chord_at_pos (
      P_CHORD_TRACK, &pos);
  g_assert_true (co == co1);

  position_set_to_bar (&pos, 19);
  co =
    chord_track_get_chord_at_pos (
      P_CHORD_TRACK, &pos);
  g_assert_true (co == co2);

  position_set_to_bar (&pos, 100);
  co =
    chord_track_get_chord_at_pos (
      P_CHORD_TRACK, &pos);
  g_assert_null (co);

  test_helper_zrythm_cleanup ();
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

#define TEST_PREFIX "/audio/chord track/"

  g_test_add_func (
    TEST_PREFIX "test get chord at pos",
    (GTestFunc) test_get_chord_at_pos);

  return g_test_run ();
}
