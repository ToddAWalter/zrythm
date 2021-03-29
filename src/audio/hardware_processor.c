/*
 * Copyright (C) 2020-2021 Alexandros Theodotou <alex at zrythm dot org>
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

#include "audio/engine.h"
#include "audio/ext_port.h"
#include "audio/hardware_processor.h"
#include "audio/midi_event.h"
#include "audio/port.h"
#include "project.h"
#include "settings/settings.h"
#include "utils/arrays.h"
#include "utils/flags.h"
#include "utils/mem.h"
#include "utils/objects.h"
#include "utils/string.h"
#include "zrythm.h"
#include "zrythm_app.h"

void
hardware_processor_init_loaded (
  HardwareProcessor * self)
{
  self->ext_audio_ports_size =
    (size_t) self->num_ext_audio_ports;
  self->ext_midi_ports_size =
    (size_t) self->num_ext_midi_ports;
}

/**
 * Returns a new empty instance.
 */
HardwareProcessor *
hardware_processor_new (
  bool      input)
{
  HardwareProcessor * self =
    object_new (HardwareProcessor);

  self->schema_version = HW_PROCESSOR_SCHEMA_VERSION;

  self->is_input = input;

  return self;
}

/**
 * Finds an ext port from its ID (type + full name).
 *
 * @see ext_port_get_id()
 */
ExtPort *
hardware_processor_find_ext_port (
  HardwareProcessor * self,
  const char *        id)
{
  g_return_val_if_fail (self && id, NULL);

  for (int i = 0; i < self->num_ext_audio_ports;
       i++)
    {
      ExtPort * port = self->ext_audio_ports[i];
      char * port_id = ext_port_get_id (port);
      if (string_is_equal (port_id, id))
        {
          g_free (port_id);
          return port;
        }
      g_free (port_id);
    }
  for (int i = 0; i < self->num_ext_midi_ports; i++)
    {
      ExtPort * port = self->ext_midi_ports[i];
      char * port_id = ext_port_get_id (port);
      if (string_is_equal (port_id, id))
        {
          g_free (port_id);
          return port;
        }
      g_free (port_id);
    }

  return NULL;
}

/**
 * Finds a port from its ID (type + full name).
 *
 * @see ext_port_get_id()
 */
Port *
hardware_processor_find_port (
  HardwareProcessor * self,
  const char *        id)
{
  g_return_val_if_fail (self && id, NULL);

  for (int i = 0; i < self->num_ext_audio_ports;
       i++)
    {
      ExtPort * ext_port = self->ext_audio_ports[i];
      char * ext_port_id =
        ext_port_get_id (ext_port);
      Port * port = self->audio_ports[i];
      if (!PROJECT || !PROJECT->loaded)
        {
          port->magic = PORT_MAGIC;
        }
      g_return_val_if_fail (
        ext_port && IS_PORT (port), NULL);
      if (string_is_equal (ext_port_id, id))
        {
          g_free (ext_port_id);
          return port;
        }
      g_free (ext_port_id);
    }
  for (int i = 0; i < self->num_ext_midi_ports; i++)
    {
      ExtPort * ext_port = self->ext_midi_ports[i];
      char * ext_port_id =
        ext_port_get_id (ext_port);
      Port * port = self->midi_ports[i];
      if (!PROJECT || !PROJECT->loaded)
        {
          port->magic = PORT_MAGIC;
        }
      g_return_val_if_fail (
        ext_port && IS_PORT (port), NULL);
      if (string_is_equal (ext_port_id, id))
        {
          g_free (ext_port_id);
          return port;
        }
      g_free (ext_port_id);
    }

  return NULL;
}

static Port *
create_port_for_ext_port (
  ExtPort * ext_port,
  PortType  type,
  PortFlow  flow)
{
  Port * port =
    port_new_with_type (
      type, flow, ext_port->full_name);
  port->id.owner_type = PORT_OWNER_TYPE_HW;
  port->id.flags |= PORT_FLAG_HW;
  port->id.ext_port_id = ext_port_get_id (ext_port);
  port->is_project = true;

  return port;
}

/**
 * Rescans the hardware ports and appends any missing
 * ones.
 */
