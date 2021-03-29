/*
 * Copyright (C) 2019-2020 Alexandros Theodotou <alex at zrythm dot org>
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

#include "zrythm-config.h"

#include "audio/engine.h"
#include "audio/engine_jack.h"
#include "audio/engine_rtaudio.h"
#include "audio/engine_rtmidi.h"
#include "audio/ext_port.h"
#include "audio/rtaudio_device.h"
#include "audio/rtmidi_device.h"
#include "audio/windows_mme_device.h"
#include "project.h"
#include "utils/objects.h"
#include "zrythm_app.h"

#ifdef HAVE_JACK
#include "weak_libjack.h"
#endif

static ExtPort *
_create (void)
{
  ExtPort * self = object_new (ExtPort);
  self->schema_version = EXT_PORT_SCHEMA_VERSION;

  return self;
}

/**
 * Inits the ExtPort after loading a project.
 */
void
ext_port_init_loaded (
  ExtPort * ext_port)
{
  /* TODO */
}

/**
 * Returns the buffer of the external port.
 */
float *
ext_port_get_buffer (
  ExtPort * self,
  nframes_t nframes)
{
  switch (self->type)
    {
#ifdef HAVE_JACK
    case EXT_PORT_TYPE_JACK:
      return
        (float *)
        jack_port_get_buffer (
          self->jport,
          nframes);
      break;
#endif
#ifdef HAVE_ALSA
    case EXT_PORT_TYPE_ALSA:
#endif
#ifdef HAVE_RTMIDI
    case EXT_PORT_TYPE_RTMIDI:
#endif
    default:
      g_return_val_if_reached (NULL);
      break;
    }
g_return_val_if_reached (NULL);
}

/**
 * Returns a unique identifier (full name prefixed
 * with backend type).
 */
char *
ext_port_get_id (
  ExtPort * self)
{
  return
    g_strdup_printf (
      "%s/%s",
      ext_port_type_strings[self->type].str,
      self->full_name);
}

/**
 * Clears the buffer of the external port.
 */
void
ext_port_clear_buffer (
  ExtPort * ext_port,
  nframes_t nframes)
{
  float * buf =
    ext_port_get_buffer (ext_port, nframes);
  if (!buf)
    return;

  g_message ("clearing buffer of %p", ext_port);

  memset (buf, 0, nframes * sizeof (float));
}

/**
 * Activates the port (starts receiving data) or
 * deactivates it.
 *
 * @param port Port to send the output to.
 */
