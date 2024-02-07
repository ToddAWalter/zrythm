/*
 * SPDX-FileCopyrightText: © 2019 Alexandros Theodotou <alex@zrythm.org>
 *
 * SPDX-License-Identifier: LicenseRef-ZrythmLicense
 */

/**
 * \file
 *
 * PianoRoll selection info.
 */

#ifndef __GUI_WIDGETS_PIANO_ROLL_SELECTION_INFO_H__
#define __GUI_WIDGETS_PIANO_ROLL_SELECTION_INFO_H__

#include "dsp/region.h"
#include "gui/widgets/region.h"
#include "utils/ui.h"

#include <gtk/gtk.h>

#define EDITOR_SELECTION_INFO_WIDGET_TYPE \
  (editor_selection_info_widget_get_type ())
G_DECLARE_FINAL_TYPE (
  EditorSelectionInfoWidget,
  editor_selection_info_widget,
  Z,
  EDITOR_SELECTION_INFO_WIDGET,
  GtkStack);

#define MW_MAS_INFO MW_CLIP_EDITOR->editor_selections

typedef struct _SelectionInfoWidget   SelectionInfoWidget;
typedef struct MidiArrangerSelections MidiArrangerSelections;

/**
 * A widget for showing info about the current
 * PianoRollSelections.
 */
typedef struct _EditorSelectionInfoWidget
{
  GtkStack              parent_instance;
  GtkLabel *            no_selection_label;
  SelectionInfoWidget * selection_info;
} EditorSelectionInfoWidget;

/**
 * Populates the SelectionInfoWidget based on the
 * leftmost object selected.
 */
void
editor_selection_info_widget_refresh (
  EditorSelectionInfoWidget * self,
  MidiArrangerSelections *    mas);

#endif
