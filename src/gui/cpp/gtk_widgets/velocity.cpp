// SPDX-FileCopyrightText: © 2019-2022, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "common/dsp/midi_note.h"
#include "common/dsp/region.h"
#include "common/utils/gtk.h"
#include "common/utils/ui.h"
#include "gui/cpp/backend/project.h"
#include "gui/cpp/gtk_widgets/arranger.h"
#include "gui/cpp/gtk_widgets/gtk_wrapper.h"
#include "gui/cpp/gtk_widgets/midi_modifier_arranger.h"
#include "gui/cpp/gtk_widgets/midi_note.h"
#include "gui/cpp/gtk_widgets/velocity.h"
#include "gui/cpp/gtk_widgets/zrythm_app.h"

/**
 * Draws the Velocity in the given cairo context in
 * relative coordinates.
 */
void
velocity_draw (Velocity * self, GtkSnapshot * snapshot)
{
  MidiNote *       mn = self->get_midi_note ();
  ArrangerWidget * arranger = self->get_arranger ();

  /* get color */
  Color color;
  midi_note_get_adjusted_color (mn, color);

  /* make velocity start at 0,0 to make it easier to
   * draw */
  gtk_snapshot_save (snapshot);
  {
    graphene_point_t tmp_pt = Z_GRAPHENE_POINT_INIT (
      (float) self->full_rect_.x, (float) self->full_rect_.y);
    gtk_snapshot_translate (snapshot, &tmp_pt);
  }

  /* --- draw --- */

  const int circle_radius = self->full_rect_.width / 2;

  /* draw line */
  {
    graphene_rect_t tmp_r = Z_GRAPHENE_RECT_INIT (
      (float) self->full_rect_.width / 2.f - VELOCITY_LINE_WIDTH / 2.f,
      (float) circle_radius, VELOCITY_LINE_WIDTH,
      (float) self->full_rect_.height);
    z_gtk_snapshot_append_color (snapshot, color, &tmp_r);
  }

  /*
   * draw circle:
   * 1. translate by a little because we add an
   * extra pixel to VELOCITY_WIDTH to mimic previous
   * behavior
   * 2. push a circle clip and fill with white
   * 3. append colored border
   */
  gtk_snapshot_save (snapshot);
  {
    graphene_point_t tmp_pt = Z_GRAPHENE_POINT_INIT (-0.5f, -0.5f);
    gtk_snapshot_translate (snapshot, &tmp_pt);
  }
  float           circle_angle = 2.f * (float) M_PI;
  GskRoundedRect  rounded_rect;
  graphene_rect_t tmp_r = Z_GRAPHENE_RECT_INIT (
    0.f, 0.f, (float) circle_radius * 2.f + 1.f,
    (float) circle_radius * 2.f + 1.f);
  gsk_rounded_rect_init_from_rect (&rounded_rect, &tmp_r, circle_angle);
  gtk_snapshot_push_rounded_clip (snapshot, &rounded_rect);
  GdkRGBA tmp_color = Z_GDK_RGBA_INIT (0.8, 0.8, 0.8, 1);
  gtk_snapshot_append_color (snapshot, &tmp_color, &rounded_rect.bounds);
  const float border_width = 2.f;
  float       border_widths[] = {
    border_width, border_width, border_width, border_width
  };
  GdkRGBA border_colors[] = {
    color.to_gdk_rgba (), color.to_gdk_rgba (), color.to_gdk_rgba (),
    color.to_gdk_rgba ()
  };
  gtk_snapshot_append_border (
    snapshot, &rounded_rect, border_widths, border_colors);
  gtk_snapshot_pop (snapshot);
  gtk_snapshot_restore (snapshot);

  /* draw text */
  if (arranger->action != UiOverlayAction::None)
    {
      char text[8];
      sprintf (text, "%d", self->vel_);
      const int padding = 3;
      int       text_start_y = padding;
      int       text_start_x = self->full_rect_.width + padding;

      gtk_snapshot_save (snapshot);
      {
        graphene_point_t tmp_pt =
          Z_GRAPHENE_POINT_INIT ((float) text_start_x, (float) text_start_y);
        gtk_snapshot_translate (snapshot, &tmp_pt);
      }
      auto &layout = arranger->vel_layout;
      pango_layout_set_text (layout.get (), text, -1);
      GdkRGBA tmp_color2 = Z_GDK_RGBA_INIT (1, 1, 1, 1);
      gtk_snapshot_append_layout (snapshot, layout.get (), &tmp_color2);
      gtk_snapshot_restore (snapshot);
    }

  gtk_snapshot_restore (snapshot);
}

#if 0
/**
 * Creates a velocity.
 */
VelocityWidget *
velocity_widget_new (Velocity * velocity)
{
  VelocityWidget * self =
    g_object_new (VELOCITY_WIDGET_TYPE,
                  nullptr);

  arranger_object_widget_setup (
    Z_ARRANGER_OBJECT_WIDGET (self),
    (ArrangerObject *) velocity);

  self->velocity = velocity;

  return self;
}
#endif