void
ext_port_activate (
  ExtPort * self,
  Port *    port,
  bool      activate)
{
  g_message (
    "attempting to %sactivate ext port %s",
    activate ? "" : "de", self->full_name);

  /*char str[600];*/
  int ret;
  (void) ret; /* avoid unused warnings */
  if (activate)
    {
      if (self->is_midi)
        {
          switch (AUDIO_ENGINE->midi_backend)
            {
#ifdef HAVE_JACK
            case MIDI_BACKEND_JACK:
              if (self->type != EXT_PORT_TYPE_JACK)
                {
                  g_message (
                    "skipping %s (not JACK)",
                    self->full_name);
                  return;
                }

              self->port = port;

              /* expose the port and connect to
               * JACK port */
              if (!self->jport)
                {
                  self->jport =
                    jack_port_by_name (
                      AUDIO_ENGINE->client,
                      self->full_name);
                }
              if (!self->jport)
                {
                  g_warning (
                    "Could not find external JACK "
                    "port '%s', skipping...",
                    self->full_name);
                  return;
                }
              port_set_expose_to_backend (
                self->port, true);

              g_message (
                "connecting jack port %s to "
                "jack port %s",
                jack_port_name (self->jport),
                jack_port_name (
                  JACK_PORT_T (self->port->data)));

              ret =
                jack_connect (
                  AUDIO_ENGINE->client,
                  jack_port_name (self->jport),
                  jack_port_name (
                    JACK_PORT_T (self->port->data)));
              if (ret)
                {
                  char msg[600];
                  engine_jack_get_error_message (
                    ret, msg);
                  g_warning (
                    "Failed connecting %s to %s:\n"
                    "%s",
                      jack_port_name (self->jport),
                      jack_port_name (
                        JACK_PORT_T (
                          self->port->data)),
                    msg);
                  return;
                }
              break;
#endif
#ifdef HAVE_RTMIDI
            case MIDI_BACKEND_ALSA_RTMIDI:
            case MIDI_BACKEND_JACK_RTMIDI:
            case MIDI_BACKEND_WINDOWS_MME_RTMIDI:
            case MIDI_BACKEND_COREMIDI_RTMIDI:
              if (self->type != EXT_PORT_TYPE_RTMIDI)
                {
                  g_message (
                    "skipping %s (not RtMidi)",
                    self->full_name);
                  return;
                }
              self->port = port;
              self->rtmidi_dev =
                rtmidi_device_new (
                  true, self->full_name,
                  0, self->port);
              if (!self->rtmidi_dev)
                {
                  g_warning (
                    "Failed creating RtMidi device "
                    "for %s", self->full_name);
                  return;
                }
              ret =
                rtmidi_device_open (
                  self->rtmidi_dev, true);
              self->port->rtmidi_ins[0] =
                self->rtmidi_dev;
              self->port->num_rtmidi_ins = 1;
              g_warn_if_fail (ret == 0);
              break;
#endif
            default:
              break;
            }
        }
      else
        {
          switch (AUDIO_ENGINE->audio_backend)
            {
#ifdef HAVE_JACK
            case AUDIO_BACKEND_JACK:
              if (self->type != EXT_PORT_TYPE_JACK)
                {
                  g_message (
                    "skipping %s (not JACK)",
                    self->full_name);
                  return;
                }
              self->port = port;

              /* expose the port and connect to
               * JACK port */
              if (!self->jport)
                {
                  self->jport =
                    jack_port_by_name (
                      AUDIO_ENGINE->client,
                      self->full_name);
                }
              if (!self->jport)
                {
                  g_warning (
                    "Could not find external JACK "
                    "port '%s', skipping...",
                    self->full_name);
                  return;
                }
              port_set_expose_to_backend (
                self->port, true);

              g_message (
                "connecting jack port %s to "
                "jack port %s",
                jack_port_name (self->jport),
                jack_port_name (
                  JACK_PORT_T (self->port->data)));

              jack_connect (
                AUDIO_ENGINE->client,
                jack_port_name (self->jport),
                jack_port_name (
                  JACK_PORT_T (self->port->data)));
              break;
#endif
#ifdef HAVE_RTAUDIO
            case AUDIO_BACKEND_ALSA_RTAUDIO:
            case AUDIO_BACKEND_JACK_RTAUDIO:
            case AUDIO_BACKEND_PULSEAUDIO_RTAUDIO:
            case AUDIO_BACKEND_COREAUDIO_RTAUDIO:
            case AUDIO_BACKEND_WASAPI_RTAUDIO:
            case AUDIO_BACKEND_ASIO_RTAUDIO:
              if (self->type != EXT_PORT_TYPE_RTAUDIO)
                {
                  g_message (
                    "skipping %s (not RtAudio)",
                    self->full_name);
                  return;
                }
              self->port = port;
              self->rtaudio_dev =
                rtaudio_device_new (
                  true, self->rtaudio_dev_name,
                  0, self->rtaudio_channel_idx,
                  self->port);
              ret =
                rtaudio_device_open (
                  self->rtaudio_dev, true);
              if (ret)
                {
                  return;
                }
              self->port->rtaudio_ins[0] =
                self->rtaudio_dev;
              self->port->num_rtaudio_ins = 1;
              break;
#endif
            default:
              break;
            }
        }
    }

  self->active = activate;
}

/**
 * Exposes the given Port if not exposed and makes
 * the connection from the Port to the ExtPort (eg in
 * JACK) or backwards.
 *
 * @param src 1 if the ext_port is the source, 0 if it
 *   is the destination.
 */
void
ext_port_connect (
  ExtPort * ext_port,
  Port *    port,
  int       src)
{
  /* TODO */
}

