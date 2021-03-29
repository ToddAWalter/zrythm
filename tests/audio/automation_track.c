/*
 * Copyright (C) 2019-2020 Alexandros Theodotou <alex at zrythm dot org>
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

#include "actions/arranger_selections.h"
#include "audio/channel.h"
#include "audio/automation_region.h"
#include "audio/automation_track.h"
#include "audio/master_track.h"
#include "project.h"
#include "utils/arrays.h"
#include "zrythm.h"

#include "tests/helpers/zrythm.h"

#include <glib.h>
#include <locale.h>

static void
test_set_at_index ()
{
  test_helper_zrythm_init ();

  Track * master = P_MASTER_TRACK;
  track_set_automation_visible (master, true);
  AutomationTracklist * atl =
    track_get_automation_tracklist (master);
  AutomationTrack ** visible_ats =
    calloc (100000, sizeof (AutomationTrack *));
  int num_visible = 0;
  automation_tracklist_get_visible_tracks (
    atl, visible_ats, &num_visible);
  AutomationTrack * first_vis_at = visible_ats[0];

  /* create a region and set it as clip editor
   * region */
  Position start, end;
  position_set_to_bar (&start, 2);
  position_set_to_bar (&end, 4);
  ZRegion * region =
    automation_region_new (
      &start, &end, master->pos,
      first_vis_at->index, 0);
  track_add_region  (
    master, region, first_vis_at, -1, F_GEN_NAME,
    F_NO_PUBLISH_EVENTS);
  arranger_object_select (
    (ArrangerObject *) region, F_SELECT,
    F_NO_APPEND, F_NO_PUBLISH_EVENTS);
  UndoableAction * ua =
    arranger_selections_action_new_create (
      (ArrangerSelections *) TL_SELECTIONS);
  undo_manager_perform (
    UNDO_MANAGER, ua);

  clip_editor_set_region (
    CLIP_EDITOR, region, F_NO_PUBLISH_EVENTS);

  AutomationTrack * first_invisible_at =
    automation_tracklist_get_first_invisible_at (
      atl);
  automation_tracklist_set_at_index (
    atl, first_invisible_at, first_vis_at->index,
    F_NO_PUSH_DOWN);

  /* check that clip editor region can be found */
  clip_editor_get_region (CLIP_EDITOR);

  test_helper_zrythm_cleanup ();
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

#define TEST_PREFIX "/audio/automation_track/"

  g_test_add_func (
    TEST_PREFIX "test set at index",
    (GTestFunc) test_set_at_index);

  return g_test_run ();
}
