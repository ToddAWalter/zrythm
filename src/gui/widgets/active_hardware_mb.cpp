// SPDX-FileCopyrightText: © 2019-2022, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "gui/widgets/active_hardware_mb.h"
#include "gui/widgets/popovers/active_hardware_popover.h"
#include "utils/logger.h"
#include "utils/string_array.h"

#include <glib/gi18n.h>

#include "gtk_wrapper.h"

G_DEFINE_TYPE (ActiveHardwareMbWidget, active_hardware_mb_widget, GTK_TYPE_WIDGET)

static void
on_create_popover (GtkMenuButton * button, gpointer user_data)
{
  ActiveHardwareMbWidget * self = Z_ACTIVE_HARDWARE_MB_WIDGET (user_data);
  self->popover = active_hardware_popover_widget_new (self);
  gtk_menu_button_set_popover (button, GTK_WIDGET (self->popover));
}

#if 0
static void
set_label (ActiveHardwareMbWidget * self)
{
  gtk_label_set_text (
    self->label, _("Select..."));
}
#endif

/**
 * This is called when the popover closes.
 */
void
active_hardware_mb_widget_refresh (ActiveHardwareMbWidget * self)
{
  /*set_label (self);*/
  active_hardware_mb_widget_save_settings (self);
  if (self->callback)
    {
      self->callback ();
    }
}

void
active_hardware_mb_widget_save_settings (ActiveHardwareMbWidget * self)
{
  StringArray controllers;

  for (
    GtkWidget * child =
      gtk_widget_get_first_child (GTK_WIDGET (self->popover->controllers_box));
    child != nullptr; child = gtk_widget_get_next_sibling (child))
    {
      if (!GTK_IS_CHECK_BUTTON (child))
        continue;

      GtkCheckButton * chkbtn = GTK_CHECK_BUTTON (child);
      if (gtk_check_button_get_active (chkbtn))
        {
          controllers.add (gtk_check_button_get_label (chkbtn));
        }
    }

  int res = g_settings_set_strv (
    self->settings, self->key, controllers.getNullTerminated ());
  z_return_if_fail (res == 1);
}

void
active_hardware_mb_widget_setup (
  ActiveHardwareMbWidget * self,
  bool                     is_input,
  bool                     is_midi,
  GSettings *              settings,
  const char *             key)
{
  self->is_midi = is_midi;
  self->input = is_input;
  self->settings = settings;
  self->key = key;

  gtk_widget_set_tooltip_text (
    GTK_WIDGET (self->mbutton),
    is_input ? _ ("Click to enable inputs") : _ ("Click to enable outputs"));
}

ActiveHardwareMbWidget *
active_hardware_mb_widget_new (void)
{
  ActiveHardwareMbWidget * self = (ActiveHardwareMbWidget *) g_object_new (
    ACTIVE_HARDWARE_MB_WIDGET_TYPE, nullptr);

  return self;
}

static void
dispose (ActiveHardwareMbWidget * self)
{
  gtk_widget_unparent (GTK_WIDGET (self->mbutton));

  G_OBJECT_CLASS (active_hardware_mb_widget_parent_class)
    ->dispose (G_OBJECT (self));
}

static void
active_hardware_mb_widget_class_init (ActiveHardwareMbWidgetClass * klass)
{
  gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (klass), "active-hw-mb");
  gtk_widget_class_set_layout_manager_type (
    GTK_WIDGET_CLASS (klass), GTK_TYPE_BIN_LAYOUT);

  GObjectClass * oklass = G_OBJECT_CLASS (klass);
  oklass->dispose = (GObjectFinalizeFunc) dispose;
}

static void
active_hardware_mb_widget_init (ActiveHardwareMbWidget * self)
{
  self->mbutton = GTK_MENU_BUTTON (gtk_menu_button_new ());
  gtk_widget_set_hexpand (GTK_WIDGET (self->mbutton), true);
  gtk_widget_set_parent (GTK_WIDGET (self->mbutton), GTK_WIDGET (self));

  gtk_menu_button_set_label (self->mbutton, _ ("Select..."));

  gtk_menu_button_set_create_popup_func (
    self->mbutton, on_create_popover, self, nullptr);
}
