/*
 * Copyright (C) 2018-2021 Alexandros Theodotou <alex at zrythm dot org>
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

#include <math.h>

#include "audio/control_port.h"
#include "audio/engine.h"
#include "audio/port.h"
#include "audio/track.h"
#include "gui/backend/event.h"
#include "gui/backend/event_manager.h"
#include "plugins/plugin.h"
#include "project.h"
#include "utils/flags.h"
#include "utils/math.h"
#include "zrythm.h"
#include "zrythm_app.h"

/**
 * Get the current real value of the control.
 */
float
control_port_get_val (
  Port * self)
{
  return self->control;
}

/**
 * Returns if the control port is toggled.
 */
bool
control_port_is_toggled (
  Port * self)
{
  return control_port_is_val_toggled (self->control);
}

/**
 * Checks if the given value is toggled.
 */
bool
control_port_is_val_toggled (
  float val)
{
  return val > 0.001f;
}

/**
 * Gets the control value for an integer port.
 */
int
control_port_get_int (
  Port * self)
{
  return
    control_port_get_int_from_val (self->control);
}

/**
 * Gets the control value for an integer port.
 */
int
control_port_get_int_from_val (
  float val)
{
  return math_round_float_to_int (val);
}

/**
 * Returns the snapped value (eg, if toggle,
 * returns 0.f or 1.f).
 */
float
control_port_get_snapped_val (
  Port * self)
{
  float val = control_port_get_val (self);

  return
    control_port_get_snapped_val_from_val (
      self, val);
}

/**
 * Returns the snapped value (eg, if toggle,
 * returns 0.f or 1.f).
 */
float
control_port_get_snapped_val_from_val (
  Port * self,
  float  val)
{
  PortFlags flags = self->id.flags;
  if (flags & PORT_FLAG_TOGGLE)
    {
      return
        control_port_is_val_toggled (val) ?
          1.f : 0.f;
    }
  else if (flags & PORT_FLAG_INTEGER)
    {
      return
        (float) control_port_get_int_from_val (val);
    }

  return val;
}

/**
 * Converts normalized value (0.0 to 1.0) to
 * real value (eg. -10.0 to 100.0).
 */
float
control_port_normalized_val_to_real (
  Port * self,
  float  normalized_val)
{
  PortIdentifier * id = &self->id;
  if (id->flags & PORT_FLAG_PLUGIN_CONTROL)
    {
      if (id->flags & PORT_FLAG_LOGARITHMIC)
        {
          /* make sure none of the values is 0 */
          float minf =
            math_floats_equal (self->minf, 0.f) ?
            1e-20f : self->minf;
          float maxf =
            math_floats_equal (self->maxf, 0.f) ?
            1e-20f : self->maxf;
          normalized_val =
            math_floats_equal (normalized_val, 0.f) ?
            1e-20f : normalized_val;

          /* see http://lv2plug.in/ns/ext/port-props/port-props.html#rangeSteps */
          return
            minf *
              powf (maxf / minf, normalized_val);
        }
      else if (id->flags & PORT_FLAG_TOGGLE)
        {
          return
            normalized_val >= 0.001f ? 1.f : 0.f;
        }
      else
        {
          return
            self->minf +
            normalized_val *
              (self->maxf - self->minf);
        }
    }
  else if (id->flags & PORT_FLAG_TOGGLE)
    {
      return normalized_val > 0.0001f;
    }
  else if (id->flags & PORT_FLAG_CHANNEL_FADER)
    {
      return
        (float) math_get_amp_val_from_fader (
          normalized_val);
    }
  else if (id->flags & PORT_FLAG_AUTOMATABLE)
    {
      return
        self->minf +
        normalized_val * (self->maxf - self->minf);
    }
  else
    {
      return normalized_val;
    }
  g_return_val_if_reached (normalized_val);
}

/**
 * Converts real value (eg. -10.0 to 100.0) to
 * normalized value (0.0 to 1.0).
 */