/**
 * Disconnects the Port from the ExtPort.
 *
 * @param src 1 if the ext_port is the source, 0 if it
 *   is the destination.
 */
void
ext_port_disconnect (
  ExtPort * ext_port,
  Port *    port,
  int       src)
{
  /* TODO */
}

/**
 * Returns if the ext port matches the current
 * backend.
 */
bool
ext_port_matches_backend (
  ExtPort * self)
{
  if (!self->is_midi)
    {
      switch (AUDIO_ENGINE->audio_backend)
        {
#ifdef HAVE_JACK
        case AUDIO_BACKEND_JACK:
          if (self->type == EXT_PORT_TYPE_JACK)
            return true;
          else
            return false;
#endif
#ifdef HAVE_RTAUDIO
        case AUDIO_BACKEND_ALSA_RTAUDIO:
        case AUDIO_BACKEND_JACK_RTAUDIO:
        case AUDIO_BACKEND_PULSEAUDIO_RTAUDIO:
        case AUDIO_BACKEND_COREAUDIO_RTAUDIO:
        case AUDIO_BACKEND_WASAPI_RTAUDIO:
        case AUDIO_BACKEND_ASIO_RTAUDIO:
          if (self->type == EXT_PORT_TYPE_RTAUDIO)
            return true;
          else
            return false;
#endif
#ifdef HAVE_ALSA
        case AUDIO_BACKEND_ALSA:
          break;
#endif
        default:
          break;
        }
    }
  else
    {
      switch (AUDIO_ENGINE->midi_backend)
        {
#ifdef HAVE_JACK
        case MIDI_BACKEND_JACK:
          if (self->type == EXT_PORT_TYPE_JACK)
            return true;
          else
            return false;
#endif
#ifdef HAVE_ALSA
        case MIDI_BACKEND_ALSA:
          break;
#endif
#ifdef _WOE32
        case MIDI_BACKEND_WINDOWS_MME:
          g_warning ("TODO");
          break;
#endif
#ifdef HAVE_RTMIDI
        case MIDI_BACKEND_ALSA_RTMIDI:
        case MIDI_BACKEND_JACK_RTMIDI:
        case MIDI_BACKEND_WINDOWS_MME_RTMIDI:
        case MIDI_BACKEND_COREMIDI_RTMIDI:
          if (self->type == EXT_PORT_TYPE_RTMIDI)
            return true;
          else
            return false;
#endif
        default:
          break;
        }
    }
  return false;
}

#ifdef HAVE_JACK
/**
 * Creates an ExtPort from a JACK port.
 */
static ExtPort *
ext_port_from_jack_port (
  jack_port_t * jport)
{
  ExtPort * self = _create ();

  self->jport = jport;
  self->full_name =
    g_strdup (jack_port_name (jport));
  self->short_name  =
    g_strdup (jack_port_short_name (jport));
  self->type = EXT_PORT_TYPE_JACK;

  char * aliases[2];
  aliases[0] =
    (char*)
    malloc (
      (size_t) jack_port_name_size ());
  aliases[1] =
    (char*)
    malloc (
      (size_t) jack_port_name_size ());
  self->num_aliases =
    jack_port_get_aliases (
      jport, aliases);

  if (self->num_aliases == 2)
    {
      self->alias2 = g_strdup (aliases[1]);
      self->alias1 = g_strdup (aliases[0]);
    }
  else if (self->num_aliases == 1)
    self->alias1 = g_strdup (aliases[0]);

  free (aliases[0]);
  free (aliases[1]);

  return self;
}

static void
get_ext_ports_from_jack (
  PortType   type,
  PortFlow   flow,
  int        hw,
  ExtPort ** arr,
  int *      size)
{
  long unsigned int flags = 0;
  if (hw)
    flags |= JackPortIsPhysical;
  if (flow == FLOW_INPUT)
    flags |= JackPortIsInput;
  else if (flow == FLOW_OUTPUT)
    flags |= JackPortIsOutput;
  const char * jtype =
    engine_jack_get_jack_type (type);
  if (!jtype)
    return;

  if (!AUDIO_ENGINE->client)
    {
      g_critical (
        "JACK client is NULL. make sure to call "
        "engine_pre_setup() before calling this");
      return;
    }

  const char ** ports =
    jack_get_ports (
      AUDIO_ENGINE->client,
      NULL, jtype, flags);

  if (!ports)
    return;

  int i = 0;
  jack_port_t * jport;
  while (ports[i] != NULL)
    {
      jport =
        jack_port_by_name (
          AUDIO_ENGINE->client,
          ports[i]);

      if (arr)
        {
          arr[*size + i] =
            ext_port_from_jack_port (jport);
        }

      i++;
    }

  jack_free (ports);

  *size += i;
}
#endif

