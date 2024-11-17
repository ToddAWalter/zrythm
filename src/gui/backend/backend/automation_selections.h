// SPDX-FileCopyrightText: © 2019-2021, 2023-2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

/**
 * @file
 *
 * API for selections in the AutomationArrangerWidget.
 */

#ifndef __GUI_BACKEND_AUTOMATION_SELECTIONS_H__
#define __GUI_BACKEND_AUTOMATION_SELECTIONS_H__

#include "common/dsp/automation_point.h"
#include "gui/backend/backend/arranger_selections.h"

/**
 * @addtogroup gui_backend
 *
 * @{
 */

#define AUTOMATION_SELECTIONS (PROJECT->automation_selections_)

/**
 * Selections to be used for the AutomationArrangerWidget's current selections,
 * copying, undoing, etc.
 */
class AutomationSelections final
    : public QObject,
      public ArrangerSelections,
      public ICloneable<AutomationSelections>,
      public ISerializable<AutomationSelections>
{
  Q_OBJECT
  QML_ELEMENT
  DEFINE_ARRANGER_SELECTIONS_QML_PROPERTIES (AutomationSelections)

public:
  AutomationSelections (QObject * parent = nullptr);

  const AutomationPoint * get_automation_point (int index) const
  {
    return std::get<AutomationPoint *> (objects_.at (index));
  }

  void sort_by_indices (bool desc) override;

  void init_after_cloning (const AutomationSelections &other) override
  {
    ArrangerSelections::copy_members_from (other);
  }

  DECLARE_DEFINE_FIELDS_METHOD ();

private:
  bool can_be_pasted_at_impl (Position pos, int idx) const final;
};

/**
 * @}
 */

#endif
