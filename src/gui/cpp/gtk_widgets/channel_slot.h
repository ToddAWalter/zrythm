// SPDX-FileCopyrightText: © 2019-2023 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

/**
 * @file
 *
 * Channel slot.
 */

#ifndef __GUI_WIDGETS_CHANNEL_SLOT_H__
#define __GUI_WIDGETS_CHANNEL_SLOT_H__

#include "gui/cpp/gtk_widgets/gtk_wrapper.h"

#include "common/plugins/plugin.h"
#include "common/utils/types.h"

#define CHANNEL_SLOT_WIDGET_TYPE (channel_slot_widget_get_type ())
G_DECLARE_FINAL_TYPE (
  ChannelSlotWidget,
  channel_slot_widget,
  Z,
  CHANNEL_SLOT_WIDGET,
  GtkWidget)

class Plugin;
class Channel;
class ChannelTrack;
TYPEDEF_STRUCT_UNDERSCORED (ChannelSlotActivateButtonWidget);

/**
 * @addtogroup widgets
 *
 * @{
 */

using ChannelSlotWidget = struct _ChannelSlotWidget
{
  GtkWidget parent_instance;

  PluginSlotType type;

  /** The Track this belongs to. */
  ChannelTrack * track;

  /** The Channel slot index. */
  int               slot_index;
  GtkGestureClick * click;
  GtkGestureDrag *  drag;

  /**
   * Previous plugin name at this slot in the last draw callback, or NULL.
   *
   * If this changes, the tooltip is changed.
   */
  char * pl_name;

  /** For multipress. */
  int n_press;

  GtkGestureClick * right_mouse_mp;

  PangoLayout * txt_layout;

  /** Flag used for adding/removing .empty CSS
   * class. */
  bool was_empty;

  /** Cache to check if the selection state was
   * changed. */
  bool was_selected;

  /** Whether to open the plugin inspector on click
   * or not. */
  bool open_plugin_inspector_on_click;

  /** Popover to be reused for context menus. */
  GtkPopoverMenu * popover_menu;

  ChannelSlotActivateButtonWidget * activate_btn;

  GtkImage * bridge_icon;
};

/**
 * Creates a new ChannelSlot widget whose track
 * and plugin can change.
 */
ChannelSlotWidget *
channel_slot_widget_new_instrument ();

/**
 * Creates a new ChannelSlot widget and binds it to
 * the given value.
 */
ChannelSlotWidget *
channel_slot_widget_new (
  int            slot_index,
  ChannelTrack * track,
  PluginSlotType type,
  bool           open_plugin_inspector_on_click);

void
channel_slot_widget_set_instrument (
  ChannelSlotWidget * self,
  ChannelTrack *      track);

Plugin *
channel_slot_widget_get_plugin (ChannelSlotWidget * self);

/**
 * @}
 */

#endif
