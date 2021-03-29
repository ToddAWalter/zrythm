/*
 * Copyright (C) 2019-2021 Alexandros Theodotou <alex at zrythm dot org>
 *
 * This file is part of Zrythm
 *
 * Zrythm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version approved by
 * Alexandros Theodotou in a public statement of acceptance.
 *
 * Zrythm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Zrythm.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdlib.h>

#include "audio/automation_track.h"
#include "audio/port.h"
#include "audio/tempo_track.h"
#include "audio/track.h"
#include "gui/backend/event.h"
#include "gui/backend/event_manager.h"
#include "project.h"
#include "utils/arrays.h"
#include "utils/flags.h"
#include "utils/math.h"
#include "utils/objects.h"
#include "zrythm_app.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

/**
 * Inits the tempo track.
 */
void
tempo_track_init (
  Track * self)
{
  self->type = TRACK_TYPE_TEMPO;
  self->main_height = TRACK_DEF_HEIGHT / 2;

  gdk_rgba_parse (&self->color, "#2f6c52");
  self->icon_name =
    g_strdup ("filename-bpm-amarok");

  /* create bpm port */
  self->bpm_port =
    port_new_with_type (
      TYPE_CONTROL, FLOW_INPUT, _("BPM"));
  self->bpm_port->minf = 60.f;
  self->bpm_port->maxf = 360.f;
  self->bpm_port->deff = 140.f;
  port_set_control_value (
    self->bpm_port, self->bpm_port->deff, false,
    false);
  port_set_owner_track (self->bpm_port, self);
  self->bpm_port->id.flags |= PORT_FLAG_BPM;
  self->bpm_port->id.flags |=
    PORT_FLAG_AUTOMATABLE;

  /* create time sig ports */
  self->beats_per_bar_port =
    port_new_with_type (
      TYPE_CONTROL, FLOW_INPUT, _("Beats per bar"));
  self->beats_per_bar_port->minf =
    TEMPO_TRACK_MIN_BEATS_PER_BAR;
  self->beats_per_bar_port->maxf =
    TEMPO_TRACK_MAX_BEATS_PER_BAR;
  self->beats_per_bar_port->deff =
    TEMPO_TRACK_MIN_BEATS_PER_BAR;
  port_set_control_value (
    self->beats_per_bar_port,
    TEMPO_TRACK_DEFAULT_BEATS_PER_BAR, false, false);
  port_set_owner_track (
    self->beats_per_bar_port, self);
  self->beats_per_bar_port->id.flags2 |=
    PORT_FLAG2_BEATS_PER_BAR;
  self->beats_per_bar_port->id.flags |=
    PORT_FLAG_AUTOMATABLE;
  self->beats_per_bar_port->id.flags |=
    PORT_FLAG_INTEGER;

  self->beat_unit_port =
    port_new_with_type (
      TYPE_CONTROL, FLOW_INPUT, _("Beat unit"));
  self->beat_unit_port->minf =
    TEMPO_TRACK_MIN_BEAT_UNIT;
  self->beat_unit_port->maxf =
    TEMPO_TRACK_MAX_BEAT_UNIT;
  self->beat_unit_port->deff =
    TEMPO_TRACK_MIN_BEAT_UNIT;
  port_set_control_value (
    self->beat_unit_port,
    TEMPO_TRACK_DEFAULT_BEAT_UNIT, false, false);
  port_set_owner_track (
    self->beat_unit_port, self);
  self->beat_unit_port->id.flags2 |=
    PORT_FLAG2_BEAT_UNIT;
  self->beat_unit_port->id.flags |=
    PORT_FLAG_AUTOMATABLE;
  self->beat_unit_port->id.flags |=
    PORT_FLAG_INTEGER;

  /* set invisible */
  self->visible = false;
}

/**
 * Creates the default tempo track.
 */
Track *
tempo_track_default (
  int   track_pos)
{
  Track * self =
    track_new (
      TRACK_TYPE_TEMPO, track_pos, _("Tempo"),
      F_WITHOUT_LANE);

  return self;
}

/**
 * Returns the BPM at the given pos.
 */
bpm_t
tempo_track_get_bpm_at_pos (
  Track *    self,
  Position * pos)
{
  AutomationTrack * at =
    automation_track_find_from_port_id (
      &self->bpm_port->id, false);
  return
    automation_track_get_val_at_pos (
      at, pos, false, false);
}

/**
 * Returns the current BPM.
 */
