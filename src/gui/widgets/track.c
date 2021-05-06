/*
 * Copyright (C) 2018-2021 Alexandros Theodotou <alex at zrythm dot org>
 *
 * This file is part of Zrythm
 *
 * Zrythm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zrythm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Zrythm.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "audio/automation_track.h"
#include "audio/audio_bus_track.h"
#include "audio/audio_group_track.h"
#include "audio/control_port.h"
#include "audio/exporter.h"
#include "audio/marker_track.h"
#include "audio/master_track.h"
#include "audio/instrument_track.h"
#include "audio/track.h"
#include "audio/tracklist.h"
#include "audio/region.h"
#include "gui/backend/tracklist_selections.h"
#include "gui/backend/event.h"
#include "gui/backend/event_manager.h"
#include "gui/widgets/arranger.h"
#include "gui/widgets/automatable_selector_popover.h"
#include "gui/widgets/automation_mode.h"
#include "gui/widgets/bot_bar.h"
#include "gui/widgets/bot_dock_edge.h"
#include "gui/widgets/center_dock.h"
#include "gui/widgets/color_area.h"
#include "gui/widgets/custom_button.h"
#include "gui/widgets/dialogs/bounce_dialog.h"
#include "gui/widgets/dialogs/export_progress_dialog.h"
#include "gui/widgets/dialogs/track_icon_chooser_dialog.h"
#include "gui/widgets/dialogs/object_color_chooser_dialog.h"
#include "gui/widgets/editable_label.h"
#include "gui/widgets/main_window.h"
#include "gui/widgets/main_notebook.h"
#include "gui/widgets/meter.h"
#include "gui/widgets/midi_activity_bar.h"
#include "gui/widgets/mixer.h"
#include "gui/widgets/timeline_arranger.h"
#include "gui/widgets/timeline_bg.h"
#include "gui/widgets/timeline_panel.h"
#include "gui/widgets/track.h"
#include "gui/widgets/track_top_grid.h"
#include "gui/widgets/tracklist.h"
#include "project.h"
#include "utils/cairo.h"
#include "utils/color.h"
#include "utils/flags.h"
#include "utils/gtk.h"
#include "utils/math.h"
#include "utils/resources.h"
#include "utils/string.h"
#include "utils/symap.h"
#include "utils/ui.h"
#include "zrythm_app.h"

#include <gtk/gtk.h>

#include <glib/gi18n.h>

G_DEFINE_TYPE (
  TrackWidget, track_widget, GTK_TYPE_BOX)

#define ICON_NAME_MONO_COMPAT "mono"
#define ICON_NAME_RECORD "media-record"
#define ICON_NAME_SOLO "solo"
#define ICON_NAME_MUTE "mute"
#define ICON_NAME_LISTEN "listen"
#define ICON_NAME_SHOW_UI "synth"
#define ICON_NAME_SHOW_AUTOMATION_LANES \
  "node-type-cusp"
#define ICON_NAME_SHOW_TRACK_LANES \
  "format-justify-fill"
#define ICON_NAME_LOCK "document-decrypt"
#define ICON_NAME_FREEZE "fork-awesome-snowflake-o"
#define ICON_NAME_PLUS "add"
#define ICON_NAME_MINUS "remove"
#define ICON_NAME_BUS "effect"
#define ICON_NAME_CHORDS "minuet-chords"
#define ICON_NAME_SHOW_MARKERS \
  "kdenlive-show-markers"
#define ICON_NAME_MIDI "instrument"
#define ICON_NAME_TEMPO "filename-bpm-amarok"
#define ICON_NAME_MODULATOR "modulator"

#ifdef _WOE32
#define NAME_FONT "9"
#define AUTOMATABLE_NAME_FONT "8.5"
#else
#define NAME_FONT "10"
#define AUTOMATABLE_NAME_FONT "9.5"
#endif

/**
 * Width of each meter: total 8 for MIDI, total
 * 16 for audio.
 */
#define METER_WIDTH 8

#define BUTTON_SIZE 18

/** Padding between each button. */
#define BUTTON_PADDING 6

/** Padding between the track edges and the
 * buttons */
#define BUTTON_PADDING_FROM_EDGE 3

#define COLOR_AREA_WIDTH 18

/** Width of the "label" to show the automation
 * value. */
#define AUTOMATION_VALUE_WIDTH 32

/** Pixels from the bottom edge to start resizing
 * at. */
#define RESIZE_PX 12

#define BOT_BUTTONS_SHOULD_BE_VISIBLE(height) \
  (height >= \
     (BUTTON_SIZE + \
        BUTTON_PADDING_FROM_EDGE) * 2 + \
     BUTTON_PADDING)

