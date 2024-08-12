// SPDX-FileCopyrightText: © 2019-2022, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "dsp/chord_region.h"
#include "dsp/chord_track.h"
#include "dsp/tracklist.h"
#include "gui/widgets/bot_dock_edge.h"
#include "gui/widgets/center_dock.h"
#include "gui/widgets/chord_arranger.h"
#include "gui/widgets/chord_editor_space.h"
#include "gui/widgets/clip_editor.h"
#include "gui/widgets/clip_editor_inner.h"
#include "gui/widgets/main_window.h"
#include "project.h"
#include "zrythm.h"
#include "zrythm_app.h"

#include <glib/gi18n.h>

ChordRegion::
  ChordRegion (const Position &start_pos, const Position &end_pos, int idx)
{
  id_.type_ = RegionType::Chord;

  init (start_pos, end_pos, P_CHORD_TRACK->get_name_hash (), 0, idx);
}

bool
ChordRegion::validate (bool is_project, double frames_per_tick) const
{
  int idx = 0;
  for (auto &chord : chord_objects_)
    {
      z_return_val_if_fail (chord->index_ == idx++, false);
    }

  if (
    !Region::are_members_valid (is_project)
    || !TimelineObject::are_members_valid (is_project)
    || !NameableObject::are_members_valid (is_project)
    || !LoopableObject::are_members_valid (is_project)
    || !MuteableObject::are_members_valid (is_project)
    || !LengthableObject::are_members_valid (is_project)
    || !ColoredObject::are_members_valid (is_project)
    || !ArrangerObject::are_members_valid (is_project))
    {
      return false;
    }

  return true;
}

ArrangerSelections *
ChordRegion::get_arranger_selections () const
{
  return CHORD_SELECTIONS.get ();
}

ArrangerWidget *
ChordRegion::get_arranger_for_children () const
{
  return MW_CHORD_ARRANGER;
}