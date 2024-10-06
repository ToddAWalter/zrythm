// SPDX-FileCopyrightText: © 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "common/dsp/timeline_object.h"
#include "common/dsp/track.h"
#include "gui/backend/gtk_widgets/arranger.h"
#include "gui/backend/gtk_widgets/center_dock.h"
#include "gui/backend/gtk_widgets/main_notebook.h"
#include "gui/backend/gtk_widgets/timeline_arranger.h"
#include "gui/backend/gtk_widgets/timeline_panel.h"
#include "gui/backend/gtk_widgets/zrythm_app.h"

void
TimelineObject::copy_members_from (const TimelineObject &other)
{
}

void
TimelineObject::init_loaded_base ()
{
}

ArrangerWidget *
TimelineObject::get_arranger () const
{
  Track * track = get_track ();
  z_return_val_if_fail (track, nullptr);
  if (track->is_pinned ())
    {
      return (ArrangerWidget *) (MW_PINNED_TIMELINE);
    }
  else
    {
      return (ArrangerWidget *) (MW_TIMELINE);
    }
}