#ifdef _WOE32
/**
 * Creates an ExtPort from a Windows MME device.
 */
static ExtPort *
ext_port_from_windows_mme_device (
  WindowsMmeDevice * dev)
{
  ExtPort * self =
    calloc (1, sizeof (ExtPort));

  self->mme_dev = dev;
  self->full_name = g_strdup (dev->name);
  self->type = EXT_PORT_TYPE_WINDOWS_MME;

  return self;
}

static void
get_ext_ports_from_windows_mme (
  PortFlow   flow,
  ExtPort ** arr,
  int *      size)
{
  WindowsMmeDevice * dev;
	    int i = 0;
  if (flow == FLOW_OUTPUT)
    {
      for (i = 0;
           i < AUDIO_ENGINE->num_mme_in_devs; i++)
        {
          if (arr)
            {
              dev = AUDIO_ENGINE->mme_in_devs[i];
              g_return_if_fail (dev);
              arr[*size + i] =
                ext_port_from_windows_mme_device (
                  dev);
            }
        }
      (*size) += i;
    }
  else if (flow == FLOW_INPUT)
    {
      for (i = 0;
           i < AUDIO_ENGINE->num_mme_out_devs; i++)
        {
          if (arr)
            {
              dev = AUDIO_ENGINE->mme_out_devs[i];
              g_return_if_fail (dev);
              arr[*size + i] =
                ext_port_from_windows_mme_device (
                  dev);
            }
        }
      (*size) += i;
    }
}
#endif

#ifdef HAVE_RTMIDI
/**
 * Creates an ExtPort from a RtMidi port.
 */
static ExtPort *
ext_port_from_rtmidi (
  unsigned int id)
{
  ExtPort * self = _create ();

  RtMidiDevice * dev =
    rtmidi_device_new (1, NULL, id, NULL);
  self->rtmidi_id = id;
  self->full_name =
    g_strdup (
      rtmidi_get_port_name (dev->in_handle, id));
  self->type = EXT_PORT_TYPE_RTMIDI;
  rtmidi_device_free (dev);

  return self;
}

static void
get_ext_ports_from_rtmidi (
  PortFlow   flow,
  ExtPort ** arr,
  int *      size)
{
  *size = 0;
  if (flow == FLOW_OUTPUT)
    {
      unsigned int num_ports =
        engine_rtmidi_get_num_in_ports (
          AUDIO_ENGINE);
      unsigned int i;
      for (i = 0; i < num_ports; i++)
        {
          if (arr)
            {
              arr[*size + (int) i] =
                ext_port_from_rtmidi (i);
            }
        }
      (*size) += (int) i;
    }
  else if (flow == FLOW_INPUT)
    {
      /* MIDI out devices not handled yet */
    }
}
#endif

#ifdef HAVE_RTAUDIO
/**
 * Creates an ExtPort from a RtAudio port.
 */
static ExtPort *
ext_port_from_rtaudio (
  unsigned int id,
  unsigned int channel_idx,
  bool         is_input,
  bool         is_duplex)
{
  ExtPort * self = _create ();

  RtAudioDevice * dev =
    rtaudio_device_new (
      1, NULL, id, channel_idx, NULL);
  self->rtaudio_id = id;
  self->rtaudio_channel_idx = channel_idx;
  self->rtaudio_is_input = is_input;
  self->rtaudio_is_duplex = is_duplex;
  self->rtaudio_dev_name = g_strdup (dev->name);
  self->full_name =
    g_strdup_printf (
      "%s (in %d)", dev->name, channel_idx);
  self->type = EXT_PORT_TYPE_RTAUDIO;
  rtaudio_device_free (dev);

  return self;
}

