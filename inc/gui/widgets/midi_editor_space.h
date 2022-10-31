// SPDX-FileCopyrightText: © 2018-2019, 2021-2022 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

/**
 * \file
 *
 * Piano roll widget.
 */

#ifndef __GUI_WIDGETS_MIDI_EDITOR_SPACE_H__
#define __GUI_WIDGETS_MIDI_EDITOR_SPACE_H__

#include <gtk/gtk.h>

#define MIDI_EDITOR_SPACE_WIDGET_TYPE \
  (midi_editor_space_widget_get_type ())
G_DECLARE_FINAL_TYPE (
  MidiEditorSpaceWidget,
  midi_editor_space_widget,
  Z,
  MIDI_EDITOR_SPACE_WIDGET,
  GtkWidget)

typedef struct _ArrangerWidget      ArrangerWidget;
typedef struct _PianoRollKeysWidget PianoRollKeysWidget;

/**
 * @addtogroup widgets
 *
 * @{
 */

#define MW_MIDI_EDITOR_SPACE \
  MW_CLIP_EDITOR_INNER->midi_editor_space

/**
 * The piano roll widget is the whole space inside
 * the clip editor tab when a MIDI region is selected.
 */
typedef struct _MidiEditorSpaceWidget
{
  GtkWidget parent_instance;

  GtkPaned * midi_arranger_velocity_paned;

  GtkScrolledWindow * piano_roll_keys_scroll;
  GtkViewport *       piano_roll_keys_viewport;

  GtkBox * midi_notes_box;

  PianoRollKeysWidget * piano_roll_keys;

  /** Piano roll. */
  GtkBox *         midi_arranger_box;
  ArrangerWidget * arranger;
  ArrangerWidget * modifier_arranger;

  GtkBox *          midi_vel_chooser_box;
  GtkComboBoxText * midi_modifier_chooser;

  /** Vertical size goup for the keys and the
   * arranger. */
  GtkSizeGroup * arranger_and_keys_vsize_group;
} MidiEditorSpaceWidget;

void
midi_editor_space_widget_setup (MidiEditorSpaceWidget * self);

/**
 * See CLIP_EDITOR_INNER_WIDGET_ADD_TO_SIZEGROUP.
 */
void
midi_editor_space_widget_update_size_group (
  MidiEditorSpaceWidget * self,
  int                     visible);

/**
 * Updates the scroll adjustment.
 */
void
midi_editor_space_widget_set_piano_keys_scroll_start_y (
  MidiEditorSpaceWidget * self,
  int                     y);

void
midi_editor_space_widget_refresh (
  MidiEditorSpaceWidget * self);

/**
 * @}
 */

#endif