#define ICON_IS(x,name) \
  (string_is_equal (x, ICON_NAME_##name))

#define CB_ICON_IS(name) \
  ICON_IS (cb->icon_name, name)

static CustomButtonWidget *
get_hovered_button (
  TrackWidget * self,
  int           x,
  int           y)
{
#define IS_BUTTON_HOVERED \
  (x >= cb->x && \
   x <= cb->x + \
     (cb->width ? cb->width : BUTTON_SIZE) && \
   y >= cb->y && y <= cb->y + BUTTON_SIZE)
#define RETURN_IF_HOVERED \
  if (IS_BUTTON_HOVERED) return cb;

  CustomButtonWidget * cb = NULL;
  for (int i = 0; i < self->num_top_buttons; i++)
    {
      cb = self->top_buttons[i];
      RETURN_IF_HOVERED;
    }
  if (BOT_BUTTONS_SHOULD_BE_VISIBLE (
        self->track->main_height))
    {
      for (int i = 0; i < self->num_bot_buttons;
           i++)
        {
          cb = self->bot_buttons[i];
          RETURN_IF_HOVERED;
        }
    }

  Track * track = self->track;
  if (track->lanes_visible)
    {
      for (int i = 0; i < track->num_lanes; i++)
        {
          TrackLane * lane = track->lanes[i];

          for (int j = 0; j < lane->num_buttons;
               j++)
            {
              cb = lane->buttons[j];
              RETURN_IF_HOVERED;
            }
        }
    }

  AutomationTracklist * atl =
    track_get_automation_tracklist (track);
  if (atl && track->automation_visible)
    {
      for (int i = 0; i < atl->num_ats; i++)
        {
          AutomationTrack * at = atl->ats[i];

          /* skip invisible automation tracks */
          if (!at->visible)
            continue;

          for (int j = 0;
               j < at->num_top_left_buttons;
               j++)
            {
              cb = at->top_left_buttons[j];
              RETURN_IF_HOVERED;
            }
          for (int j = 0;
               j < at->num_top_right_buttons;
               j++)
            {
              cb = at->top_right_buttons[j];
              RETURN_IF_HOVERED;
            }
          if (BOT_BUTTONS_SHOULD_BE_VISIBLE (
                at->height))
            {
              for (int j = 0;
                   j < at->num_bot_left_buttons;
                   j++)
                {
                  cb = at->bot_left_buttons[j];
                  RETURN_IF_HOVERED;
                }
              for (int j = 0;
                   j < at->num_bot_right_buttons;
                   j++)
                {
                  cb = at->bot_right_buttons[j];
                  RETURN_IF_HOVERED;
                }
            }
        }
    }
  return NULL;
}

static AutomationModeWidget *
get_hovered_am_widget (
  TrackWidget * self,
  int           x,
  int           y)
{
#define IS_BUTTON_HOVERED \
  (x >= cb->x && \
   x <= cb->x + \
     (cb->width ? cb->width : BUTTON_SIZE) && \
   y >= cb->y && y <= cb->y + BUTTON_SIZE)
#define RETURN_IF_HOVERED \
  if (IS_BUTTON_HOVERED) return cb;

  Track * track = self->track;
  AutomationTracklist * atl =
    track_get_automation_tracklist (track);
  if (atl && track->automation_visible)
    {
      for (int i = 0; i < atl->num_ats; i++)
        {
          AutomationTrack * at = atl->ats[i];

          /* skip invisible automation tracks */
          if (!at->visible)
            continue;

          AutomationModeWidget * am = at->am_widget;
          if (!am)
            continue;
          if (x >= am->x &&
              x <= am->x + am->width &&
              y >= am->y &&
              y <= am->y + BUTTON_SIZE)
            {
              return am;
            }
        }
    }
  return NULL;
}

/** 250 ms */
/*static const float MAX_TIME = 250000.f;*/

/**
 * @param height Total track widget height.
 */
static void
draw_color_area (
  TrackWidget *  self,
  cairo_t *      cr,
  GdkRectangle * rect,
  int            height)
{
  cairo_surface_t * surface =
    z_cairo_get_surface_from_icon_name (
      self->track->icon_name, 16, 0);

  GdkRGBA bg_color = self->track->color;
  if (self->color_area_hovered)
    {
      color_brighten_default (&bg_color);
    }
  gdk_cairo_set_source_rgba (cr, &bg_color);
  cairo_rectangle (
    cr, 0, 0, COLOR_AREA_WIDTH, height);
  cairo_fill (cr);

  GdkRGBA c2, c3;
  ui_get_contrast_color (
    &self->track->color, &c2);
  ui_get_contrast_color (
    &c2, &c3);

  /* add shadow in the back */
  cairo_set_source_rgba (
    cr, c3.red, c3.green, c3.blue, 0.4);
  cairo_mask_surface (
    cr, surface, 2, 2);
  cairo_fill (cr);

  /* add main icon */
  if (self->icon_hovered)
    {
      color_brighten_default (&c2);
    }
  cairo_set_source_rgba (
    cr, c2.red, c2.green, c2.blue, 1);
  /*cairo_set_source_surface (*/
    /*self->cached_cr, surface, 1, 1);*/
  cairo_mask_surface (
    cr, surface, 1, 1);
  cairo_fill (cr);

#if 0
  /* draw meters */
  cairo_set_source_rgba (cr, 0, 1, 0.2, 1.0);
  int height = rect->height;
  switch (self->track->in_signal_type)
    {
    case TYPE_AUDIO:
      {
      float rms =
        port_get_rms_db (
          self->track->processor.stereo_in->l, 1);
      sample_t amp =
        math_dbfs_to_amp (rms);
      sample_t fader_val =
        math_get_fader_val_from_amp (amp);
      fader_val *= (sample_t) height;
      cairo_rectangle (
        cr, (COLOR_AREA_WIDTH - 4),
        (height - (int) fader_val),
        2, fader_val);
      cairo_fill (cr);

      rms =
        port_get_rms_db (
          self->track->processor.stereo_in->r, 1);
      amp =
        math_dbfs_to_amp (rms);
      fader_val =
        math_get_fader_val_from_amp (amp);
      fader_val *= (sample_t) height;
      cairo_rectangle (
        cr, (COLOR_AREA_WIDTH - 2),
        (height - (int) fader_val),
        2, fader_val);
      cairo_fill (cr);
      }
      break;
    case TYPE_EVENT:
      {
        int has_midi_events = 0;
        MidiEvent event;
        float val = 0.f;
        if (!self->track->processor.midi_out)
          break;
        Port * port =
          self->track->processor.midi_out;
        while (
          zix_ring_read (
            port->midi_ring, &event,
            sizeof (MidiEvent)) > 0)
          {
            has_midi_events = 1;
          }

        if (has_midi_events)
          {
            self->last_midi_in_trigger_time =
              g_get_real_time ();
            val = 1.f;
          }
        else
          {
            gint64 time_diff =
              g_get_real_time () -
              self->last_midi_in_trigger_time;
            if ((float) time_diff < MAX_TIME)
              {
                val =
                  1.f - (float) time_diff / MAX_TIME;
              }
            else
              val = 0.f;
          }
        val *= height;
        cairo_rectangle (
          cr, (COLOR_AREA_WIDTH - 4),
          (height - (int) val),
          4, val);
        cairo_fill (cr);
      }
      break;
    default:
      break;
    }
#endif
}

static void
draw_name (
  TrackWidget * self,
  cairo_t *     cr)
{
  /* draw text */
  cairo_set_source_rgba (
    cr, 1, 1, 1, 1);
  cairo_move_to (cr, 22, 2);
  PangoLayout * layout = self->layout;
  pango_layout_set_text (
    layout, self->track->name, -1);
  pango_cairo_show_layout (cr, layout);
}

/**
 * @param top 1 to draw top, 0 to draw bottom.
 * @param width Track width.
 */
static void
draw_buttons (
  TrackWidget * self,
  cairo_t *     cr,
  int           top,
  int           width)
{
  CustomButtonWidget * hovered_cb =
    get_hovered_button (
      self, (int) self->last_x, (int) self->last_y);
  int num_buttons =
    top? self->num_top_buttons :
    self->num_bot_buttons;
  CustomButtonWidget ** buttons =
    top? self->top_buttons :
    self->bot_buttons;
  Track * track = self->track;
  for (int i = 0; i < num_buttons; i++)
    {
      CustomButtonWidget * cb = buttons[i];

      if (top)
        {
          cb->x =
            width - (BUTTON_SIZE + BUTTON_PADDING) *
            (num_buttons - i);
          cb->y = BUTTON_PADDING_FROM_EDGE;
        }
      else
        {
          cb->x =
            width -
              (BUTTON_SIZE + BUTTON_PADDING) *
              (self->num_bot_buttons - i);
          cb->y =
            track->main_height -
              (BUTTON_PADDING_FROM_EDGE +
               BUTTON_SIZE);
        }

      CustomButtonWidgetState state =
        CUSTOM_BUTTON_WIDGET_STATE_NORMAL;

      bool is_solo = CB_ICON_IS (SOLO);

      if (cb == self->clicked_button)
        {
          /* currently clicked button */
          state =
            CUSTOM_BUTTON_WIDGET_STATE_ACTIVE;
        }
      else if (is_solo &&
               track_get_soloed (track))
        {
          state =
            CUSTOM_BUTTON_WIDGET_STATE_TOGGLED;
        }
      else if (is_solo &&
               track_get_implied_soloed (
                 track))
        {
          state =
            CUSTOM_BUTTON_WIDGET_STATE_SEMI_TOGGLED;
        }
      else if (CB_ICON_IS (SHOW_UI) &&
               instrument_track_is_plugin_visible (
                 track))
        {
          state =
            CUSTOM_BUTTON_WIDGET_STATE_TOGGLED;
        }
      else if (CB_ICON_IS (MUTE) &&
               track_get_muted (
                 track))
        {
          state =
            CUSTOM_BUTTON_WIDGET_STATE_TOGGLED;
        }
      else if (CB_ICON_IS (LISTEN) &&
               track_get_listened (track))
        {
          state =
            CUSTOM_BUTTON_WIDGET_STATE_TOGGLED;
        }
      else if (CB_ICON_IS (FREEZE) &&
                 track->frozen)
        {
          state =
            CUSTOM_BUTTON_WIDGET_STATE_TOGGLED;
        }
      else if (CB_ICON_IS (MONO_COMPAT) &&
               channel_get_mono_compat_enabled (
                 track->channel))
        {
          state =
            CUSTOM_BUTTON_WIDGET_STATE_TOGGLED;
        }
      else if (CB_ICON_IS (RECORD) &&
               track->recording)
        {
          state =
            CUSTOM_BUTTON_WIDGET_STATE_TOGGLED;
        }
      else if (CB_ICON_IS (SHOW_TRACK_LANES) &&
               track->lanes_visible)
        {
          state =
            CUSTOM_BUTTON_WIDGET_STATE_TOGGLED;
        }
      else if (CB_ICON_IS (
                 SHOW_AUTOMATION_LANES) &&
               track->automation_visible)
        {
          state =
            CUSTOM_BUTTON_WIDGET_STATE_TOGGLED;
        }
      else if (hovered_cb == cb)
        {
          state =
            CUSTOM_BUTTON_WIDGET_STATE_HOVERED;
        }

      if (state != cb->last_state)
        {
          /* add another cycle to draw transition of
           * 1 frame */
          self->redraw =
            CUSTOM_BUTTON_WIDGET_MAX_TRANSITION_FRAMES;
          track_widget_force_redraw (self);
        }
      custom_button_widget_draw (
        cb, cr, cb->x, cb->y, state);
    }
}

static void
draw_lanes (
  TrackWidget * self,
  cairo_t *     cr,
  int           width)
{
  Track * track = self->track;
  g_return_if_fail (track);

  if (!track->lanes_visible)
    return;

  int total_height = (int) track->main_height;
  for (int i = 0; i < track->num_lanes; i++)
    {
      TrackLane * lane = track->lanes[i];

      /* remember y */
      lane->y = total_height;

      /* draw separator */
      cairo_set_source_rgba (
        cr, 1, 1, 1, 0.3);
      cairo_set_line_width (cr, 0.5);
      cairo_move_to (
        cr, COLOR_AREA_WIDTH, total_height);
      cairo_line_to (cr, width, total_height);
      cairo_stroke (cr);

      /* draw text */
      cairo_set_source_rgba (
        cr, 1, 1, 1, 1);
      cairo_move_to (
        cr,
        COLOR_AREA_WIDTH +
          BUTTON_PADDING_FROM_EDGE,
        total_height + BUTTON_PADDING_FROM_EDGE);
      PangoLayout * layout = self->layout;
      char str[50];
      sprintf (str, _("Lane %d"), i + 1);
      pango_layout_set_text (
        layout, str, -1);
      pango_cairo_show_layout (cr, layout);

      /* create buttons if necessary */
      CustomButtonWidget * cb;
      if (lane->num_buttons == 0)
        {
          lane->buttons[0] =
            custom_button_widget_new (
              ICON_NAME_SOLO, BUTTON_SIZE);
          cb = lane->buttons[0];
          cb->owner_type =
            CUSTOM_BUTTON_WIDGET_OWNER_LANE;
          cb->owner = lane;
          cb->toggled_color =
            UI_COLORS->solo_checked;
          cb->held_color =
            UI_COLORS->solo_active;
          lane->buttons[1] =
            custom_button_widget_new (
              ICON_NAME_MUTE, BUTTON_SIZE);
          cb = lane->buttons[1];
          cb->owner_type =
            CUSTOM_BUTTON_WIDGET_OWNER_LANE;
          cb->owner = lane;
          lane->num_buttons = 2;
        }

      /* draw buttons */
      CustomButtonWidget * hovered_cb =
        get_hovered_button (
          self, (int) self->last_x,
          (int) self->last_y);
      for (int j = 0; j < lane->num_buttons; j++)
        {
          cb =
            lane->buttons[j];

          cb->x =
            width - (BUTTON_SIZE + BUTTON_PADDING) *
            (lane->num_buttons - j);
          cb->y =
            total_height + BUTTON_PADDING_FROM_EDGE;

          CustomButtonWidgetState state =
            CUSTOM_BUTTON_WIDGET_STATE_NORMAL;

          if (cb == self->clicked_button)
            {
              /* currently clicked button */
              state =
                CUSTOM_BUTTON_WIDGET_STATE_ACTIVE;
            }
          else if (CB_ICON_IS (SOLO) &&
                   lane->solo)
            {
              state =
                CUSTOM_BUTTON_WIDGET_STATE_TOGGLED;
            }
          else if (CB_ICON_IS (MUTE) &&
                   lane->mute)
            {
              state =
                CUSTOM_BUTTON_WIDGET_STATE_TOGGLED;
            }
          else if (hovered_cb == cb)
            {
              state =
                CUSTOM_BUTTON_WIDGET_STATE_HOVERED;
            }

          if (state != cb->last_state)
            {
              /* add another cycle to draw
               * transition */
              self->redraw =
                CUSTOM_BUTTON_WIDGET_MAX_TRANSITION_FRAMES;
              track_widget_force_redraw (self);
            }
          custom_button_widget_draw (
            cb, cr, cb->x, cb->y, state);
        }

      total_height += (int) lane->height;
    }
}

static void
draw_automation (
  TrackWidget * self,
  cairo_t *     cr,
  int           width)
{
  Track * track = self->track;
  g_return_if_fail (track);

  if (!track->automation_visible)
    return;

  AutomationTracklist * atl =
    track_get_automation_tracklist (track);
  g_return_if_fail (atl);
  int total_height = (int) track->main_height;

  if (track->lanes_visible)
    {
      for (int j = 0; j < track->num_lanes; j++)
        {
          TrackLane * lane = track->lanes[j];
          total_height += (int) lane->height;
        }
    }

  for (int i = 0; i < atl->num_ats; i++)
    {
      AutomationTrack * at = atl->ats[i];

      if (!(at->created && at->visible))
        continue;

      /* remember y */
      at->y = total_height;

      /* draw separator above at */
      cairo_set_source_rgba (
        cr, 1, 1, 1, 0.3);
      cairo_set_line_width (cr, 0.5);
      cairo_move_to (
        cr, COLOR_AREA_WIDTH, total_height);
      cairo_line_to (cr, width, total_height);
      cairo_stroke (cr);

      /* create buttons if necessary */
      CustomButtonWidget * cb;
      if (at->num_top_left_buttons == 0)
        {
          at->top_left_buttons[0] =
            custom_button_widget_new (
              ICON_NAME_SHOW_AUTOMATION_LANES,
              BUTTON_SIZE);
          cb = at->top_left_buttons[0];
          cb->owner_type =
            CUSTOM_BUTTON_WIDGET_OWNER_AT;
          cb->owner = at;
          /*char text[500];*/
          /*sprintf (*/
            /*text, "%d - %s",*/
            /*at->index, at->automatable->label);*/
          custom_button_widget_set_text (
            cb, self->layout,
            at->port_id.label,
            AUTOMATABLE_NAME_FONT);
          at->num_top_left_buttons = 1;
        }
      if (at->num_top_right_buttons == 0)
        {
#if 0
          at->top_right_buttons[0] =
            custom_button_widget_new (
              ICON_NAME_MUTE, BUTTON_SIZE);
          at->top_right_buttons[0]->owner_type =
            CUSTOM_BUTTON_WIDGET_OWNER_AT;
          at->top_right_buttons[0]->owner = at;
          at->num_top_right_buttons = 1;
#endif
        }
      if (at->num_bot_left_buttons == 0)
        {
        }
      if (!at->am_widget)
        {
          at->am_widget =
            automation_mode_widget_new (
              BUTTON_SIZE, self->layout, at);
        }
      if (at->num_bot_right_buttons == 0)
        {
          at->bot_right_buttons[0] =
            custom_button_widget_new (
              ICON_NAME_MINUS,
              BUTTON_SIZE);
          at->bot_right_buttons[0]->owner_type =
            CUSTOM_BUTTON_WIDGET_OWNER_AT;
          at->bot_right_buttons[0]->owner = at;
          at->bot_right_buttons[1] =
            custom_button_widget_new (
              ICON_NAME_PLUS,
              BUTTON_SIZE);
          at->bot_right_buttons[1]->owner_type =
            CUSTOM_BUTTON_WIDGET_OWNER_AT;
          at->bot_right_buttons[1]->owner = at;
          at->num_bot_right_buttons = 2;
        }

      /* draw top left buttons */
      CustomButtonWidget * hovered_cb =
        get_hovered_button (
          self, (int) self->last_x,
          (int) self->last_y);
      AutomationModeWidget * hovered_am =
        get_hovered_am_widget (
          self, (int) self->last_x,
          (int) self->last_y);
      for (int j = 0; j < at->num_top_left_buttons;
           j++)
        {
          cb =
            at->top_left_buttons[j];

          cb->x =
            BUTTON_PADDING_FROM_EDGE +
            COLOR_AREA_WIDTH;
          cb->y =
            total_height + BUTTON_PADDING_FROM_EDGE;

          CustomButtonWidgetState state =
            CUSTOM_BUTTON_WIDGET_STATE_NORMAL;

          if (cb == self->clicked_button)
            {
              /* currently clicked button */
              state =
                CUSTOM_BUTTON_WIDGET_STATE_ACTIVE;
            }
          else if (hovered_cb == cb)
            {
              state =
                CUSTOM_BUTTON_WIDGET_STATE_HOVERED;
            }

          if (state != cb->last_state)
            {
              /* add another cycle to draw
               * transition */
              self->redraw =
                CUSTOM_BUTTON_WIDGET_MAX_TRANSITION_FRAMES;
              track_widget_force_redraw (self);
            }
          custom_button_widget_draw_with_text (
            cb, cr, cb->x, cb->y,
            width -
              (COLOR_AREA_WIDTH +
               BUTTON_PADDING_FROM_EDGE * 2 +
               at->num_top_right_buttons *
                 (BUTTON_SIZE + BUTTON_PADDING) +
               AUTOMATION_VALUE_WIDTH +
               BUTTON_PADDING),
            state);
        }

      /* draw automation value */
      PangoLayout * layout = self->layout;
      char str[50];
      Port * port =
        automation_track_get_port (at);
      sprintf (
        str, "%.2f",
        (double)
        control_port_get_val (port));
      cb = at->top_left_buttons[0];
      cairo_move_to (
        cr,
        cb->x + cb->width + BUTTON_PADDING,
        total_height + BUTTON_PADDING_FROM_EDGE);
      pango_layout_set_text (
        layout, str, -1);
      pango_cairo_show_layout (cr, layout);

      /* draw top right buttons */
      for (int j = 0; j < at->num_top_right_buttons;
           j++)
        {
          cb = at->top_right_buttons[j];

          cb->x =
            width - (BUTTON_SIZE + BUTTON_PADDING) *
            (at->num_top_right_buttons - j);
          cb->y =
            total_height + BUTTON_PADDING_FROM_EDGE;

          CustomButtonWidgetState state =
            CUSTOM_BUTTON_WIDGET_STATE_NORMAL;

          if (cb == self->clicked_button)
            {
              /* currently clicked button */
              state =
                CUSTOM_BUTTON_WIDGET_STATE_ACTIVE;
            }
          else if (hovered_cb == cb)
            {
              state =
                CUSTOM_BUTTON_WIDGET_STATE_HOVERED;
            }

          if (state != cb->last_state)
            {
              /* add another cycle to draw
               * transition */
              self->redraw =
                CUSTOM_BUTTON_WIDGET_MAX_TRANSITION_FRAMES;
              track_widget_force_redraw (self);
            }
          custom_button_widget_draw (
            cb, cr, cb->x, cb->y, state);
        }

      if (BOT_BUTTONS_SHOULD_BE_VISIBLE (
            at->height))
        {
          /* automation mode */
          AutomationModeWidget * am = at->am_widget;

          am->x =
            BUTTON_PADDING_FROM_EDGE +
            COLOR_AREA_WIDTH;
          am->y =
            (total_height + at->height) -
              (BUTTON_PADDING_FROM_EDGE +
               BUTTON_SIZE);

          CustomButtonWidgetState state =
            CUSTOM_BUTTON_WIDGET_STATE_NORMAL;
          if (self->clicked_am == am)
            {
              /* currently clicked button */
              state =
                CUSTOM_BUTTON_WIDGET_STATE_ACTIVE;
            }
          else if (hovered_am == am)
            {
              state =
                CUSTOM_BUTTON_WIDGET_STATE_HOVERED;
            }

          if (state != cb->last_state)
            {
              /* add another cycle to draw
               * transition */
              self->redraw =
                CUSTOM_BUTTON_WIDGET_MAX_TRANSITION_FRAMES;
              track_widget_force_redraw (self);
            }
          automation_mode_widget_draw (
            am, cr, am->x, am->y,
            self->last_x, state);

          for (int j = 0;
               j < at->num_bot_right_buttons;
               j++)
            {
              cb =
                at->bot_right_buttons[j];

              cb->x =
                width -
                  (BUTTON_SIZE + BUTTON_PADDING) *
                  (at->num_bot_right_buttons - j);
              cb->y =
                (total_height + at->height) -
                  (BUTTON_PADDING_FROM_EDGE +
                   BUTTON_SIZE);

              state =
                CUSTOM_BUTTON_WIDGET_STATE_NORMAL;

              if (cb == self->clicked_button)
                {
                  /* currently clicked button */
                  state =
                    CUSTOM_BUTTON_WIDGET_STATE_ACTIVE;
                }
              else if (hovered_cb == cb)
                {
                  state =
                    CUSTOM_BUTTON_WIDGET_STATE_HOVERED;
                }

              if (state != cb->last_state)
                {
                  /* add another cycle to draw
                   * transition */
                  self->redraw =
                    CUSTOM_BUTTON_WIDGET_MAX_TRANSITION_FRAMES;
                  track_widget_force_redraw (self);
                }
              custom_button_widget_draw (
                cb, cr, cb->x, cb->y, state);
            }
        }
      total_height += (int) at->height;
    }
}

static int
track_draw_cb (
  GtkWidget *   widget,
  cairo_t *     cr,
  TrackWidget * self)
{
  GdkRectangle rect;
  gdk_cairo_get_clip_rectangle (cr, &rect);

  Track * track = self->track;

  /*g_message (*/
    /*"track %s rect x %d %d redraw %d",*/
    /*self->track->name, rect.x, rect.y,*/
    /*self->redraw);*/
  /*g_message (*/
    /*"redrawing track (frames left: %d)",*/
    /*self->redraw);*/
  if (self->redraw)
    {
      GtkStyleContext *context =
        gtk_widget_get_style_context (widget);

      int width =
        gtk_widget_get_allocated_width (widget);
      int height =
        gtk_widget_get_allocated_height (widget);

      z_cairo_reset_caches (
        &self->cached_cr,
        &self->cached_surface, width,
        height, cr);

      gtk_render_background (
        context, self->cached_cr, 0, 0,
        width, height);

      if (self->bg_hovered)
        {
          cairo_set_source_rgba (
            self->cached_cr, 1, 1, 1, 0.1);
          cairo_rectangle (
            self->cached_cr, 0, 0, width, height);
          cairo_fill (self->cached_cr);
        }
      else if (track_is_selected (track))
        {
          cairo_set_source_rgba (
            self->cached_cr, 1, 1, 1, 0.07);
          cairo_rectangle (
            self->cached_cr, 0, 0, width, height);
          cairo_fill (self->cached_cr);
        }

      draw_color_area (
        self, self->cached_cr, &rect, height);

      draw_name (self, self->cached_cr);

      draw_buttons (
        self, self->cached_cr, 1, width);

      /* only show bot buttons if enough space */
      if (BOT_BUTTONS_SHOULD_BE_VISIBLE (
            track->main_height))
        {
          draw_buttons (
            self, self->cached_cr, 0, width);
        }

      draw_lanes (self, self->cached_cr, width);

      draw_automation (
        self, self->cached_cr, width);

      self->redraw--;

      /* if there are still frames left, draw
       * again to finish drawing the sequence */
      if (self->redraw)
        gtk_widget_queue_draw_area (
          widget, rect.x, rect.y, rect.width,
          rect.height);
    }

  cairo_set_source_surface (
    cr, self->cached_surface, 0, 0);
  cairo_paint (cr);

  return FALSE;
}

static AutomationTrack *
get_at_to_resize (
  TrackWidget * self,
  int           y)
{
  Track * track = self->track;

  if (!track->automation_visible)
    return NULL;

  AutomationTracklist * atl =
    track_get_automation_tracklist (track);
  g_return_val_if_fail (atl, NULL);
  int total_height = (int) track->main_height;
  if (track->lanes_visible)
    {
      for (int i = 0; i < track->num_lanes; i++)
        {
          TrackLane * lane = track->lanes[i];
          total_height += (int) lane->height;
        }
    }

  for (int i = 0; i < atl->num_ats; i++)
    {
      AutomationTrack * at = atl->ats[i];

      if (at->created && at->visible)
        total_height += (int) at->height;

      int val = total_height - y;
      if (val >= 0 && val < RESIZE_PX)
        {
          return at;
        }
    }

  return NULL;
}

static TrackLane *
get_lane_to_resize (
  TrackWidget * self,
  int           y)
{
  Track * track = self->track;

  if (!track->lanes_visible)
    return NULL;

  int total_height = (int) track->main_height;
  for (int i = 0; i < track->num_lanes; i++)
    {
      TrackLane * lane = track->lanes[i];
      total_height += (int) lane->height;

      int val = total_height - y;
      if (val >= 0 && val < RESIZE_PX)
        {
          return lane;
        }
    }

  return NULL;
}

static void
set_tooltip_from_button (
  TrackWidget *        self,
  CustomButtonWidget * cb)
{
#define SET_TOOLTIP(txt) \
  gtk_widget_set_has_tooltip ( \
    GTK_WIDGET (self), true); \
  self->tooltip_text = txt

  if (!cb)
    {
      gtk_widget_set_has_tooltip (
        GTK_WIDGET (self), false);
    }
  else if (CB_ICON_IS (SOLO))
    {
      SET_TOOLTIP (_("Solo"));
    }
  else if (CB_ICON_IS (SHOW_UI))
    {
      if (instrument_track_is_plugin_visible (
            self->track))
        {
          SET_TOOLTIP (_("Hide instrument UI"));
        }
      else
        {
          SET_TOOLTIP (_("Show instrument UI"));
        }
    }
  else if (CB_ICON_IS (MUTE))
    {
      if (track_get_muted (self->track))
        {
          SET_TOOLTIP (_("Unmute"));
        }
      else
        {
          SET_TOOLTIP (_("Mute"));
        }
    }
  else if (CB_ICON_IS (LISTEN))
    {
      if (track_get_muted (self->track))
        {
          SET_TOOLTIP (_("Unlisten"));
        }
      else
        {
          SET_TOOLTIP (_("Listen"));
        }
    }
  else if (CB_ICON_IS (MONO_COMPAT))
    {
      SET_TOOLTIP (_("Mono compatibility"));
    }
  else if (CB_ICON_IS (RECORD))
    {
      if (self->track->recording)
        {
          SET_TOOLTIP (_("Disarm"));
        }
      else
        {
          SET_TOOLTIP (_("Arm for recording"));
        }
    }
  else if (CB_ICON_IS (SHOW_TRACK_LANES))
    {
      if (self->track->lanes_visible)
        {
          SET_TOOLTIP (_("Hide lanes"));
        }
      else
        {
          SET_TOOLTIP (_("Show lanes"));
        }
    }
  else if (CB_ICON_IS (SHOW_AUTOMATION_LANES))
    {
      if (self->track->automation_visible)
        {
          SET_TOOLTIP (_("Hide automation"));
        }
      else
        {
          SET_TOOLTIP (_("Show automation"));
        }
    }
  else if (CB_ICON_IS (PLUS))
    {
      SET_TOOLTIP (_("Add"));
    }
  else if (CB_ICON_IS (MINUS))
    {
      SET_TOOLTIP (_("Remove"));
    }
  else if (CB_ICON_IS (FREEZE))
    {
      SET_TOOLTIP (_("Freeze/unfreeze"));
    }
  else if (CB_ICON_IS (LOCK))
    {
      SET_TOOLTIP (_("Lock/unlock"));
    }
  else
    {
      /* tooltip missing */
      g_warn_if_reached ();
    }

#undef SET_TOOLTIP
}

static gboolean
on_motion (
  GtkWidget *      widget,
  GdkEventMotion * event,
  TrackWidget *    self)
{
  /*int height =*/
    /*gtk_widget_get_allocated_height (widget);*/

  /* show resize cursor or not */
  if (self->bg_hovered)
    {
      self->color_area_hovered =
        event->x < COLOR_AREA_WIDTH;
      CustomButtonWidget * cb =
        get_hovered_button (
          self, (int) event->x, (int) event->y);
      AutomationModeWidget * am =
        get_hovered_am_widget (
          self, (int) event->x, (int) event->y);
      int val =
        (int) self->track->main_height -
        (int) event->y;
      int resizing_track =
        val >= 0 && val < RESIZE_PX;
      AutomationTrack * resizing_at =
        get_at_to_resize (self, (int) event->y);
      TrackLane * resizing_lane =
        get_lane_to_resize (self, (int) event->y);
      if (self->resizing)
        {
          self->resize = 1;
        }
      else if (!cb && !am && resizing_track)
        {
          self->resize = 1;
          self->resize_target_type =
            TRACK_WIDGET_RESIZE_TARGET_TRACK;
          self->resize_target = self->track;
          /*g_message ("RESIZING TRACK");*/
        }
      else if (!cb && !am && resizing_at)
        {
          self->resize = 1;
          self->resize_target_type =
            TRACK_WIDGET_RESIZE_TARGET_AT;
          self->resize_target = resizing_at;
          /*g_message (*/
            /*"RESIZING AT %s",*/
            /*resizing_at->automatable->label);*/
        }
      else if (!cb && !am && resizing_lane)
        {
          self->resize = 1;
          self->resize_target_type =
            TRACK_WIDGET_RESIZE_TARGET_LANE;
          self->resize_target = resizing_lane;
          /*g_message (*/
            /*"RESIZING LANE %d",*/
            /*resizing_lane->pos);*/
        }
      else
        {
          self->resize = 0;
        }

      if (self->resize == 1)
        {
          ui_set_cursor_from_name (
            widget, "s-resize");
        }
      else
        {
          ui_set_pointer_cursor (widget);
        }
    }

  if (event->type == GDK_ENTER_NOTIFY)
    {
      self->bg_hovered = true;
      self->color_area_hovered = false;
      self->resize = 0;
    }
  else if (event->type == GDK_LEAVE_NOTIFY)
    {
      ui_set_pointer_cursor (widget);
      if (!self->resizing)
        {
          self->bg_hovered = false;
          self->color_area_hovered = false;
          self->resize = 0;
          self->button_pressed = 0;
        }
      self->icon_hovered = false;
    }
  else
    {
      if (event->type == GDK_MOTION_NOTIFY)
        {
          GdkEventMotion * motion_ev =
            (GdkEventMotion *) event;

          /* set tooltips */
          CustomButtonWidget * hovered_btn =
            get_hovered_button (
              self,
              (int) motion_ev->x,
              (int) motion_ev->y);
          set_tooltip_from_button (
            self, hovered_btn);

          /* set whether icon is covered */
          if (motion_ev->x >= 1 &&
              motion_ev->y >= 1 &&
              motion_ev->x < 18 &&
              motion_ev->y < 18)
            {
              self->icon_hovered = true;
            }
          else
            {
              self->icon_hovered = false;
            }
        }

      self->bg_hovered = true;
    }
  track_widget_force_redraw (self);

  self->last_x = event->x;
  self->last_y = event->y;

  return FALSE;
}

static bool
on_query_tooltip (
  GtkWidget *   widget,
  gint          x,
  gint          y,
  gboolean      keyboard_mode,
  GtkTooltip *  tooltip,
  TrackWidget * self)
{
  /* TODO set tooltip rect */
  gtk_tooltip_set_text (
    tooltip, self->tooltip_text);

  return true;
}

/**
 * Wrapper.
 */
void
track_widget_force_redraw (
  TrackWidget * self)
{
  g_return_if_fail (self);
  self->redraw =
    MIN (self->redraw + 1, 10);
  gtk_widget_queue_draw (
    (GtkWidget *) self->drawing_area);
}

static void
get_visible_rect (
  TrackWidget *  self,
  GdkRectangle * rect)
{
  rect->x = 0;
  rect->y = 0;
  rect->height =
    gtk_widget_get_allocated_height (
      GTK_WIDGET (self));
  rect->width =
    gtk_widget_get_allocated_width (
      GTK_WIDGET (self));
}

/**
 * Causes a redraw of the meters only.
 */
void
track_widget_redraw_meters (
  TrackWidget * self)
{
  GdkRectangle rect;
  get_visible_rect (self, &rect);

#if 0
  int draw_count = 0;
  switch (self->track->in_signal_type)
    {
    case TYPE_AUDIO:
      draw_count++;
      if (self->redraw < draw_count)
        self->redraw = draw_count;
      gtk_widget_queue_draw_area (
        GTK_WIDGET (self->drawing_area),
        COLOR_AREA_WIDTH - 4,
        0, 4, self->track->main_height);
      break;
    case TYPE_EVENT:
      draw_count++;
      if (self->redraw < draw_count)
        self->redraw = draw_count;
      gtk_widget_queue_draw_area (
        GTK_WIDGET (self->drawing_area),
        COLOR_AREA_WIDTH - 4,
        0, 4, self->track->main_height);
      break;
    default:
      break;
    }

  switch (self->track->out_signal_type)
    {
    case TYPE_AUDIO:
      break;
    case TYPE_EVENT:
      break;
    default:
      break;
    }
#endif

  if (gtk_widget_get_visible (
        GTK_WIDGET (self->meter_l)))
    {
      gtk_gl_area_queue_render (
        GTK_GL_AREA (self->meter_l));
    }
  if (gtk_widget_get_visible (
        GTK_WIDGET (self->meter_r)))
    {
      gtk_gl_area_queue_render (
        GTK_GL_AREA (self->meter_r));
    }
}

/**
 * Returns if cursor is in top half of the track.
 *
 * Used by timeline to determine if it will select
 * objects or range.
 */
int
track_widget_is_cursor_in_top_half (
  TrackWidget * self,
  double        y)
{
  gint wx, wy;
  gtk_widget_translate_coordinates (
    GTK_WIDGET (MW_TIMELINE),
    GTK_WIDGET (self),
    0,
    (int) y,
    &wx,
    &wy);

  /* if bot half */
  if (wy >= self->track->main_height / 2 &&
      wy <= self->track->main_height)
    {
      return 0;
    }
  else /* if top half */
    {
      return 1;
    }
}

static TrackLane *
get_lane_at_y (
  TrackWidget * self,
  double        y)
{
  Track * track = self->track;

  if (!track->lanes_visible)
    return NULL;

  TrackLane * lane = NULL;
  for (int i = 0; i < track->num_lanes; i++)
    {
      lane = track->lanes[i];

      g_message ("checking %d", i);
      if (lane->widget &&
          ui_is_child_hit (
            GTK_WIDGET (self),
            GTK_WIDGET (lane->widget),
            0, 1, 0, y, 0, 0))
        {
          return lane;
        }
    }

  return NULL;
}

/**
 * Info to pass when selecting a MIDI channel from
 * the context menu.
 */
typedef struct MidiChSelectionInfo
{
  /** Either one of these should be set. */
  Track *     track;
  TrackLane * lane;

  /** MIDI channel (1-16), or 0 for lane to
   * inherit. */
  midi_byte_t ch;
} MidiChSelectionInfo;

static void
on_midi_ch_selected (
  GtkMenuItem *         menu_item,
  MidiChSelectionInfo * info)
{
  if (info->lane)
    {
      info->lane->midi_ch = info->ch;
    }
  if (info->track)
    {
      info->track->midi_ch = info->ch;
    }
  free (info);
}

static void
on_passthrough_input_toggled (
  GtkCheckMenuItem * menu_item,
  Track *            track)
{
  track->passthrough_midi_input =
    gtk_check_menu_item_get_active (menu_item);
}

static void
on_quick_bounce_clicked (
  GtkMenuItem * menuitem,
  Track *       track)
{
  ExportSettings settings;
  settings.mode = EXPORT_MODE_TRACKS;
  export_settings_set_bounce_defaults (
    &settings, NULL, track->name);
  tracklist_selections_mark_for_bounce (
    TRACKLIST_SELECTIONS,
    settings.bounce_with_parents,
    F_NO_MARK_MASTER);

  /* start exporting in a new thread */
  GThread * thread =
    g_thread_new (
      "bounce_thread",
      (GThreadFunc) exporter_generic_export_thread,
      &settings);

  /* create a progress dialog and block */
  ExportProgressDialogWidget * progress_dialog =
    export_progress_dialog_widget_new (
      &settings, true, false, F_CANCELABLE);
  gtk_window_set_transient_for (
    GTK_WINDOW (progress_dialog),
    GTK_WINDOW (MAIN_WINDOW));
  gtk_dialog_run (GTK_DIALOG (progress_dialog));
  gtk_widget_destroy (
    GTK_WIDGET (progress_dialog));

  g_thread_join (thread);

  if (!settings.has_error && !settings.cancelled)
    {
      /* create audio track with bounced material */
      Marker * m =
        marker_track_get_start_marker (
          P_MARKER_TRACK);
      ArrangerObject * m_obj = (ArrangerObject *) m;
      exporter_create_audio_track_after_bounce (
        &settings, &m_obj->pos);
    }

  export_settings_free_members (&settings);
}

static void
on_bounce_clicked (
  GtkMenuItem * menuitem,
  Track *       track)
{
  BounceDialogWidget * dialog =
    bounce_dialog_widget_new (
      BOUNCE_DIALOG_TRACKS, track->name);
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
show_context_menu (
  TrackWidget * self,
  double        y)
{
  GtkWidget *menu;
  GtkMenuItem *menuitem;
  menu = gtk_menu_new();
  Track * track = self->track;
  TrackLane * lane =
    get_lane_at_y (self, y);

#define APPEND(mi) \
  gtk_menu_shell_append ( \
    GTK_MENU_SHELL (menu), \
    GTK_WIDGET (menuitem));

#define ADD_SEPARATOR \
  menuitem = \
    GTK_MENU_ITEM ( \
      gtk_separator_menu_item_new ()); \
  gtk_widget_set_visible ( \
    GTK_WIDGET (menuitem), true); \
  APPEND (menuitem)

  int num_selected =
    TRACKLIST_SELECTIONS->num_tracks;

  if (num_selected > 0)
    {
      char * str;

      if (track->type != TRACK_TYPE_MASTER &&
          track->type != TRACK_TYPE_CHORD &&
          track->type != TRACK_TYPE_MARKER &&
          track->type != TRACK_TYPE_TEMPO)
        {
          /* delete track */
          if (num_selected == 1)
            str =
              g_strdup (_("_Delete Track"));
          else
            str =
              g_strdup (_("_Delete Tracks"));
          menuitem =
            z_gtk_create_menu_item (
              str, "edit-delete", F_NO_TOGGLE,
              "win.delete-selected-tracks");
          g_free (str);
          APPEND (menuitem);

          /* duplicate track */
          if (num_selected == 1)
            str =
              g_strdup (_("_Duplicate Track"));
          else
            str =
              g_strdup (_("_Duplicate Tracks"));
          menuitem =
            z_gtk_create_menu_item (
              str, "edit-copy", F_NO_TOGGLE,
              "win.duplicate-selected-tracks");
          g_free (str);
          APPEND (menuitem);
        }

      /* add regions */
      if (track->type == TRACK_TYPE_INSTRUMENT)
        {
          menuitem =
            z_gtk_create_menu_item (
              _("Add Region"), "list-add",
              F_NO_TOGGLE, "win.add-region");
          APPEND (menuitem);
        }

      menuitem =
        z_gtk_create_menu_item (
          num_selected == 1 ?
            _("Hide Track") :
            _("Hide Tracks"),
          "view-hidden", F_NO_TOGGLE,
          "win.hide-selected-tracks");
      APPEND (menuitem);

      menuitem =
        z_gtk_create_menu_item (
          num_selected == 1 ?
            _("Pin/Unpin Track") :
            _("Pin/Unpin Tracks"),
          "window-pin", F_NO_TOGGLE,
          "win.pin-selected-tracks");
      APPEND (menuitem);
    }

  if (track->out_signal_type == TYPE_AUDIO)
    {
      ADD_SEPARATOR;
      menuitem =
        z_gtk_create_menu_item (
          _("Quick bounce"), "file-music-line",
          F_NO_TOGGLE, NULL);
      APPEND (menuitem);
      g_signal_connect (
        menuitem, "activate",
        G_CALLBACK (on_quick_bounce_clicked),
        track);

      menuitem =
        z_gtk_create_menu_item (
          _("Bounce..."), "document-export",
          F_NO_TOGGLE, NULL);
      APPEND (menuitem);
      g_signal_connect (
        menuitem, "activate",
        G_CALLBACK (on_bounce_clicked),
        track);
    }

  /* add solo/mute/listen */
  if (track_type_has_channel (track->type))
    {
      ADD_SEPARATOR;

      if (tracklist_selections_contains_soloed_track (
            TRACKLIST_SELECTIONS, F_NO_SOLO))
        {
          menuitem =
            z_gtk_create_menu_item (
              _("Solo"), ICON_NAME_SOLO, F_NO_TOGGLE,
              "win.solo-selected-tracks");
          APPEND (menuitem);
        }
      if (tracklist_selections_contains_soloed_track (
            TRACKLIST_SELECTIONS, F_SOLO))
        {
          menuitem =
            z_gtk_create_menu_item (
              _("Unsolo"), "unsolo", F_NO_TOGGLE,
              "win.unsolo-selected-tracks");
          APPEND (menuitem);
        }

      if (tracklist_selections_contains_muted_track (
            TRACKLIST_SELECTIONS, F_NO_MUTE))
        {
          menuitem =
            z_gtk_create_menu_item (
              _("Mute"), ICON_NAME_MUTE, F_NO_TOGGLE,
              "win.mute-selected-tracks");
          APPEND (menuitem);
        }
      if (tracklist_selections_contains_muted_track (
            TRACKLIST_SELECTIONS, F_MUTE))
        {
          menuitem =
            z_gtk_create_menu_item (
              _("Unmute"), "unmute", F_NO_TOGGLE,
              "win.unmute-selected-tracks");
          APPEND (menuitem);
        }

      if (tracklist_selections_contains_listened_track (
            TRACKLIST_SELECTIONS, F_NO_LISTEN))
        {
          menuitem =
            z_gtk_create_menu_item (
              _("Listen"), ICON_NAME_LISTEN,
              F_NO_TOGGLE,
              "win.listen-selected-tracks");
          APPEND (menuitem);
        }
      if (tracklist_selections_contains_listened_track (
            TRACKLIST_SELECTIONS, F_LISTEN))
        {
          menuitem =
            z_gtk_create_menu_item (
              _("Unlisten"), "unlisten",
              F_NO_TOGGLE,
              "win.unlisten-selected-tracks");
          APPEND (menuitem);
        }
    }

  ADD_SEPARATOR;
  menuitem =
    z_gtk_create_menu_item (
      _("Change color..."), "color-fill",
      F_NO_TOGGLE, "win.change-track-color");
  APPEND (menuitem);

  /* add midi channel selectors */
  if (track_type_has_piano_roll (track->type))
    {
      ADD_SEPARATOR;
      menuitem =
        z_gtk_create_menu_item (
          _("Track MIDI Ch"), "signal-midi",
          F_NO_TOGGLE, NULL);

      GtkMenu * submenu =
        GTK_MENU (gtk_menu_new ());
      gtk_widget_set_visible (
        GTK_WIDGET (submenu), 1);
      GtkMenuItem * submenu_item;

      /* add passthrough checkbox */
      submenu_item =
        GTK_MENU_ITEM (
          gtk_check_menu_item_new_with_label (
            _("Passthrough input")));
      gtk_check_menu_item_set_active (
        GTK_CHECK_MENU_ITEM (submenu_item),
        track->passthrough_midi_input);
      g_signal_connect (
        G_OBJECT (submenu_item), "toggled",
        G_CALLBACK (on_passthrough_input_toggled),
        track);
      gtk_menu_shell_append (
        GTK_MENU_SHELL (submenu),
        GTK_WIDGET (submenu_item));
      gtk_widget_set_visible (
        GTK_WIDGET (submenu_item), 1);

      /* add separator */
      submenu_item =
        GTK_MENU_ITEM (
          gtk_separator_menu_item_new ());
      gtk_menu_shell_append (
        GTK_MENU_SHELL (submenu),
        GTK_WIDGET (submenu_item));
      gtk_widget_set_visible (
        GTK_WIDGET (submenu_item), 1);

      /* add each MIDI ch */
      for (int i = 1; i <= 16; i++)
        {
          char * lbl =
            g_strdup_printf (
              _("MIDI Channel %d"), i);
          submenu_item =
            GTK_MENU_ITEM (
              gtk_check_menu_item_new_with_label (lbl));
          gtk_check_menu_item_set_draw_as_radio (
            GTK_CHECK_MENU_ITEM (submenu_item), 1);
          g_free (lbl);

          if (i == track->midi_ch)
            gtk_check_menu_item_set_active (
              GTK_CHECK_MENU_ITEM (submenu_item), 1);

          MidiChSelectionInfo * info =
            calloc (
              1, sizeof (MidiChSelectionInfo));
          info->track = track;
          info->ch = (midi_byte_t) i;
          g_signal_connect (
            G_OBJECT (submenu_item), "activate",
            G_CALLBACK (on_midi_ch_selected),
            info);

          gtk_menu_shell_append (
            GTK_MENU_SHELL (submenu),
            GTK_WIDGET (submenu_item));
          gtk_widget_set_visible (
            GTK_WIDGET (submenu_item), 1);
        }

      gtk_menu_item_set_submenu (
        menuitem, GTK_WIDGET (submenu));
      gtk_widget_set_visible (
        GTK_WIDGET (menuitem), 1);

      APPEND (menuitem);

      if (lane)
        {
          char * lbl =
            g_strdup_printf (
              _("Lane %d MIDI Ch"),
              lane->pos);
          menuitem =
            GTK_MENU_ITEM (
              gtk_menu_item_new_with_label (
                lbl));
          g_free (lbl);

          submenu =
            GTK_MENU (gtk_menu_new ());
          gtk_widget_set_visible (
            GTK_WIDGET (submenu), 1);
          for (int i = 0; i <= 16; i++)
            {
              if (i == 0)
                lbl =
                  g_strdup (_("Inherit"));
              else
                lbl =
                  g_strdup_printf (
                    _("MIDI Channel %d"), i);
              submenu_item =
                GTK_MENU_ITEM (
                  gtk_check_menu_item_new_with_label (
                    lbl));
              g_free (lbl);

              if (lane->midi_ch == i)
                gtk_check_menu_item_set_active (
                  GTK_CHECK_MENU_ITEM (
                    submenu_item), 1);

              MidiChSelectionInfo * info =
                calloc (
                  1, sizeof (MidiChSelectionInfo));
              info->lane = lane;
              info->ch = (midi_byte_t) i;
              g_signal_connect (
                G_OBJECT (submenu_item), "activate",
                G_CALLBACK (on_midi_ch_selected),
                info);

              gtk_menu_shell_append (
                GTK_MENU_SHELL (submenu),
                GTK_WIDGET (submenu_item));
              gtk_widget_set_visible (
                GTK_WIDGET (submenu_item), 1);
            }

          gtk_menu_item_set_submenu (
            menuitem, GTK_WIDGET (submenu));
          gtk_widget_set_visible (
            GTK_WIDGET (menuitem), 1);

          APPEND (menuitem);
        }
    }


#undef APPEND
#undef ADD_SEPARATOR

  gtk_menu_attach_to_widget (
    GTK_MENU (menu),
    GTK_WIDGET (self), NULL);
  gtk_menu_popup_at_pointer (GTK_MENU (menu), NULL);
}

static void
on_right_click (
  GtkGestureMultiPress *gesture,
  gint                  n_press,
  gdouble               x,
  gdouble               y,
  TrackWidget *         self)
{

  GdkModifierType state_mask =
    ui_get_state_mask (GTK_GESTURE (gesture));

  Track * track = self->track;
  if (!track_is_selected (track))
    {
      if (state_mask & GDK_SHIFT_MASK ||
          state_mask & GDK_CONTROL_MASK)
        {
          track_select (
            track, F_SELECT, 0, 1);
        }
      else
        {
          track_select (
            track, F_SELECT, 1, 1);
        }
    }
  if (n_press == 1)
    {
      show_context_menu (self, y);
    }
}

/**
 * Shows a popover for editing the track name.
 */
static void
show_edit_name_popover (
  TrackWidget * self)
{
  /*GtkWidget * popover =*/
    /*gtk_popover_new (GTK_WIDGET (self));*/

  /*gtk_container_add (*/

  editable_label_widget_show_popover_for_widget (
    GTK_WIDGET (self), self->track,
    (GenericStringGetter) track_get_name,
    (GenericStringSetter)
    track_set_name_with_action);
}

static void
multipress_pressed (
  GtkGestureMultiPress *gesture,
  gint                  n_press,
  gdouble               x,
  gdouble               y,
  TrackWidget *         self)
{
  self->was_armed = self->track->recording;

  /* FIXME should do this via focus on click
   * property */
  /*if (!gtk_widget_has_focus (GTK_WIDGET (self)))*/
    /*gtk_widget_grab_focus (GTK_WIDGET (self));*/

  GdkModifierType state_mask =
    ui_get_state_mask (GTK_GESTURE (gesture));

  CustomButtonWidget * cb =
    get_hovered_button (self, (int) x, (int) y);
  AutomationModeWidget * am =
    get_hovered_am_widget (self, (int) x, (int) y);
  if (cb || am)
    {
      self->button_pressed = 1;
      self->clicked_button = cb;
      self->clicked_am = am;
    }
  else
    {
      Track * track = self->track;

      PROJECT->last_selection =
        SELECTION_TYPE_TRACKLIST;
      EVENTS_PUSH (
        ET_PROJECT_SELECTION_TYPE_CHANGED, NULL);

      bool ctrl = state_mask & GDK_CONTROL_MASK;
      bool shift = state_mask & GDK_SHIFT_MASK;
      tracklist_selections_handle_click (
        track, ctrl, shift, false);
    }

  if (self->icon_hovered)
    {
      TrackIconChooserDialogWidget * icon_chooser =
        track_icon_chooser_dialog_widget_new (
          self->track);
      track_icon_chooser_dialog_widget_run (
        icon_chooser);
    }
  else if (self->color_area_hovered)
    {
      ObjectColorChooserDialogWidget * color_chooser =
        object_color_chooser_dialog_widget_new_for_track (
          self->track);
      object_color_chooser_dialog_widget_run (
        color_chooser);
    }

  track_widget_force_redraw (self);
}

static void
multipress_released (
  GtkGestureMultiPress *gesture,
  gint                  n_press,
  gdouble               x,
  gdouble               y,
  TrackWidget *         self)
{
  if (self->clicked_button)
    {
      CustomButtonWidget * cb =
        self->clicked_button;
      Track * track = self->track;

      /* if track not selected, select it */
      if (!track_is_selected (track))
        {
          track_select (
            track, F_SELECT, F_EXCLUSIVE,
            F_PUBLISH_EVENTS);
        }

      if ((Track *) cb->owner == track)
        {
          if (CB_ICON_IS (MONO_COMPAT))
            {
              channel_set_mono_compat_enabled (
                track->channel,
                !channel_get_mono_compat_enabled (
                  track->channel),
                F_PUBLISH_EVENTS);
            }
          else if (CB_ICON_IS (RECORD))
            {
              track_widget_on_record_toggled (self);
            }
          else if (CB_ICON_IS (SOLO))
            {
              track_set_soloed (
                track,
                !track_get_soloed (track),
                F_TRIGGER_UNDO, F_AUTO_SELECT,
                F_PUBLISH_EVENTS);
            }
          else if (CB_ICON_IS (MUTE))
            {
              track_set_muted (
                track,
                !track_get_muted (track),
                F_TRIGGER_UNDO, F_AUTO_SELECT,
                F_PUBLISH_EVENTS);
            }
          else if (CB_ICON_IS (LISTEN))
            {
              track_set_listened (
                track,
                !track_get_listened (track),
                F_TRIGGER_UNDO, F_AUTO_SELECT,
                F_PUBLISH_EVENTS);
            }
          else if (CB_ICON_IS (SHOW_TRACK_LANES))
            {
              track_widget_on_show_lanes_toggled (
                self);
            }
          else if (CB_ICON_IS (SHOW_UI))
            {
              instrument_track_toggle_plugin_visible (
                track);
            }
          else if (CB_ICON_IS (
                     SHOW_AUTOMATION_LANES))
            {
              track_widget_on_show_automation_toggled (
                self);
            }
          else if (CB_ICON_IS (FREEZE))
            {
#if 0
              track_freeze (
                track, !track->frozen);
#endif
            }
        }
      else if (cb->owner_type ==
                 CUSTOM_BUTTON_WIDGET_OWNER_LANE)
        {
          /*TrackLane * lane =*/
            /*(TrackLane *) cb->owner;*/

          /* TODO */
        }
      else if (cb->owner_type ==
                 CUSTOM_BUTTON_WIDGET_OWNER_AT)
        {
          AutomationTrack * at =
            (AutomationTrack *) cb->owner;
          g_return_if_fail (at);
          AutomationTracklist * atl =
            automation_track_get_automation_tracklist (
              at);
          g_return_if_fail (atl);

          if (CB_ICON_IS (PLUS))
            {
              AutomationTrack * new_at =
                automation_tracklist_get_first_invisible_at (
                  atl);

              /* if any invisible at exists, show
               * it */
              if (new_at)
                {
                  if (!new_at->created)
                    new_at->created = 1;
                  new_at->visible = 1;

                  /* move it after the clicked
                   * automation track */
                  automation_tracklist_set_at_index (
                    atl, new_at,
                    at->index + 1, true);

                  EVENTS_PUSH (
                    ET_AUTOMATION_TRACK_ADDED,
                    new_at);
                }
            }
          else if (CB_ICON_IS (MINUS))
            {
              /* don't allow deleting if no other
               * visible automation tracks */
              int num_visible;
              automation_tracklist_get_visible_tracks (
                atl, NULL, &num_visible);
              if (num_visible > 1)
                {
                  at->visible = 0;
                  EVENTS_PUSH (
                    ET_AUTOMATION_TRACK_REMOVED,
                    at);
                }
            }
          else if (CB_ICON_IS (
                     SHOW_AUTOMATION_LANES))
            {
              AutomatableSelectorPopoverWidget * popover =
                automatable_selector_popover_widget_new (at);
              gtk_popover_set_relative_to (
                GTK_POPOVER (popover),
                GTK_WIDGET (self));
              GdkRectangle rect = {
                .x =
                  (int) cb->x +
                  (int) cb->width / 2,
                .y = (int) cb->y,
                .height = cb->size,
                .width = 0,
              };
              gtk_popover_set_pointing_to (
                GTK_POPOVER (popover), &rect);
              gtk_popover_set_modal (
                GTK_POPOVER (popover), 1);
              gtk_widget_show_all (
                GTK_WIDGET (popover));
            }
        }
    }
  else if (self->clicked_am)
    {
      AutomationModeWidget * am =
        self->clicked_am;
      AutomationTrack * at =
        (AutomationTrack *) am->owner;
      g_return_if_fail (at);
      AutomationTracklist * atl =
        automation_track_get_automation_tracklist (
          at);
      g_return_if_fail (atl);
      if (at->automation_mode ==
            AUTOMATION_MODE_RECORD &&
          am->hit_mode ==
            AUTOMATION_MODE_RECORD)
        {
          automation_track_swap_record_mode (at);
          automation_mode_widget_init (am);
        }
      at->automation_mode = am->hit_mode;
      EVENTS_PUSH (
        ET_AUTOMATION_TRACK_CHANGED, at);
    }
  else if (n_press == 2)
    {
      /* show popup to edit the name */
      show_edit_name_popover (self);
    }

  self->button_pressed = 0;
  self->clicked_button = NULL;
  self->clicked_am = NULL;
  track_widget_force_redraw (self);
}

static void
on_drag_begin (GtkGestureDrag *gesture,
               gdouble         start_x,
               gdouble         start_y,
               TrackWidget * self)
{
  self->selected_in_dnd = 0;
  self->dragged = 0;

  if (self->resize)
    {
      /* start resizing */
      self->resizing = 1;
    }
  else if (self->button_pressed)
    {
      gtk_event_controller_reset (
        GTK_EVENT_CONTROLLER (gesture));
      /* if one of the buttons is pressed, ignore */
    }
  else
    {
      Track * track = self->track;
      self->selected_in_dnd = 1;
      MW_MIXER->start_drag_track = track;

      if (self->n_press == 1)
        {
          int ctrl = 0, selected = 0;

          ctrl = self->ctrl_held_at_start;

          if (tracklist_selections_contains_track (
                TRACKLIST_SELECTIONS, track))
            {
              selected = 1;
            }

          /* no control & not selected */
          if (!ctrl && !selected)
            {
              tracklist_selections_select_single (
                TRACKLIST_SELECTIONS,
                track, F_PUBLISH_EVENTS);
            }
          else if (!ctrl && selected)
            { }
          else if (ctrl && !selected)
            tracklist_selections_add_track (
              TRACKLIST_SELECTIONS, track, 1);
        }
    }

  self->start_x = start_x;
  self->start_y = start_y;
}

static void
on_drag_update (
  GtkGestureDrag * gesture,
  gdouble         offset_x,
  gdouble         offset_y,
  TrackWidget * self)
{
  g_message ("drag_update");

  self->dragged = 1;

  if (self->resizing)
    {
      /* resize */
      Track * track = self->track;
      int diff =
        (int) (offset_y - self->last_offset_y);

      switch (self->resize_target_type)
        {
        case TRACK_WIDGET_RESIZE_TARGET_TRACK:
          track->main_height =
            MAX (
              TRACK_MIN_HEIGHT,
              track->main_height + diff);
          break;
        case TRACK_WIDGET_RESIZE_TARGET_AT:
          {
            AutomationTrack * at =
              (AutomationTrack *)
              self->resize_target;
            at->height =
              MAX (
                TRACK_MIN_HEIGHT,
                at->height + diff);
          }
          break;
        case TRACK_WIDGET_RESIZE_TARGET_LANE:
          {
            TrackLane * lane =
              (TrackLane *)
              self->resize_target;
            lane->height =
              MAX (
                TRACK_MIN_HEIGHT,
                lane->height + diff);
            g_message ("lane %d height changed",
                       lane->pos);
          }
          break;
        }
      /* FIXME should be event */
      track_widget_update_size (self);
    }
  else
    {
      /* start dnd */
      char * entry_track =
        g_strdup (TARGET_ENTRY_TRACK);
      GtkTargetEntry entries[] = {
        {
          entry_track, GTK_TARGET_SAME_APP,
          symap_map (ZSYMAP, TARGET_ENTRY_TRACK),
        },
      };
      GtkTargetList * target_list =
        gtk_target_list_new (
          entries, G_N_ELEMENTS (entries));

      gtk_drag_begin_with_coordinates (
        (GtkWidget *) self, target_list,
        GDK_ACTION_MOVE | GDK_ACTION_COPY,
        (int) gtk_gesture_single_get_button (
          GTK_GESTURE_SINGLE (gesture)),
        NULL,
        (int) (self->start_x + offset_x),
        (int) (self->start_y + offset_y));

      g_free (entry_track);
    }

  self->last_offset_y = offset_y;
}

static void
on_drag_end (
  GtkGestureDrag * gesture,
  gdouble          offset_x,
  gdouble          offset_y,
  TrackWidget *    self)
{
  self->resizing = 0;
  self->last_offset_y = 0;
}

static void
on_dnd_drag_begin (
  GtkWidget *      widget,
  GdkDragContext * context,
  TrackWidget *    self)
{
  gtk_drag_set_icon_name (
    context, "track-inspector", 0, 0);
}

static void
on_dnd_drag_data_get (
  GtkWidget        * widget,
  GdkDragContext   * context,
  GtkSelectionData * data,
  guint              info,
  guint              time,
  TrackWidget *      self)
{
  /* Not really needed since the selections are
   * used. just send master */
  gtk_selection_data_set (
    data,
    gdk_atom_intern_static_string (
      TARGET_ENTRY_TRACK),
    32,
    (const guchar *) &P_MASTER_TRACK,
    sizeof (P_MASTER_TRACK));
}

/**
 * Highlights/unhighlights the Tracks
 * appropriately.
 *
 * @param highlight 1 to highlight top or bottom,
 *   0 to unhighlight all.
 */
void
track_widget_do_highlight (
  TrackWidget * self,
  gint          x,
  gint          y,
  const int     highlight)
{
  g_message ("DOING H IGHLIGHT ON %s",
    self->track->name);
  if (highlight)
    {
      /* if we are closer to the start of selection
       * than the end */
      int h =
        gtk_widget_get_allocated_height (
          GTK_WIDGET (self));
      if (y < h / 2)
        {
          /* highlight top */
          gtk_drag_highlight (
            GTK_WIDGET (
              self->highlight_top_box));
          gtk_widget_set_size_request (
            GTK_WIDGET (
              self->highlight_top_box),
            -1, 2);

          /* unhilight bot */
          gtk_drag_unhighlight (
            GTK_WIDGET (
              self->highlight_bot_box));
          gtk_widget_set_size_request (
            GTK_WIDGET (
              self->highlight_bot_box),
            -1, -1);
        }
      else
        {
          /* highlight bot */
          gtk_drag_highlight (
            GTK_WIDGET (
              self->highlight_bot_box));
          gtk_widget_set_size_request (
            GTK_WIDGET (
              self->highlight_bot_box),
            -1, 2);

          /* unhilight top */
          gtk_drag_unhighlight (
            GTK_WIDGET (
              self->highlight_top_box));
          gtk_widget_set_size_request (
            GTK_WIDGET (
              self->highlight_top_box),
            -1, -1);
        }
    }
  else
    {
      gtk_drag_unhighlight (
        GTK_WIDGET (
          self->highlight_top_box));
      gtk_widget_set_size_request (
        GTK_WIDGET (self->highlight_top_box),
        -1, -1);
      gtk_drag_unhighlight (
        GTK_WIDGET (
          self->highlight_bot_box));
      gtk_widget_set_size_request (
        GTK_WIDGET (self->highlight_bot_box),
        -1, -1);
    }
}

/**
 * Converts Y from the arranger coordinates to
 * the track coordinates.
 */
int
track_widget_get_local_y (
  TrackWidget *    self,
  ArrangerWidget * arranger,
  int              arranger_y)
{
  gint y_local;
  gtk_widget_translate_coordinates (
    GTK_WIDGET (arranger),
    GTK_WIDGET (self),
    0, (int) arranger_y, NULL, &y_local);

  return y_local;
}

/**
 * Callback when lanes button is toggled.
 */
void
track_widget_on_show_lanes_toggled (
  TrackWidget * self)
{
  Track * track = self->track;

  /* set visibility flag */
  track_set_lanes_visible (
    track, !track->lanes_visible);
}

void
track_widget_on_show_automation_toggled (
  TrackWidget * self)
{
  Track * track = self->track;

  g_message (
    "%s: toggled on %s", __func__, track->name);

  /* set visibility flag */
  track_set_automation_visible (
    track, !track->automation_visible);
}

void
track_widget_on_record_toggled (
  TrackWidget * self)
{
  Track * track = self->track;
  g_return_if_fail (track);
  Channel * chan = track->channel;
  g_return_if_fail (chan);

  /* toggle record flag */
  track_set_recording (
    track, !self->was_armed, F_PUBLISH_EVENTS);
  chan->record_set_automatically = false;
  g_debug (
    "%s recording: %d",
    track->name,  track->recording);

  EVENTS_PUSH (
    ET_TRACK_STATE_CHANGED, track);
}

static void
recreate_pango_layouts (
  TrackWidget * self,
  GdkRectangle * allocation)
{
  if (PANGO_IS_LAYOUT (self->layout))
    g_object_unref (self->layout);

  GtkWidget * widget =
    GTK_WIDGET (self->drawing_area);

  PangoFontDescription *desc;
  self->layout =
    gtk_widget_create_pango_layout (
      widget, NULL);
  desc =
    pango_font_description_from_string (
      NAME_FONT);
  pango_layout_set_font_description (
    self->layout, desc);
  pango_font_description_free (desc);
  pango_layout_set_ellipsize (
    self->layout, PANGO_ELLIPSIZE_END);
  if (allocation)
    {
      pango_layout_set_width (
        self->layout,
        pango_units_from_double (
          allocation->width - 20));
    }
}

static void
track_widget_on_size_allocate (
  GtkWidget *    widget,
  GdkRectangle * allocation,
  TrackWidget * self)
{
  recreate_pango_layouts (self, allocation);
  track_widget_force_redraw (self);
}

static void
on_screen_changed (
  GtkWidget *    widget,
  GdkScreen *    previous_screen,
  TrackWidget * self)
{
  recreate_pango_layouts (self, NULL);
}

/**
 * Add a button.
 *
 * @param top 1 for top, 0 for bottom.
 */
static CustomButtonWidget *
add_button (
  TrackWidget * self,
  bool          top,
  const char *  icon_name)
{
  CustomButtonWidget * cb =
    custom_button_widget_new (
      icon_name, BUTTON_SIZE);
  if (top)
    {
      self->top_buttons[
        self->num_top_buttons++] = cb;
    }
  else
    {
      self->bot_buttons[
        self->num_bot_buttons++] = cb;
    }

  cb->owner_type = CUSTOM_BUTTON_WIDGET_OWNER_TRACK;
  cb->owner = self->track;

  return cb;
}

static CustomButtonWidget *
add_solo_button (
  TrackWidget * self,
  int           top)
{
  CustomButtonWidget * cb =
    add_button (self, top, ICON_NAME_SOLO);
  cb->toggled_color =
    UI_COLORS->solo_checked;
  cb->held_color =
    UI_COLORS->solo_active;

  return cb;
}

static CustomButtonWidget *
add_record_button (
  TrackWidget * self,
  int           top)
{
  CustomButtonWidget * cb =
    add_button (self, top, ICON_NAME_RECORD);
  gdk_rgba_parse (
    &cb->toggled_color,
    UI_COLOR_RECORD_CHECKED);
  gdk_rgba_parse (
    &cb->held_color, UI_COLOR_RECORD_ACTIVE);

  return cb;
}

/**
 * Updates the full track size and redraws the
 * track.
 */
void
track_widget_update_size (
  TrackWidget * self)
{
  g_return_if_fail (self->track);
  int height =
    (int)
    track_get_full_visible_height (self->track);
  int w;
  gtk_widget_get_size_request (
    (GtkWidget *) self, &w, NULL);
  g_return_if_fail (height > 1);
  gtk_widget_set_size_request (
    GTK_WIDGET (self), w, height);
  track_widget_force_redraw (self);
}

static gboolean
track_tick_cb (
  GtkWidget *     widget,
  GdkFrameClock * frame_clock,
  TrackWidget *   self)
{
  if (!gtk_widget_get_mapped (
        GTK_WIDGET (self->drawing_area)))
    {
      return G_SOURCE_CONTINUE;
    }

  track_widget_redraw_meters (self);
  return G_SOURCE_CONTINUE;
}

/**
 * Wrapper for child track widget.
 *
 * Sets color, draw callback, etc.
 */
TrackWidget *
track_widget_new (Track * track)
{
  g_return_val_if_fail (track, NULL);

  TrackWidget * self =
    g_object_new (
      TRACK_WIDGET_TYPE, NULL);

  self->track = track;

  switch (track->type)
    {
    case TRACK_TYPE_INSTRUMENT:
      add_record_button (self, 1);
      add_solo_button (self, 1);
      add_button (
        self, true, ICON_NAME_MUTE);
      add_button (
        self, true, ICON_NAME_LISTEN);
      add_button (
        self, true, ICON_NAME_SHOW_UI);
      add_button (
        self, false, ICON_NAME_LOCK);
      add_button (
        self, false, ICON_NAME_FREEZE);
      add_button (
        self, false, ICON_NAME_SHOW_TRACK_LANES);
      add_button (
        self, false,
        ICON_NAME_SHOW_AUTOMATION_LANES);
      break;
    case TRACK_TYPE_MIDI:
      add_record_button (self, 1);
      add_solo_button (self, 1);
      add_button (
        self, 1, ICON_NAME_MUTE);
      add_button (
        self, 0, ICON_NAME_LOCK);
      add_button (
        self, 0, ICON_NAME_SHOW_TRACK_LANES);
      add_button (
        self, 0, ICON_NAME_SHOW_AUTOMATION_LANES);
      break;
    case TRACK_TYPE_MASTER:
      add_button (
        self, 1, ICON_NAME_MONO_COMPAT);
      add_solo_button (self, 1);
      add_button (
        self, 1, ICON_NAME_MUTE);
      add_button (
        self, 0, ICON_NAME_SHOW_AUTOMATION_LANES);
      break;
    case TRACK_TYPE_CHORD:
      add_record_button (self, 1);
      add_solo_button (self, 1);
      add_button (
        self, 1, ICON_NAME_MUTE);
      break;
    case TRACK_TYPE_MARKER:
      break;
    case TRACK_TYPE_TEMPO:
      add_button (
        self, true,
        ICON_NAME_SHOW_AUTOMATION_LANES);
      break;
    case TRACK_TYPE_MODULATOR:
      add_button (
        self, true,
        ICON_NAME_SHOW_AUTOMATION_LANES);
      break;
    case TRACK_TYPE_AUDIO_BUS:
      add_solo_button (self, 1);
      add_button (
        self, 1, ICON_NAME_MUTE);
      add_button (
        self, true, ICON_NAME_LISTEN);
      add_button (
        self, 0, ICON_NAME_SHOW_AUTOMATION_LANES);
      break;
    case TRACK_TYPE_MIDI_BUS:
      add_solo_button (self, 1);
      add_button (
        self, 1, ICON_NAME_MUTE);
      add_button (
        self, 0, ICON_NAME_SHOW_AUTOMATION_LANES);
      break;
    case TRACK_TYPE_AUDIO_GROUP:
      add_button (
        self, 1, ICON_NAME_MONO_COMPAT);
      add_solo_button (self, 1);
      add_button (
        self, 1, ICON_NAME_MUTE);
      add_button (
        self, true, ICON_NAME_LISTEN);
      add_button (
        self, 0, ICON_NAME_SHOW_AUTOMATION_LANES);
      break;
    case TRACK_TYPE_MIDI_GROUP:
      add_solo_button (self, 1);
      add_button (
        self, 1, ICON_NAME_MUTE);
      add_button (
        self, 0, ICON_NAME_SHOW_AUTOMATION_LANES);
      break;
    case TRACK_TYPE_AUDIO:
      add_record_button (self, 1);
      add_solo_button (self, 1);
      add_button (
        self, 1, ICON_NAME_MUTE);
      add_button (
        self, true, ICON_NAME_LISTEN);
      add_button (
        self, 0, ICON_NAME_LOCK);
      add_button (
        self, 0, ICON_NAME_SHOW_TRACK_LANES);
      add_button (
        self, 0, ICON_NAME_SHOW_AUTOMATION_LANES);
      break;
    }

  if (track_type_has_channel (self->track->type))
    {
      switch (self->track->out_signal_type)
        {
        case TYPE_EVENT:
          meter_widget_setup (
            self->meter_l,
            self->track->channel->midi_out, 8);
          gtk_widget_set_margin_start (
            GTK_WIDGET (self->meter_l), 2);
          gtk_widget_set_margin_end (
            GTK_WIDGET (self->meter_l), 2);
          self->meter_l->padding = 0;
          gtk_widget_set_visible (
            GTK_WIDGET (self->meter_r), 0);
          break;
        case TYPE_AUDIO:
          meter_widget_setup (
            self->meter_l,
            self->track->channel->stereo_out->l, 6);
          self->meter_l->padding = 0;
          meter_widget_setup (
            self->meter_r,
            self->track->channel->stereo_out->r, 6);
          self->meter_r->padding = 0;
          break;
        default:
          break;
        }
    }
  else
    {
      gtk_widget_set_visible (
        GTK_WIDGET (self->meter_l), 0);
      gtk_widget_set_visible (
        GTK_WIDGET (self->meter_r), 0);
    }

  track_widget_update_size (self);

  gtk_widget_add_tick_callback (
    GTK_WIDGET (self),
    (GtkTickCallback) track_tick_cb,
    self, NULL);

  return self;
}

static void
on_destroy (
  TrackWidget * self)
{

  Track * track = self->track;

  CustomButtonWidget * cb = NULL;
  for (int i = 0; i < self->num_top_buttons; i++)
    {
      cb = self->top_buttons[i];
      custom_button_widget_free (cb);
    }
  for (int i = 0; i < self->num_bot_buttons; i++)
    {
      cb = self->bot_buttons[i];
      custom_button_widget_free (cb);
    }

  g_object_unref (self);

  track->widget = NULL;
}

static void
track_widget_init (TrackWidget * self)
{
  g_type_ensure (METER_WIDGET_TYPE);

  gtk_widget_init_template (GTK_WIDGET (self));

  gtk_widget_set_vexpand_set (
    (GtkWidget *) self, 1);

  /* set font sizes */
  /*gtk_label_set_max_width_chars (*/
    /*self->top_grid->name->label, 14);*/
  /*gtk_label_set_width_chars (*/
    /*self->top_grid->name->label, 14);*/
  /*gtk_label_set_ellipsize (*/
    /*self->top_grid->name->label, PANGO_ELLIPSIZE_END);*/
  /*gtk_label_set_xalign (*/
    /*self->top_grid->name->label, 0);*/

  self->drag =
    GTK_GESTURE_DRAG (
      gtk_gesture_drag_new (
        GTK_WIDGET (self->drawing_area)));

  self->multipress =
    GTK_GESTURE_MULTI_PRESS (
      gtk_gesture_multi_press_new (
        GTK_WIDGET (self->drawing_area)));
  self->right_mouse_mp =
    GTK_GESTURE_MULTI_PRESS (
      gtk_gesture_multi_press_new (
        GTK_WIDGET (self->drawing_area)));
  gtk_gesture_single_set_button (
    GTK_GESTURE_SINGLE (self->right_mouse_mp),
    GDK_BUTTON_SECONDARY);

  /* make widget able to notify */
  gtk_widget_add_events (
    GTK_WIDGET (self->drawing_area),
    GDK_ALL_EVENTS_MASK);

  g_signal_connect (
    G_OBJECT (self->multipress), "pressed",
    G_CALLBACK (multipress_pressed), self);
  g_signal_connect (
    G_OBJECT (self->multipress), "released",
    G_CALLBACK (multipress_released), self);
  g_signal_connect (
    G_OBJECT (self->right_mouse_mp), "pressed",
    G_CALLBACK (on_right_click), self);
  g_signal_connect (
    G_OBJECT (self->drawing_area),
    "enter-notify-event",
    G_CALLBACK (on_motion),  self);
  g_signal_connect (
    G_OBJECT (self->drawing_area),
    "leave-notify-event",
    G_CALLBACK (on_motion),  self);
  g_signal_connect (
    G_OBJECT (self->drawing_area),
    "motion-notify-event",
    G_CALLBACK (on_motion),  self);
  g_signal_connect (
    G_OBJECT (self->drawing_area), "draw",
    G_CALLBACK (track_draw_cb),  self);
  g_signal_connect (
    G_OBJECT (self->drag), "drag-begin",
    G_CALLBACK (on_drag_begin), self);
  g_signal_connect (
    G_OBJECT (self->drag), "drag-update",
    G_CALLBACK (on_drag_update), self);
  g_signal_connect (
    G_OBJECT (self->drag), "drag-end",
    G_CALLBACK (on_drag_end), self);
  g_signal_connect (
    GTK_WIDGET (self), "drag-begin",
    G_CALLBACK (on_dnd_drag_begin), self);
  g_signal_connect (
    GTK_WIDGET (self), "drag-data-get",
    G_CALLBACK (on_dnd_drag_data_get), self);
  g_signal_connect (
    G_OBJECT(self), "destroy",
    G_CALLBACK (on_destroy),  NULL);
  g_signal_connect (
    G_OBJECT (self->drawing_area), "screen-changed",
    G_CALLBACK (on_screen_changed),  self);
  g_signal_connect (
    G_OBJECT (self->drawing_area), "size-allocate",
    G_CALLBACK (track_widget_on_size_allocate),
    self);
  g_signal_connect (
    G_OBJECT (self), "query-tooltip",
    G_CALLBACK (on_query_tooltip),  self);

  g_object_ref (self);

  self->redraw = 1;
}

static void
track_widget_class_init (TrackWidgetClass * _klass)
{
  GtkWidgetClass * klass =
    GTK_WIDGET_CLASS (_klass);
  resources_set_class_template (
    klass, "track.ui");

#define BIND_CHILD(x) \
  gtk_widget_class_bind_template_child ( \
    klass, TrackWidget, x)

  BIND_CHILD (drawing_area);
  BIND_CHILD (main_box);
  BIND_CHILD (meter_l);
  BIND_CHILD (meter_r);
  BIND_CHILD (highlight_top_box);
  BIND_CHILD (highlight_bot_box);

#undef BIND_CHILD
}