bool
hardware_processor_rescan_ext_ports (
  HardwareProcessor * self)
{
  g_debug ("rescanning ports...");

  /* get correct flow */
  PortFlow flow =
  /* these are reversed:
   * input here -> port that outputs in backend */
    self->is_input ? FLOW_OUTPUT : FLOW_INPUT;

  /* collect audio ports */
  int num_ext_audio_ports = 0;
  ext_ports_get (
    TYPE_AUDIO, flow,
    true, NULL, &num_ext_audio_ports);
  ExtPort * ext_audio_ports[num_ext_audio_ports * 2];
  ext_ports_get (
    TYPE_AUDIO, flow,
    true, ext_audio_ports, &num_ext_audio_ports);

  /* add missing ports to the list */
  for (int i = 0; i < num_ext_audio_ports; i++)
    {
      ExtPort * ext_port = ext_audio_ports[i];
      ExtPort * existing_port =
        hardware_processor_find_ext_port (
          self, ext_port_get_id (ext_port));

      if (!existing_port)
        {
          array_double_size_if_full (
            self->ext_audio_ports,
            self->num_ext_audio_ports,
            self->ext_audio_ports_size,
            ExtPort *);
          array_append (
            self->ext_audio_ports,
            self->num_ext_audio_ports, ext_port);
          char * id = ext_port_get_id (ext_port);
          g_message (
            "[HW] Added audio port %s", id);
          g_free (id);
        }
    }

  /* collect midi ports */
  int num_ext_midi_ports = 0;
  ext_ports_get (
    TYPE_EVENT, flow,
    true, NULL, &num_ext_midi_ports);
  ExtPort * ext_midi_ports[num_ext_midi_ports * 2];
  ext_ports_get (
    TYPE_EVENT, flow,
    true, ext_midi_ports, &num_ext_midi_ports);

  /* add missing ports to the list */
  for (int i = 0; i < num_ext_midi_ports; i++)
    {
      ExtPort * ext_port = ext_midi_ports[i];
      ExtPort * existing_port =
        hardware_processor_find_ext_port (
          self, ext_port_get_id (ext_port));

      if (!existing_port)
        {
          array_double_size_if_full (
            self->ext_midi_ports,
            self->num_ext_midi_ports,
            self->ext_midi_ports_size,
            ExtPort *);
          array_append (
            self->ext_midi_ports,
            self->num_ext_midi_ports, ext_port);
          char * id = ext_port_get_id (ext_port);
          g_message (
            "[HW] Added MIDI port %s", id);
          g_free (id);
        }
    }

  /* create ports for each ext port */
  self->audio_ports =
    realloc (
      self->audio_ports,
      self->ext_audio_ports_size *
        sizeof (Port *));
  self->midi_ports =
    realloc (
      self->midi_ports,
      self->ext_midi_ports_size *
        sizeof (Port *));
  for (int i = 0; i < self->num_ext_audio_ports; i++)
    {
      ExtPort * ext_port = self->ext_audio_ports[i];
      g_return_val_if_fail (ext_port, G_SOURCE_REMOVE);
      if (i >= self->num_audio_ports)
        {
          self->audio_ports[i] =
            create_port_for_ext_port (
              ext_port, TYPE_AUDIO, FLOW_OUTPUT);
          self->num_audio_ports++;
        }

      g_return_val_if_fail (
        IS_PORT (self->audio_ports[i]),
        G_SOURCE_REMOVE);
    }
  for (int i = 0; i < self->num_ext_midi_ports; i++)
    {
      ExtPort * ext_port = self->ext_midi_ports[i];
      g_return_val_if_fail (ext_port, G_SOURCE_REMOVE);
      if (i >= self->num_midi_ports)
        {
          self->midi_ports[i] =
            create_port_for_ext_port (
              ext_port, TYPE_EVENT, FLOW_OUTPUT);
          self->num_midi_ports++;
        }

      g_return_val_if_fail (
        IS_PORT (self->midi_ports[i]),
        G_SOURCE_REMOVE);
    }

  /* TODO deactivate ports that weren't found
   * (stop engine temporarily to remove) */

  g_debug (
    "[%s] have %d audio and %d MIDI ports",
    self->is_input ?
      "HW processor inputs" :
      "HW processor outputs",
    self->num_ext_audio_ports,
    self->num_ext_midi_ports);

  for (int i = 0; i < self->num_ext_audio_ports;
       i++)
    {
      char * id =
        ext_port_get_id (self->ext_audio_ports[i]);
      g_debug (
        "[%s] audio: %s",
        self->is_input ?
          "HW processor input" :
          "HW processor output",
        id);
      g_free (id);
    }
  for (int i = 0; i < self->num_ext_midi_ports;
       i++)
    {
      char * id =
        ext_port_get_id (self->ext_midi_ports[i]);
      g_debug (
        "[%s] MIDI: %s",
        self->is_input ?
          "HW processor input" :
          "HW processor output",
        id);
      g_free (id);
    }

  return G_SOURCE_CONTINUE;
}

/**
 * Sets up the ports but does not start them.
 *
 * @return Non-zero on fail.
 */
