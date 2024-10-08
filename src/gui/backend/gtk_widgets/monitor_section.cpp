// SPDX-FileCopyrightText: © 2019-2021, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "common/dsp/control_port.h"
#include "common/dsp/control_room.h"
#include "common/dsp/engine.h"
#include "common/dsp/engine_jack.h"
#include "common/dsp/fader.h"
#include "common/dsp/tracklist.h"
#include "common/utils/error.h"
#include "common/utils/flags.h"
#include "common/utils/gtk.h"
#include "common/utils/math.h"
#include "common/utils/resources.h"
#include "gui/backend/backend/actions/tracklist_selections.h"
#include "gui/backend/backend/project.h"
#include "gui/backend/backend/settings/g_settings_manager.h"
#include "gui/backend/backend/settings/settings.h"
#include "gui/backend/backend/zrythm.h"
#include "gui/backend/gtk_widgets/active_hardware_mb.h"
#include "gui/backend/gtk_widgets/knob.h"
#include "gui/backend/gtk_widgets/knob_with_name.h"
#include "gui/backend/gtk_widgets/monitor_section.h"
#include "gui/backend/gtk_widgets/zrythm_app.h"

#include <glib/gi18n.h>

G_DEFINE_TYPE (MonitorSectionWidget, monitor_section_widget, GTK_TYPE_BOX)

void
monitor_section_widget_refresh (MonitorSectionWidget * self)
{
  int num_muted = TRACKLIST->get_num_muted_tracks ();
  int num_soloed = TRACKLIST->get_num_soloed_tracks ();
  int num_listened = TRACKLIST->get_num_listened_tracks ();

  char str[200];
  snprintf (str, 200, _ ("<small>%d muted</small>"), num_muted);
  gtk_label_set_markup (self->muted_tracks_lbl, str);
  gtk_widget_set_tooltip_text (
    GTK_WIDGET (self->muted_tracks_lbl), _ ("Currently muted tracks"));

  snprintf (str, 200, _ ("<small>%d soloed</small>"), num_soloed);
  gtk_label_set_markup (self->soloed_tracks_lbl, str);
  gtk_widget_set_tooltip_text (
    GTK_WIDGET (self->soloed_tracks_lbl), _ ("Currently soloed tracks"));

  snprintf (str, 200, _ ("<small>%d listened</small>"), num_listened);
  gtk_label_set_markup (self->listened_tracks_lbl, str);
  gtk_widget_set_tooltip_text (
    GTK_WIDGET (self->listened_tracks_lbl), _ ("Currently listened tracks"));

  gtk_widget_set_sensitive (GTK_WIDGET (self->soloing_btn), num_soloed > 0);
  gtk_widget_set_sensitive (GTK_WIDGET (self->muting_btn), num_muted > 0);
  gtk_widget_set_sensitive (GTK_WIDGET (self->listening_btn), num_listened > 0);
}

static void
on_unsolo_all_clicked (GtkButton * btn, MonitorSectionWidget * self)
{
  /* remember selections */
  auto tracks_before = TRACKLIST_SELECTIONS->track_names_;

  /* unsolo all */
  TRACKLIST->select_all (true, false);
  try
    {
      UNDO_MANAGER->perform (std::make_unique<SoloTracksAction> (
        *TRACKLIST_SELECTIONS->gen_tracklist_selections (), false));
    }
  catch (const ZrythmException &e)
    {
      e.handle (_ ("Failed to unsolo all tracks"));
    }

  /* restore selections */
  for (auto &track_name : tracks_before)
    {
      auto tr = TRACKLIST->find_track_by_name (track_name);
      tr->select (true, track_name == tracks_before[0], false);
    }
}

static void
on_unmute_all_clicked (GtkButton * btn, MonitorSectionWidget * self)
{
  /* remember selections */
  auto tracks_before = TRACKLIST_SELECTIONS->track_names_;

  /* unmute all */
  TRACKLIST->select_all (true, false);
  try
    {
      UNDO_MANAGER->perform (std::make_unique<MuteTracksAction> (
        *TRACKLIST_SELECTIONS->gen_tracklist_selections (), false));
    }
  catch (const ZrythmException &e)
    {
      e.handle (_ ("Failed to unmute all tracks"));
    }

  /* restore selections */
  for (auto &track_name : tracks_before)
    {
      auto tr = TRACKLIST->find_track_by_name (track_name);
      tr->select (true, track_name == tracks_before[0], false);
    }
}