bpm_t
tempo_track_get_current_bpm (
  Track * self)
{
  return
    port_get_control_value (
      self->bpm_port, false);
}

/**
 * Sets the BPM.
 *
 * @param update_snap_points Whether to update the
 *   snap points.
 * @param stretch_audio_region Whether to stretch
 *   audio regions. This should only be true when
 *   the BPM change is final.
 * @param start_bpm The BPM at the start of the
 *   action, if not temporary.
 */
void
tempo_track_set_bpm (
  Track * self,
  bpm_t   bpm,
  bpm_t   start_bpm,
  bool    temporary,
  bool    fire_events)
{
  if (AUDIO_ENGINE->transport_type ==
        AUDIO_ENGINE_NO_JACK_TRANSPORT)
    {
      g_debug (
        "%s: bpm <%f>, temporary <%d>",
        __func__, (double) bpm, temporary);
    }

  if (bpm < TEMPO_TRACK_MIN_BPM)
    {
      bpm = TEMPO_TRACK_MIN_BPM;
    }
  else if (bpm > TEMPO_TRACK_MAX_BPM)
    {
      bpm = TEMPO_TRACK_MAX_BPM;
    }

  port_set_control_value (
    self->bpm_port, bpm, false, false);

  if (!temporary)
    {
      UndoableAction * action =
        transport_action_new_bpm_change (
          start_bpm,
          port_get_control_value (
            self->bpm_port, false),
          false);
      undo_manager_perform (
        UNDO_MANAGER, action);
    }

  if (fire_events)
    {
      EVENTS_PUSH (ET_BPM_CHANGED, NULL);
    }
}

int
tempo_track_beat_unit_enum_to_int (
  BeatUnit ebeat_unit)
{
  int bu = 0;
  switch (ebeat_unit)
    {
    case BEAT_UNIT_2:
      bu = 2;
      break;
    case BEAT_UNIT_4:
      bu = 4;
      break;
    case BEAT_UNIT_8:
      bu = 8;
      break;
    case BEAT_UNIT_16:
      bu = 16;
      break;
    default:
      g_warn_if_reached ();
      break;
    }

  return bu;
}

void
tempo_track_set_beat_unit_from_enum (
  Track *  self,
  BeatUnit ebeat_unit)
{
  port_set_control_value (
    self->beat_unit_port, ebeat_unit,
    F_NOT_NORMALIZED, F_PUBLISH_EVENTS);
  EVENTS_PUSH (
    ET_TIME_SIGNATURE_CHANGED, NULL);
}

BeatUnit
tempo_track_beat_unit_to_enum (
  int beat_unit)
{
  switch (beat_unit)
    {
    case 2:
      return BEAT_UNIT_2;
    case 4:
      return BEAT_UNIT_4;
    case 8:
      return BEAT_UNIT_8;
    case 16:
      return BEAT_UNIT_16;;
    default:
      break;
    }
  g_return_val_if_reached (0);
}

BeatUnit
tempo_track_get_beat_unit_enum (
  Track * self)
{
  return
    tempo_track_beat_unit_to_enum (
      tempo_track_get_beat_unit (self));
}

void
tempo_track_set_beat_unit (
  Track * self,
  int     beat_unit)
{
  BeatUnit ebu =
    tempo_track_beat_unit_to_enum (beat_unit);
  tempo_track_set_beat_unit_from_enum (
    self, ebu);
}

/**
 * Updates beat unit and anything depending on it.
 */
void
tempo_track_set_beats_per_bar (
  Track *     self,
  int         beats_per_bar)
{
  port_set_control_value (
    self->beats_per_bar_port, beats_per_bar,
    F_NOT_NORMALIZED, F_PUBLISH_EVENTS);
  EVENTS_PUSH (
    ET_TIME_SIGNATURE_CHANGED, NULL);
}

int
tempo_track_get_beats_per_bar (
  Track * self)
{
  return
    math_round_float_to_int (
      self->beats_per_bar_port->control);
}

int
tempo_track_get_beat_unit (
  Track * self)
{
  BeatUnit ebu =
    math_round_float_to_int (
      self->beat_unit_port->control);
  return tempo_track_beat_unit_enum_to_int (ebu);
}

/**
 * Removes all objects from the tempo track.
 *
 * Mainly used in testing.
 */
void
tempo_track_clear (
  Track * self)
{
  /* TODO */
  /*g_warn_if_reached ();*/
}