float
control_port_real_val_to_normalized (
  Port * self,
  float  real_val)
{
  PortIdentifier * id = &self->id;
  if (id->flags & PORT_FLAG_PLUGIN_CONTROL)
    {
      if (self->id.flags & PORT_FLAG_LOGARITHMIC)
        {
          /* make sure none of the values is 0 */
          float minf =
            math_floats_equal (self->minf, 0.f) ?
            1e-20f : self->minf;
          float maxf =
            math_floats_equal (self->maxf, 0.f) ?
            1e-20f : self->maxf;
          real_val =
            math_floats_equal (real_val, 0.f) ?
            1e-20f : real_val;

          /* see http://lv2plug.in/ns/ext/port-props/port-props.html#rangeSteps */
          return
            logf (real_val / minf) /
            logf (maxf / minf);
        }
      else if (self->id.flags & PORT_FLAG_TOGGLE)
        {
          return real_val;
        }
      else
        {
          float sizef = self->maxf - self->minf;
          return
            (sizef - (self->maxf - real_val)) /
            sizef;
        }
    }
  else if (id->flags & PORT_FLAG_TOGGLE)
    {
      return real_val;
    }
  else if (id->flags & PORT_FLAG_CHANNEL_FADER)
    {
      return
        (float)
        math_get_fader_val_from_amp (real_val);
    }
  else if (id->flags & PORT_FLAG_AUTOMATABLE)
    {
      float sizef = self->maxf - self->minf;
      return
        (sizef - (self->maxf - real_val)) /
        sizef;
    }
  else
    {
      return real_val;
    }
  g_return_val_if_reached (0.f);
}

/**
 * Updates the actual value.
 *
 * The given value is always a normalized 0.0-1.0
 * value and must be translated to the actual value
 * before setting it.
 *
 * @param automating Whether this is from an
 *   automation event. This will set Lv2Port's
 *   automating field to true, which will cause the
 *   plugin to receive a UI event for this change.
 */
void
control_port_set_val_from_normalized (
  Port * self,
  float  val,
  bool   automating)
{
  PortIdentifier * id = &self->id;
  if (id->flags & PORT_FLAG_PLUGIN_CONTROL)
    {
      float real_val =
        control_port_normalized_val_to_real (
          self, val);
      if (!math_floats_equal (
            port_get_control_value (
              self, F_NORMALIZE),
            real_val))
        {
          EVENTS_PUSH (
            ET_AUTOMATION_VALUE_CHANGED, self);
        }

      port_set_control_value (
        self, real_val, F_NOT_NORMALIZED,
        F_PUBLISH_EVENTS);
      self->automating = automating;
      self->base_value = real_val;
    }
  else if (id->flags & PORT_FLAG_TOGGLE)
    {
      float real_val =
        control_port_normalized_val_to_real (
          self, val);
      if (!math_floats_equal (self->control, real_val))
        {
          EVENTS_PUSH (
            ET_AUTOMATION_VALUE_CHANGED, self);
          if (real_val > 0.0001f)
            self->control = 1.f;
          else
            self->control = 0.f;
        }

      if (id->flags & PORT_FLAG_CHANNEL_MUTE)
        {
          Track * track = port_get_track (self, 1);
          track_set_muted (
            track, self->control > 0.0001f, 0, 1);
        }
    }
  else if (id->flags & PORT_FLAG_CHANNEL_FADER)
    {
      Track * track = port_get_track (self, 1);
      Channel * ch = track_get_channel (track);
      if (!math_floats_equal (
            fader_get_fader_val (
              ch->fader), val))
        {
          EVENTS_PUSH (
            ET_AUTOMATION_VALUE_CHANGED, self);
        }
      fader_set_amp (
        ch->fader,
        (float)
        math_get_amp_val_from_fader (val));
    }
  else if (id->flags & PORT_FLAG_STEREO_BALANCE)
    {
      Track * track = port_get_track (self, true);
      Channel * ch = track_get_channel (track);
      if (!math_floats_equal (
            channel_get_balance_control (ch), val))
        {
          EVENTS_PUSH (
            ET_AUTOMATION_VALUE_CHANGED, self);
        }
      channel_set_balance_control (ch, val);
    }
  else if (id->flags & PORT_FLAG_MIDI_AUTOMATABLE)
    {
      float real_val =
        self->minf +
        val * (self->maxf - self->minf);
      if (!math_floats_equal (val, self->control))
        {
          EVENTS_PUSH (
            ET_AUTOMATION_VALUE_CHANGED, self);
        }
      port_set_control_value (
        self, real_val, 0, 0);
    }
  else if (id->flags & PORT_FLAG_AUTOMATABLE)
    {
      float real_val =
        control_port_normalized_val_to_real (
          self, val);
      if (!math_floats_equal (
            real_val, self->control))
        {
          EVENTS_PUSH (
            ET_AUTOMATION_VALUE_CHANGED, self);
        }
      port_set_control_value (
        self, real_val, F_NOT_NORMALIZED,
        F_NO_PUBLISH_EVENTS);
    }
  else
    {
      g_warn_if_reached ();
    }
}