static void
on_unlisten_all_clicked (GtkButton * btn, MonitorSectionWidget * self)
{
  /* remember selections */
  auto tracks_before = TRACKLIST_SELECTIONS->track_names_;

  /* unlisten all */
  TRACKLIST->select_all (true, false);
  try
    {
      UNDO_MANAGER->perform (std::make_unique<ListenTracksAction> (
        *TRACKLIST_SELECTIONS->gen_tracklist_selections (), false));
    }
  catch (const ZrythmException &e)
    {
      e.handle (_ ("Failed to unlisten all tracks"));
    }

  /* restore selections */
  for (auto &track_name : tracks_before)
    {
      auto tr = TRACKLIST->find_track_by_name (track_name);
      tr->select (true, track_name == tracks_before[0], false);
    }
}

static void
on_mono_toggled (GtkToggleButton * btn, MonitorSectionWidget * self)
{
  bool active = gtk_toggle_button_get_active (btn);
  MONITOR_FADER->set_mono_compat_enabled (active, false);
  g_settings_set_boolean (S_MONITOR, "mono", active);
}

static void
on_dim_toggled (GtkToggleButton * btn, MonitorSectionWidget * self)
{
  bool active = gtk_toggle_button_get_active (btn);
  CONTROL_ROOM->dim_output_ = active;
  g_settings_set_boolean (S_MONITOR, "dim-output", active);
}

static void
on_mute_toggled (GtkToggleButton * btn, MonitorSectionWidget * self)
{
  bool active = gtk_toggle_button_get_active (btn);
  MONITOR_FADER->mute_->control_ = active ? 1.f : 0.f;
  g_settings_set_boolean (S_MONITOR, "mute", active);
}

static void
on_devices_updated (MonitorSectionWidget * self)
{
#if HAVE_JACK
  /* reconnect to devices */
  GError * err = NULL;
  bool ret = engine_jack_reconnect_monitor (AUDIO_ENGINE.get (), true, &err);
  if (!ret)
    {
      HANDLE_ERROR (
        err, "%s", _ ("Failed to connect to left monitor output port"));
      return;
    }
  ret = engine_jack_reconnect_monitor (AUDIO_ENGINE.get (), false, &err);
  if (!ret)
    {
      HANDLE_ERROR (
        err, "%s", _ ("Failed to connect to right monitor output port"));
      return;
    }
#endif
}

void
monitor_section_widget_setup (
  MonitorSectionWidget * self,
  ControlRoom *          control_room)
{
  self->control_room = control_room;

  auto create_and_add_knob =
    [] (
      auto &knob_owner, Fader &fader, auto &container, const char * label,
      const auto size) {
      auto knob = knob_widget_new_simple (
        bind_member_function (fader, &Fader::get_fader_val),
        bind_member_function (fader, &Fader::get_default_fader_val),
        bind_member_function (fader, &Fader::set_fader_val), &fader, 0.f, 1.f,
        size, 0.f);
      knob->hover_str_getter =
        bind_member_function (fader, &Fader::db_string_getter);
      knob_owner = knob_with_name_widget_new (
        nullptr, [label] () { return label; }, nullptr, knob,
        GTK_ORIENTATION_VERTICAL, false, 2);
      gtk_box_append (container, GTK_WIDGET (knob_owner));
    };

  create_and_add_knob (
    self->monitor_level, *MONITOR_FADER, self->monitor_level_box, _ ("Monitor"),
    78);
  constexpr int basic_knob_size = 52;
  create_and_add_knob (
    self->mute_level, *CONTROL_ROOM->mute_fader_, self->mute_level_box,
    _ ("Mute"), basic_knob_size);
  create_and_add_knob (
    self->listen_level, *CONTROL_ROOM->listen_fader_, self->listen_level_box,
    _ ("Listen"), basic_knob_size);
  create_and_add_knob (
    self->dim_level, *CONTROL_ROOM->dim_fader_, self->dim_level_box, _ ("Dim"),
    basic_knob_size);

  z_gtk_button_set_icon_name_and_text (
    GTK_BUTTON (self->mono_toggle), "codicons-merge", _ ("Mono"), true,
    GTK_ORIENTATION_HORIZONTAL, 1);
  gtk_toggle_button_set_active (
    self->mono_toggle, MONITOR_FADER->get_mono_compat_enabled ());

  z_gtk_button_set_icon_name_and_text (
    GTK_BUTTON (self->dim_toggle), "dim", _ ("Dim"), true,
    GTK_ORIENTATION_HORIZONTAL, 1);
  gtk_toggle_button_set_active (self->dim_toggle, CONTROL_ROOM->dim_output_);

  z_gtk_button_set_icon_name_and_text (
    GTK_BUTTON (self->mute_toggle), "mute", _ ("Mute"), true,
    GTK_ORIENTATION_HORIZONTAL, 1);
  gtk_toggle_button_set_active (
    self->mute_toggle, MONITOR_FADER->mute_->is_toggled ());

  /* left/right outputs */
  if (AUDIO_ENGINE->audio_backend_ == AudioBackend::AUDIO_BACKEND_JACK)
    {
      self->left_outputs = active_hardware_mb_widget_new ();
      active_hardware_mb_widget_setup (
        self->left_outputs, F_NOT_INPUT, F_NOT_MIDI, S_MONITOR, "l-devices");
      self->left_outputs->callback = [self] () { on_devices_updated (self); };
      gtk_box_append (
        GTK_BOX (self->left_output_box), GTK_WIDGET (self->left_outputs));

      self->right_outputs = active_hardware_mb_widget_new ();
      active_hardware_mb_widget_setup (
        self->right_outputs, F_NOT_INPUT, F_NOT_MIDI, S_MONITOR, "r-devices");
      self->right_outputs->callback = [self] () { on_devices_updated (self); };
      gtk_box_append (
        GTK_BOX (self->right_output_box), GTK_WIDGET (self->right_outputs));
    }
  else
    {
      gtk_widget_set_visible (GTK_WIDGET (self->l_label), false);
      gtk_widget_set_visible (GTK_WIDGET (self->r_label), false);
    }

  /* set tooltips */
  gtk_widget_set_tooltip_text (
    GTK_WIDGET (self->soloing_btn), _ ("Unsolo all tracks"));
  gtk_widget_set_tooltip_text (
    GTK_WIDGET (self->muting_btn), _ ("Unmute all tracks"));
  gtk_widget_set_tooltip_text (
    GTK_WIDGET (self->listening_btn), _ ("Unlisten all tracks"));

  /* set signals */
  g_signal_connect (
    G_OBJECT (self->mono_toggle), "toggled", G_CALLBACK (on_mono_toggled), self);
  g_signal_connect (
    G_OBJECT (self->dim_toggle), "toggled", G_CALLBACK (on_dim_toggled), self);
  g_signal_connect (
    G_OBJECT (self->mute_toggle), "toggled", G_CALLBACK (on_mute_toggled), self);

  g_signal_connect (
    G_OBJECT (self->soloing_btn), "clicked", G_CALLBACK (on_unsolo_all_clicked),
    self);
  g_signal_connect (
    G_OBJECT (self->muting_btn), "clicked", G_CALLBACK (on_unmute_all_clicked),
    self);
  g_signal_connect (
    G_OBJECT (self->listening_btn), "clicked",
    G_CALLBACK (on_unlisten_all_clicked), self);

  monitor_section_widget_refresh (self);
}

