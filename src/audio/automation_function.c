/*
 * Copyright (C) 2020 Alexandros Theodotou <alex at zrythm dot org>
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

#include "audio/engine.h"
#include "audio/automation_function.h"
#include "gui/backend/arranger_selections.h"
#include "gui/backend/event.h"
#include "gui/backend/event_manager.h"
#include "project.h"
#include "settings/settings.h"
#include "utils/flags.h"
#include "zrythm_app.h"

static void
flip (
  AutomationSelections * sel,
  bool                   vertical)
{
  for (int i = 0; i < sel->num_automation_points; i++)
    {
      AutomationPoint * ap =
        sel->automation_points[i];

      if (vertical)
        {
          automation_point_set_fvalue (
            ap, 1.f - ap->normalized_val,
            F_NORMALIZED, F_NO_PUBLISH_EVENTS);
          ap->curve_opts.curviness =
            - ap->curve_opts.curviness;
        }
      else
        {
          /* TODO */
        }
    }
}

/**
 * Applies the given action to the given selections.
 *
 * @param sel Selections to edit.
 * @param type Function type.
 */
void
automation_function_apply (
  ArrangerSelections *   sel,
  AutomationFunctionType type)
{
  g_message (
    "applying %s...",
    automation_function_type_to_string (type));

  switch (type)
    {
    case AUTOMATION_FUNCTION_FLIP_HORIZONTAL:
      /* TODO */
      break;
    case AUTOMATION_FUNCTION_FLIP_VERTICAL:
      flip ((AutomationSelections *) sel, true);
      break;
    }

  /* set last action */
  g_settings_set_int (
    S_UI, "automation-function", type);

  EVENTS_PUSH (ET_EDITOR_FUNCTION_APPLIED, NULL);
}
