// SPDX-FileCopyrightText: © 2019-2022 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include <cstdlib>

#include "dsp/automation_track.h"
#include "dsp/port.h"
#include "dsp/router.h"
#include "dsp/tempo_track.h"
#include "dsp/track.h"
#include "gui/backend/event.h"
#include "gui/backend/event_manager.h"
#include "project.h"
#include "utils/arrays.h"
#include "utils/error.h"
#include "utils/flags.h"
#include "utils/math.h"
#include "utils/objects.h"
#include "utils/ui.h"
#include "zrythm_app.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

/**
 * Inits the tempo track.
 */
void
tempo_track_init (Track * self)
{
  self->type = TrackType::TRACK_TYPE_TEMPO;
  self->main_height = TRACK_DEF_HEIGHT / 2;

  gdk_rgba_parse (&self->color, "#2f6c52");
  self->icon_name = g_strdup ("filename-bpm-amarok");

  /* create bpm port */
  self->bpm_port = port_new_with_type_and_owner (
    ZPortType::Z_PORT_TYPE_CONTROL, ZPortFlow::Z_PORT_FLOW_INPUT, _ ("BPM"),
    ZPortOwnerType::Z_PORT_OWNER_TYPE_TRACK, self);
  self->bpm_port->id.sym = g_strdup ("bpm");
  self->bpm_port->minf = 60.f;
  self->bpm_port->maxf = 360.f;
  self->bpm_port->deff = 140.f;
  port_set_control_value (self->bpm_port, self->bpm_port->deff, false, false);
  self->bpm_port->id.flags |= ZPortFlags::Z_PORT_FLAG_BPM;
  self->bpm_port->id.flags |= ZPortFlags::Z_PORT_FLAG_AUTOMATABLE;

  /* create time sig ports */
  self->beats_per_bar_port = port_new_with_type_and_owner (
    ZPortType::Z_PORT_TYPE_CONTROL, ZPortFlow::Z_PORT_FLOW_INPUT,
    _ ("Beats per bar"), ZPortOwnerType::Z_PORT_OWNER_TYPE_TRACK, self);
  self->beats_per_bar_port->id.sym = g_strdup ("beats_per_bar");
  self->beats_per_bar_port->minf = TEMPO_TRACK_MIN_BEATS_PER_BAR;
  self->beats_per_bar_port->maxf = TEMPO_TRACK_MAX_BEATS_PER_BAR;
  self->beats_per_bar_port->deff = TEMPO_TRACK_MIN_BEATS_PER_BAR;
  port_set_control_value (
    self->beats_per_bar_port, TEMPO_TRACK_DEFAULT_BEATS_PER_BAR, false, false);
  self->beats_per_bar_port->id.flags2 |= ZPortFlags2::Z_PORT_FLAG2_BEATS_PER_BAR;
  self->beats_per_bar_port->id.flags |= ZPortFlags::Z_PORT_FLAG_AUTOMATABLE;
  self->beats_per_bar_port->id.flags |= ZPortFlags::Z_PORT_FLAG_INTEGER;

  self->beat_unit_port = port_new_with_type_and_owner (
    ZPortType::Z_PORT_TYPE_CONTROL, ZPortFlow::Z_PORT_FLOW_INPUT,
    _ ("Beat unit"), ZPortOwnerType::Z_PORT_OWNER_TYPE_TRACK, self);
  self->beat_unit_port->id.sym = g_strdup ("beat_unit");
  self->beat_unit_port->minf = static_cast<float> (TEMPO_TRACK_MIN_BEAT_UNIT);
  self->beat_unit_port->maxf = static_cast<float> (TEMPO_TRACK_MAX_BEAT_UNIT);
  self->beat_unit_port->deff = static_cast<float> (TEMPO_TRACK_MIN_BEAT_UNIT);
  port_set_control_value (
    self->beat_unit_port, static_cast<float> (TEMPO_TRACK_DEFAULT_BEAT_UNIT),
    false, false);
  self->beat_unit_port->id.flags2 |= ZPortFlags2::Z_PORT_FLAG2_BEAT_UNIT;
  self->beat_unit_port->id.flags |= ZPortFlags::Z_PORT_FLAG_AUTOMATABLE;
  self->beat_unit_port->id.flags |= ZPortFlags::Z_PORT_FLAG_INTEGER;

  /* set invisible */
  self->visible = false;
}

/**
 * Creates the default tempo track.
 */
Track *
tempo_track_default (int track_pos)
{
  Track * self = track_new (
    TrackType::TRACK_TYPE_TEMPO, track_pos, _ ("Tempo"), F_WITHOUT_LANE);

  return self;
}

/**
 * Returns the BPM at the given pos.
 */
bpm_t
tempo_track_get_bpm_at_pos (Track * self, Position * pos)
{
  AutomationTrack * at =
    automation_track_find_from_port_id (&self->bpm_port->id, false);
  return automation_track_get_val_at_pos (
    at, pos, false, false, Z_F_NO_USE_SNAPSHOTS);
}

/**
 * Returns the current BPM.
 */
bpm_t
tempo_track_get_current_bpm (Track * self)
{
  return port_get_control_value (self->bpm_port, false);
}

