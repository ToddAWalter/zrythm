// SPDX-FileCopyrightText: © 2020-2022, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

/**
 * @file
 *
 * Channel slot.
 */

#ifndef __GUI_WIDGETS_FADER_BUTTONS_H__
#define __GUI_WIDGETS_FADER_BUTTONS_H__

#include "gui/cpp/gtk_widgets/gtk_wrapper.h"

#define FADER_BUTTONS_WIDGET_TYPE (fader_buttons_widget_get_type ())
G_DECLARE_FINAL_TYPE (
  FaderButtonsWidget,
  fader_buttons_widget,
  Z,
  FADER_BUTTONS_WIDGET,
  GtkBox)

class ChannelTrack;

/**
 * @addtogroup widgets
 *
 * @{
 */

using FaderButtonsWidget = struct _FaderButtonsWidget
{
  GtkBox parent_instance;

  GtkToggleButton * mono_compat;
  GtkToggleButton * solo;
  GtkToggleButton * mute;
  GtkToggleButton * record;
  GtkToggleButton * listen;
  GtkToggleButton * swap_phase;
  GtkButton *       e;

  /**
   * Signal handler IDs.
   */
  gulong mono_compat_toggled_handler_id;
  gulong record_toggled_handler_id;
  gulong solo_toggled_handler_id;
  gulong mute_toggled_handler_id;
  gulong listen_toggled_handler_id;
  gulong swap_phase_toggled_handler_id;

  /** Owner track. */
  ChannelTrack * track;

  /** Popover to be reused for context menus. */
  GtkPopoverMenu * popover_menu;
};

void
fader_buttons_widget_block_signal_handlers (FaderButtonsWidget * self);

void
fader_buttons_widget_unblock_signal_handlers (FaderButtonsWidget * self);

void
fader_buttons_widget_refresh (FaderButtonsWidget * self, ChannelTrack * track);

FaderButtonsWidget *
fader_buttons_widget_new (ChannelTrack * track);

/**
 * @}
 */

#endif