static void
get_ext_ports_from_rtaudio (
  PortFlow   flow,
  ExtPort ** arr,
  int *      size)
{
  *size = 0;
  /* note: this is an output port from the graph
   * side that will be used as an input port on
   * the zrythm side */
  if (flow == FLOW_OUTPUT)
    {
      rtaudio_t rtaudio =
        engine_rtaudio_create_rtaudio (AUDIO_ENGINE);
      if (!rtaudio)
        {
          g_warn_if_reached ();
          return;
        }
      int num_devs = rtaudio_device_count (rtaudio);
      for (unsigned int i = 0;
           i < (unsigned int) num_devs; i++)
        {
          rtaudio_device_info_t dev_nfo =
            rtaudio_get_device_info (
              rtaudio, (int) i);
          if (dev_nfo.input_channels > 0)
            {
              for (unsigned int j = 0;
                   j < dev_nfo.input_channels; j++)
                {
                  if (arr)
                    {
                      arr[*size] =
                        ext_port_from_rtaudio (
                          i, j, true, false);
                    }
                  (*size)++;
                }
#if 0
              for (unsigned int j = 0;
                   j < dev_nfo.duplex_channels; j++)
                {
                  arr[*size] =
                    ext_port_from_rtaudio (
                      i, j, false, true);
                  (*size)++;
                  g_message ("\n\n found duplex device");
                }
#endif
            }
          else
            {
              continue;
            }
        }
      rtaudio_destroy (rtaudio);
    }
  else if (flow == FLOW_INPUT)
    {
      rtaudio_t rtaudio =
        engine_rtaudio_create_rtaudio (AUDIO_ENGINE);
      if (!rtaudio)
        {
          g_warn_if_reached ();
          return;
        }
      int num_devs = rtaudio_device_count (rtaudio);
      for (unsigned int i = 0;
           i < (unsigned int) num_devs; i++)
        {
          rtaudio_device_info_t dev_nfo =
            rtaudio_get_device_info (
              rtaudio, (int) i);
          if (dev_nfo.output_channels > 0)
            {
              for (unsigned int j = 0;
                   j < dev_nfo.output_channels; j++)
                {
                  if (arr)
                    {
                      arr[*size] =
                        ext_port_from_rtaudio (
                          i, j, false, false);
                    }
                  (*size)++;
                }
#if 0
              for (unsigned int j = 0;
                   j < dev_nfo.duplex_channels; j++)
                {
                  arr[*size] =
                    ext_port_from_rtaudio (
                      i, j, false, true);
                  (*size)++;
                  g_message ("\n\n found duplex device");
                }
#endif
            }
          else
            {
              continue;
            }
        }
    }
}
#endif

/**
 * Collects external ports of the given type.
 *
 * @param flow The signal flow. Note that this is
 *   inverse to what Zrythm sees. E.g., to get
 *   MIDI inputs like MIDI keyboards, pass
 *   \ref FLOW_OUTPUT here.
 * @param hw Hardware or not.
 * @param ports An array of ExtPort pointers to fill
 *   in. The array should be preallocated.
 * @param size Size of the array to fill in.
 */
