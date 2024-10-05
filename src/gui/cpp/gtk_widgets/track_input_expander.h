// SPDX-FileCopyrightText: © 2019-2020, 2023-2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#ifndef __GUI_WIDGETS_TRACK_INPUT_EXPANDER_H__
#define __GUI_WIDGETS_TRACK_INPUT_EXPANDER_H__

#include "gui/cpp/gtk_widgets/two_col_expander_box.h"
#include "utils/types.h"

#include "gtk_wrapper.h"

#define TRACK_INPUT_EXPANDER_WIDGET_TYPE \
  (track_input_expander_widget_get_type ())
G_DECLARE_FINAL_TYPE (
  TrackInputExpanderWidget,
  track_input_expander_widget,
  Z,
  TRACK_INPUT_EXPANDER_WIDGET,
  TwoColExpanderBoxWidget);

class ChannelTrack;
TYPEDEF_STRUCT_UNDERSCORED (EditableLabelWidget);
TYPEDEF_STRUCT_UNDERSCORED (KnobWidget);

/**
 * @addtogroup widgets
 *
 * @{
 */

using TrackInputExpanderWidget = struct _TrackInputExpanderWidget
{
  TwoColExpanderBoxWidget parent_instance;

  /** Track input port for midi. */
  GtkDropDown * midi_input;

  /** Track input port for audio L. */
  GtkDropDown * stereo_l_input;

  /** Track input port for audio R. */
  GtkDropDown * stereo_r_input;

  /** MIDI channels selector. */
  GtkDropDown * midi_channels;

  /** Size group for audio inputs. */
  GtkSizeGroup * audio_input_size_group;

  /** Mono switch for audio tracks. */
  GtkToggleButton * mono;

  /** Gain knob for audio tracks. */
  GtkBox *     gain_box;
  KnobWidget * gain;

  /** Track the TrackInputExpanderWidget is associated with. */
  ChannelTrack * track;
};

/**
 * Refreshes each field.
 *
 * @memberof TrackInputExpander
 */
void
track_input_expander_widget_refresh (
  TrackInputExpanderWidget * self,
  ChannelTrack *             track);

/**
 * Sets up the TrackInputExpanderWidget.
 */
void
track_input_expander_widget_setup (
  TrackInputExpanderWidget * self,
  ChannelTrack *             track);

/**
 * @}
 */

#endif