const char *
tempo_track_get_current_bpm_as_str (void * self)
{
  Track * t = (Track *) self;
  g_return_val_if_fail (IS_TRACK_AND_NONNULL (t), NULL);

  static char * bpm_str = NULL;
  if (bpm_str)
    g_free (bpm_str);

  bpm_t bpm = tempo_track_get_current_bpm (t);
  bpm_str = g_strdup_printf ("%.2f", bpm);

  return bpm_str;
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
  if (
    AUDIO_ENGINE->transport_type
    == AudioEngineJackTransportType::AUDIO_ENGINE_NO_JACK_TRANSPORT)
    {
      g_debug (
        "%s: bpm <%f>, temporary <%d>", __func__, (double) bpm, temporary);
    }

  if (bpm < TEMPO_TRACK_MIN_BPM)
    {
      bpm = TEMPO_TRACK_MIN_BPM;
    }
  else if (bpm > TEMPO_TRACK_MAX_BPM)
    {
      bpm = TEMPO_TRACK_MAX_BPM;
    }

  if (temporary)
    {
      port_set_control_value (self->bpm_port, bpm, false, false);
    }
  else
    {
      GError * err = NULL;
      bool     ret =
        transport_action_perform_bpm_change (start_bpm, bpm, false, &err);
      if (!ret)
        {
          HANDLE_ERROR (err, "%s", _ ("Failed to change BPM"));
        }
    }

  if (fire_events)
    {
      EVENTS_PUSH (EventType::ET_BPM_CHANGED, NULL);
    }
}

void
tempo_track_set_bpm_from_str (void * _self, const char * str)
{
  Track * self = (Track *) _self;
  g_return_if_fail (IS_TRACK_AND_NONNULL (self));

  bpm_t bpm = (float) atof (str);
  if (math_floats_equal (bpm, 0))
    {
      ui_show_error_message (
        _ ("Invalid BPM Value"), _ ("Please enter a positive decimal number"));
      return;
    }

  tempo_track_set_bpm (
    self, bpm, tempo_track_get_current_bpm (self), Z_F_NOT_TEMPORARY,
    F_PUBLISH_EVENTS);
}

int
tempo_track_beat_unit_enum_to_int (ZBeatUnit ebeat_unit)
{
  int bu = 0;
  switch (ebeat_unit)
    {
    case ZBeatUnit::Z_BEAT_UNIT_2:
      bu = 2;
      break;
    case ZBeatUnit::Z_BEAT_UNIT_4:
      bu = 4;
      break;
    case ZBeatUnit::Z_BEAT_UNIT_8:
      bu = 8;
      break;
    case ZBeatUnit::Z_BEAT_UNIT_16:
      bu = 16;
      break;
    default:
      g_warn_if_reached ();
      break;
    }

  return bu;
}

void
tempo_track_set_beat_unit_from_enum (Track * self, ZBeatUnit ebeat_unit)
{
  g_return_if_fail (
    !engine_get_run (AUDIO_ENGINE)
    || (ROUTER && router_is_processing_kickoff_thread (ROUTER)));

  port_set_control_value (
    self->beat_unit_port, static_cast<float> (ebeat_unit), F_NOT_NORMALIZED,
    F_PUBLISH_EVENTS);
  EVENTS_PUSH (EventType::ET_TIME_SIGNATURE_CHANGED, NULL);
}

ZBeatUnit
tempo_track_beat_unit_to_enum (int beat_unit)
{
  switch (beat_unit)
    {
    case 2:
      return ZBeatUnit::Z_BEAT_UNIT_2;
    case 4:
      return ZBeatUnit::Z_BEAT_UNIT_4;
    case 8:
      return ZBeatUnit::Z_BEAT_UNIT_8;
    case 16:
      return ZBeatUnit::Z_BEAT_UNIT_16;
      ;
    default:
      break;
    }
  g_return_val_if_reached (ENUM_INT_TO_VALUE (ZBeatUnit, 0));
}

ZBeatUnit
tempo_track_get_beat_unit_enum (Track * self)
{
  return tempo_track_beat_unit_to_enum (tempo_track_get_beat_unit (self));
}

void
tempo_track_set_beat_unit (Track * self, int beat_unit)
{
  ZBeatUnit ebu = tempo_track_beat_unit_to_enum (beat_unit);
  tempo_track_set_beat_unit_from_enum (self, ebu);
}

/**
 * Updates beat unit and anything depending on it.
 */
void
tempo_track_set_beats_per_bar (Track * self, int beats_per_bar)
{
  g_return_if_fail (
    !engine_get_run (AUDIO_ENGINE)
    || (ROUTER && router_is_processing_kickoff_thread (ROUTER)));

  port_set_control_value (
    self->beats_per_bar_port, beats_per_bar, F_NOT_NORMALIZED, F_PUBLISH_EVENTS);
  EVENTS_PUSH (EventType::ET_TIME_SIGNATURE_CHANGED, NULL);
}

int
tempo_track_get_beats_per_bar (Track * self)
{
  return (int) math_round_float_to_signed_32 (self->beats_per_bar_port->control);
}

int
tempo_track_get_beat_unit (Track * self)
{
  ZBeatUnit ebu =
    (ZBeatUnit) math_round_float_to_signed_32 (self->beat_unit_port->control);
  return tempo_track_beat_unit_enum_to_int (ebu);
}

/**
 * Removes all objects from the tempo track.
 *
 * Mainly used in testing.
 */
void
tempo_track_clear (Track * self)
{
  /* TODO */
  /*g_warn_if_reached ();*/
}