int
hardware_processor_setup (
  HardwareProcessor * self)
{
  if (ZRYTHM_TESTING || ZRYTHM_GENERATING_PROJECT)
    return 0;

  g_return_val_if_fail (
    ZRYTHM_APP_IS_GTK_THREAD && S_P_GENERAL_ENGINE,
    -1);

  if (self->is_input)
    {
      /* cache selections */
      self->selected_midi_ports =
        g_settings_get_strv (
          S_P_GENERAL_ENGINE, "midi-controllers");
      self->selected_audio_ports =
        g_settings_get_strv (
          S_P_GENERAL_ENGINE, "audio-inputs");

      /* get counts */
      self->num_selected_midi_ports = 0;
      self->num_selected_audio_ports = 0;
      while ((self->selected_midi_ports[
                self->num_selected_midi_ports++]));
      self->num_selected_midi_ports--;
      while ((self->selected_audio_ports[
                self->num_selected_audio_ports++]));
      self->num_selected_audio_ports--;
    }

  /* ---- scan current ports ---- */

  hardware_processor_rescan_ext_ports (self);

  /* ---- end scan ---- */

  /* add timer to keep rescanning */
  g_timeout_add_seconds (
    7,
    (GSourceFunc) hardware_processor_rescan_ext_ports,
    self);

  self->setup = true;

  return 0;
}

/*void*/
/*hardware_processor_find_or_create_rtaudio_dev (*/
  /*HardwareProcessor * self,*/

/**
 * Starts or stops the ports.
 *
 * @param activate True to activate, false to
 *   deactivate
 */
void
hardware_processor_activate (
  HardwareProcessor * self,
  bool                activate)
{
  g_message ("hw processor activate: %d", activate);

  /* go through each selected port and activate/
   * deactivate */
  for (int i = 0; i < self->num_selected_midi_ports;
       i++)
    {
      char * selected_port =
        self->selected_midi_ports[i];
      ExtPort * ext_port =
        hardware_processor_find_ext_port (
          self, selected_port);
      Port * port =
        hardware_processor_find_port (
          self, selected_port);
      if (port && ext_port)
        {
          ext_port_activate (
            ext_port, port, activate);
        }
      else
        {
          g_message (
            "could not find port %s", selected_port);
        }
    }
  for (int i = 0; i < self->num_selected_audio_ports;
       i++)
    {
      char * selected_port =
        self->selected_audio_ports[i];
      ExtPort * ext_port =
        hardware_processor_find_ext_port (
          self, selected_port);
      Port * port =
        hardware_processor_find_port (
          self, selected_port);
      if (port && ext_port)
        {
          ext_port_activate (
            ext_port, port, activate);
        }
      else
        {
          g_message (
            "could not find port %s", selected_port);
        }
    }

  self->activated = activate;
}

/**
 * Processes the data.
 */
void
hardware_processor_process (
  HardwareProcessor * self,
  nframes_t           nframes)
{
  /* go through each selected port and fetch data */
  for (int i = 0; i < self->num_audio_ports; i++)
    {
      ExtPort * ext_port = self->ext_audio_ports[i];
      if (!ext_port->active)
        continue;

      Port * port = self->audio_ports[i];

      /* clear the buffer */
      port_clear_buffer (port);

      switch (AUDIO_ENGINE->audio_backend)
        {
#ifdef HAVE_JACK
        case AUDIO_BACKEND_JACK:
          port_receive_audio_data_from_jack (
            port, 0, nframes);
          break;
#endif
#ifdef HAVE_RTAUDIO
        case AUDIO_BACKEND_ALSA_RTAUDIO:
        case AUDIO_BACKEND_JACK_RTAUDIO:
        case AUDIO_BACKEND_PULSEAUDIO_RTAUDIO:
        case AUDIO_BACKEND_COREAUDIO_RTAUDIO:
        case AUDIO_BACKEND_WASAPI_RTAUDIO:
        case AUDIO_BACKEND_ASIO_RTAUDIO:
          /* extract audio data from the RtAudio
           * device ring buffer into RtAudio
           * device temp buffer */
          port_prepare_rtaudio_data (port);

          /* copy data from RtAudio temp buffer
           * to normal buffer */
          port_sum_data_from_rtaudio (
            port, 0, nframes);
          break;
#endif
        default:
          break;
        }
    }
  for (int i = 0; i < self->num_midi_ports; i++)
    {
      ExtPort * ext_port = self->ext_midi_ports[i];
      if (!ext_port->active)
        continue;

      Port * port = self->midi_ports[i];

      /* clear the buffer */
      midi_events_clear (
        port->midi_events, F_NOT_QUEUED);

      switch (AUDIO_ENGINE->midi_backend)
        {
#ifdef HAVE_JACK
        case MIDI_BACKEND_JACK:
          port_receive_midi_events_from_jack (
            port, 0, nframes);
          break;
#endif
#ifdef HAVE_RTMIDI
        case MIDI_BACKEND_ALSA_RTMIDI:
        case MIDI_BACKEND_JACK_RTMIDI:
        case MIDI_BACKEND_WINDOWS_MME_RTMIDI:
        case MIDI_BACKEND_COREMIDI_RTMIDI:
          /* extract MIDI events from the RtMidi
           * device ring buffer into RtMidi device */
          port_prepare_rtmidi_events (port);

          /* copy data from RtMidi device events
           * to normal events */
          port_sum_data_from_rtmidi (
            port, 0, nframes);
          break;
#endif
        default:
          break;
        }
    }
}