void
ext_ports_get (
  PortType   type,
  PortFlow   flow,
  bool       hw,
  ExtPort ** arr,
  int *      size)
{
  *size = 0;
  if (type == TYPE_AUDIO)
    {
      switch (AUDIO_ENGINE->audio_backend)
        {
#ifdef HAVE_JACK
        case AUDIO_BACKEND_JACK:
          get_ext_ports_from_jack (
            type, flow, hw, arr, size);
          break;
#endif
#ifdef HAVE_RTAUDIO
        case AUDIO_BACKEND_ALSA_RTAUDIO:
        case AUDIO_BACKEND_JACK_RTAUDIO:
        case AUDIO_BACKEND_PULSEAUDIO_RTAUDIO:
        case AUDIO_BACKEND_COREAUDIO_RTAUDIO:
        case AUDIO_BACKEND_WASAPI_RTAUDIO:
        case AUDIO_BACKEND_ASIO_RTAUDIO:
          get_ext_ports_from_rtaudio (
            flow, arr, size);
          break;
#endif
#ifdef HAVE_ALSA
        case AUDIO_BACKEND_ALSA:
          break;
#endif
        default:
          break;
        }
    }
  else if (type == TYPE_EVENT)
    {
      switch (AUDIO_ENGINE->midi_backend)
        {
#ifdef HAVE_JACK
        case MIDI_BACKEND_JACK:
          get_ext_ports_from_jack (
            type, flow, hw, arr, size);
          break;
#endif
#ifdef HAVE_ALSA
        case MIDI_BACKEND_ALSA:
          break;
#endif
#ifdef _WOE32
        case MIDI_BACKEND_WINDOWS_MME:
          get_ext_ports_from_windows_mme (
            flow, arr, size);
          break;
#endif
#ifdef HAVE_RTMIDI
        case MIDI_BACKEND_ALSA_RTMIDI:
        case MIDI_BACKEND_JACK_RTMIDI:
        case MIDI_BACKEND_WINDOWS_MME_RTMIDI:
        case MIDI_BACKEND_COREMIDI_RTMIDI:
          get_ext_ports_from_rtmidi (
            flow, arr, size);
          break;
#endif
        default:
          break;
        }
      if (arr)
        {
          for (int i = 0; i < *size; i++)
            {
              arr[i]->is_midi = true;
            }
        }
    }
}

/**
 * Prints the port info.
 */
void
ext_port_print (
  ExtPort * self)
{
  g_message (
    "Ext port:\n"
    "full name: %s",
    self->full_name);
}

/**
 * Creates a shallow clone of the port.
 */
ExtPort *
ext_port_clone (
  ExtPort * ext_port)
{
  g_return_val_if_fail (ext_port, NULL);

  ExtPort * newport = _create ();

#ifdef HAVE_JACK
  newport->jport = ext_port->jport;
#endif
#ifdef _WOE32
  newport->mme_dev = ext_port->mme_dev;
#endif
#ifdef HAVE_RTMIDI
  newport->rtmidi_id = ext_port->rtmidi_id;
#endif
  newport->rtaudio_channel_idx =
    ext_port->rtaudio_channel_idx;
  newport->rtaudio_dev_name =
    ext_port->rtaudio_dev_name;
#ifdef HAVE_RTAUDIO
  newport->rtaudio_id = ext_port->rtaudio_id;
  newport->rtaudio_is_input =
    ext_port->rtaudio_is_input;
  newport->rtaudio_is_duplex =
    ext_port->rtaudio_is_duplex;
#endif
  if (ext_port->full_name)
    newport->full_name =
      g_strdup (ext_port->full_name);
  if (ext_port->short_name)
    newport->short_name =
      g_strdup (ext_port->short_name);
  if (ext_port->alias1)
    newport->alias1 =
    g_strdup (ext_port->alias1);
  if (ext_port->alias2)
    newport->alias2 =
    g_strdup (ext_port->alias2);
  newport->num_aliases = ext_port->num_aliases;
  newport->type = ext_port->type;

  return newport;
}

/**
 * Checks in the GSettings whether this port is
 * marked as enabled by the user.
 *
 * @note Not realtime safe.
 *
 * @return Whether the port is enabled.
 */
bool
ext_port_get_enabled (
  ExtPort * self)
{
  /* TODO */
  return true;
}

/**
 * Frees an array of ExtPort pointers.
 */
void
ext_ports_free (
  ExtPort ** ext_ports,
  int        size)
{
  for (int i = 0; i < size; i++)
    {
      g_warn_if_fail (!ext_ports[i]);
      object_free_w_func_and_null (
        ext_port_free, ext_ports[i]);
    }
}

/**
 * Frees the ext_port.
 */
void
ext_port_free (
  ExtPort * self)
{
  g_free_and_null (self->full_name);
  g_free_and_null (self->short_name);
  g_free_and_null (self->alias1);
  g_free_and_null (self->alias2);

  object_zero_and_free (self);
}