/**
 * Creates a MonitorSectionWidget.
 */
MonitorSectionWidget *
monitor_section_widget_new (void)
{
  MonitorSectionWidget * self = static_cast<MonitorSectionWidget *> (
    g_object_new (MONITOR_SECTION_WIDGET_TYPE, nullptr));

  return self;
}

static void
monitor_section_widget_init (MonitorSectionWidget * self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

#if 0
  z_gtk_button_set_icon_name_and_text (
    GTK_BUTTON (self->soloing_toggle), "solo",
    _("Soloing"), true,
    GTK_ORIENTATION_HORIZONTAL, 1);
  z_gtk_button_set_icon_name_and_text (
    GTK_BUTTON (self->muting_toggle), "mute",
    _("Muting"), true,
    GTK_ORIENTATION_HORIZONTAL, 1);
  z_gtk_button_set_icon_name_and_text (
    GTK_BUTTON (self->listening_toggle), "listen",
    _("Listening"), true,
    GTK_ORIENTATION_HORIZONTAL, 1);
#endif
}

static void
monitor_section_widget_class_init (MonitorSectionWidgetClass * _klass)
{
  GtkWidgetClass * klass = GTK_WIDGET_CLASS (_klass);
  resources_set_class_template (klass, "monitor_section.ui");

  gtk_widget_class_set_css_name (klass, "control-room");

#define BIND_CHILD(x) \
  gtk_widget_class_bind_template_child (klass, MonitorSectionWidget, x)

  BIND_CHILD (soloing_btn);
  BIND_CHILD (soloed_tracks_lbl);
  BIND_CHILD (muting_btn);
  BIND_CHILD (muted_tracks_lbl);
  BIND_CHILD (listening_btn);
  BIND_CHILD (listened_tracks_lbl);
  BIND_CHILD (mute_level_box);
  BIND_CHILD (listen_level_box);
  BIND_CHILD (dim_level_box);
  BIND_CHILD (mono_toggle);
  BIND_CHILD (dim_toggle);
  BIND_CHILD (mute_toggle);
  BIND_CHILD (monitor_level_box);
  BIND_CHILD (left_output_box);
  BIND_CHILD (l_label);
  BIND_CHILD (right_output_box);
  BIND_CHILD (r_label);

#undef BIND_CHILD
}
