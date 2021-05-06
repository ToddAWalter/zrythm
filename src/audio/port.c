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

#include "zrythm-config.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "project.h"
#include "audio/channel.h"
#include "audio/clip.h"
#include "audio/control_port.h"
#include "audio/engine_jack.h"
#include "audio/graph.h"
#include "audio/hardware_processor.h"
#include "audio/master_track.h"
#include "audio/midi_event.h"
#include "audio/pan.h"
#include "audio/port.h"
#include "audio/router.h"
#include "audio/rtaudio_device.h"
#include "audio/rtmidi_device.h"
#include "audio/tempo_track.h"
#include "audio/windows_mme_device.h"
#include "gui/backend/event.h"
#include "gui/backend/event_manager.h"
#include "gui/widgets/channel.h"
#include "plugins/carla_native_plugin.h"
#include "plugins/plugin.h"
#include "plugins/lv2/lv2_ui.h"
#include "utils/arrays.h"
#include "utils/dsp.h"
#include "utils/error.h"
#include "utils/flags.h"
#include "utils/math.h"
#include "utils/mem.h"
#include "utils/object_utils.h"
#include "utils/objects.h"
#include "utils/string.h"
#include "zix/ring.h"
#include "zrythm_app.h"

#include <gtk/gtk.h>

#define SLEEPTIME_USEC 60

#define AUDIO_RING_SIZE 65536

static void
allocate_buf (
  Port * self)
{
  if (self->id.type == TYPE_CV ||
      self->id.type == TYPE_AUDIO)
    {
      if (!self->buf)
        {
          size_t max =
            MAX (
              AUDIO_ENGINE->block_length,
              self->min_buf_size);
          max = MAX (max, 1);
          self->buf =
            object_new_n (max, float);
        }
    }
}

static void
alloc_srcs (
  Port * self)
{
  self->srcs = object_new_n (1, Port *);
  self->src_ids =
    object_new_n (1, PortIdentifier);
  self->src_multipliers = object_new_n (1, float);
  self->src_locked = object_new_n (1, int);
  self->src_enabled = object_new_n (1, int);
  self->srcs_size = 1;
}

static void
alloc_dests (
  Port * self)
{
  self->dests = object_new_n (1, Port *);
  self->dest_ids =
    object_new_n (1, PortIdentifier);
  self->multipliers = object_new_n (1, float);
  self->dest_locked = object_new_n (1, int);
  self->dest_enabled = object_new_n (1, int);
  self->dests_size = 1;
}

static void
realloc_dests (
  Port * src,
  size_t prev_sz,
  size_t new_sz)
{
  src->dests =
    object_realloc_n (
      src->dests, prev_sz, new_sz, Port *);
  src->dest_ids =
    object_realloc_n (
      src->dest_ids, prev_sz, new_sz,
      PortIdentifier);
  src->multipliers =
    object_realloc_n (
      src->multipliers, prev_sz, new_sz, float);
  src->dest_locked =
    object_realloc_n (
      src->dest_locked, prev_sz, new_sz, int);
  src->dest_enabled =
    object_realloc_n (
      src->dest_enabled, prev_sz, new_sz, int);

  src->dests_size = new_sz;
}

static void
realloc_srcs (
  Port * dest,
  size_t prev_sz,
  size_t new_sz)
{
  dest->srcs =
    object_realloc_n (
      dest->srcs, prev_sz, new_sz, Port *);
  dest->src_ids =
    object_realloc_n (
      dest->src_ids, prev_sz, new_sz,
      PortIdentifier);
  dest->src_multipliers =
    object_realloc_n (
      dest->src_multipliers, prev_sz, new_sz,
      float);
  dest->src_locked =
    object_realloc_n (
      dest->src_locked, prev_sz, new_sz, int);
  dest->src_enabled =
    object_realloc_n (
      dest->src_enabled, prev_sz, new_sz, int);

  dest->srcs_size = new_sz;
}

/**
 * This function finds the Ports corresponding to
 * the PortIdentifiers for srcs and dests.
 *
 * Should be called after the ports are loaded from
 * yml.
 */
void
port_init_loaded (
  Port * self,
  bool   is_project)
{
#if 0
  g_debug (
    "%s, is project: %d",
    self->id.label, is_project);
#endif

  self->magic = PORT_MAGIC;

  self->is_project = is_project;
  self->unsnapped_control = self->control;

  if (self->num_dests == 0)
    {
      alloc_dests (self);
    }
  else
    {
      g_warn_if_fail (self->num_dests > 0);
      self->dests_size = (size_t) self->num_dests;
    }
  self->dests =
    object_new_n (self->dests_size, Port *);

  if (self->num_srcs == 0)
    {
      alloc_srcs (self);
    }
  else
    {
      g_warn_if_fail (self->num_srcs > 0);
      self->srcs_size = (size_t) self->num_srcs;
    }
  self->srcs =
    object_new_n (self->srcs_size, Port *);

  if (!is_project)
    return;

  PortIdentifier * id;
  for (int i = 0; i < self->num_srcs; i++)
    {
      id = &self->src_ids[i];
      if (!self->srcs[i])
        {
          self->srcs[i] =
            port_find_from_identifier (id);
        }
      g_warn_if_fail (self->srcs[i]);
    }
  for (int i = 0; i < self->num_dests; i++)
    {
      id = &self->dest_ids[i];
      if (!self->dests[i])
        {
          self->dests[i] =
            port_find_from_identifier (id);
        }
      g_warn_if_fail (self->dests[i]);
    }

  allocate_buf (self);

  switch (self->id.type)
    {
    case TYPE_EVENT:
      if (!self->midi_events)
        {
          self->midi_events = midi_events_new ();
        }
      if (!self->midi_ring)
        {
          self->midi_ring =
            zix_ring_new (
              sizeof (MidiEvent) * (size_t) 11);
        }
#ifdef _WOE32
      if (AUDIO_ENGINE->midi_backend ==
            MIDI_BACKEND_WINDOWS_MME)
        {
          zix_sem_init (
            &self->mme_connections_sem, 1);
        }
#endif
      break;
    case TYPE_AUDIO:
      /* fall through */
    case TYPE_CV:
      if (!self->audio_ring)
        {
          self->audio_ring =
            zix_ring_new (
              sizeof (float) * AUDIO_RING_SIZE);
        }
    default:
      break;
    }

  if (self->id.flags & PORT_FLAG_AUTOMATABLE)
    {
      if (!self->at)
        {
          self->at =
            automation_track_find_from_port (
              self, NULL, false);
        }
      g_return_if_fail (self->at);
    }
}

/**
 * Finds the Port corresponding to the identifier.
 *
 * @param id The PortIdentifier to use for
 *   searching.
 */
Port *
port_find_from_identifier (
  PortIdentifier * id)
{
  Track * tr = NULL;
  Channel * ch = NULL;
  Plugin * pl = NULL;
  PortFlags flags = id->flags;
  PortFlags2 flags2 = id->flags2;
  switch (id->owner_type)
    {
    case PORT_OWNER_TYPE_BACKEND:
      switch (id->type)
        {
        case TYPE_EVENT:
          if (id->flow == FLOW_OUTPUT)
            { /* TODO */ }
          else if (id->flow == FLOW_INPUT)
            {
              if (flags & PORT_FLAG_MANUAL_PRESS)
                return
                  AUDIO_ENGINE->
                    midi_editor_manual_press;
            }
          break;
        case TYPE_AUDIO:
          if (id->flow == FLOW_OUTPUT)
            {
              if (flags & PORT_FLAG_STEREO_L)
                return
                  AUDIO_ENGINE->monitor_out->l;
              else if (flags &
                         PORT_FLAG_STEREO_R)
                return
                  AUDIO_ENGINE->monitor_out->r;
            }
          else if (id->flow == FLOW_INPUT)
            {
              /* none */
            }
          break;
        default:
          break;
        }
      break;
    case PORT_OWNER_TYPE_PLUGIN:
      tr = TRACKLIST->tracks[id->track_pos];
      g_warn_if_fail (IS_TRACK_AND_NONNULL (tr));
      switch (id->plugin_id.slot_type)
        {
        case PLUGIN_SLOT_MIDI_FX:
          pl =
            tr->channel->midi_fx[
              id->plugin_id.slot];
          break;
        case PLUGIN_SLOT_INSTRUMENT:
          pl = tr->channel->instrument;
          break;
        case PLUGIN_SLOT_INSERT:
          pl =
            tr->channel->inserts[
              id->plugin_id.slot];
          break;
        case PLUGIN_SLOT_MODULATOR:
          pl =
            tr->modulators[id->plugin_id.slot];
          break;
        default:
          g_return_val_if_reached (NULL);
          break;
        }
      g_warn_if_fail (IS_PLUGIN (pl));
      switch (id->flow)
        {
        case FLOW_INPUT:
          return
            pl->in_ports[id->port_index];
          break;
        case FLOW_OUTPUT:
          return
            pl->out_ports[id->port_index];
          break;
        default:
          g_return_val_if_reached (NULL);
          break;
        }
      break;
    case PORT_OWNER_TYPE_TRACK_PROCESSOR:
      tr = TRACKLIST->tracks[id->track_pos];
      g_warn_if_fail (tr);
      switch (id->type)
        {
        case TYPE_EVENT:
          if (id->flow == FLOW_OUTPUT)
            {
              return tr->processor->midi_out;
            }
          else if (id->flow == FLOW_INPUT)
            {
              if (flags & PORT_FLAG_PIANO_ROLL)
                return tr->processor->piano_roll;
              else
                return tr->processor->midi_in;
            }
          break;
        case TYPE_AUDIO:
          if (id->flow == FLOW_OUTPUT)
            {
              if (flags & PORT_FLAG_STEREO_L)
                return tr->processor->stereo_out->l;
              else if (flags &
                         PORT_FLAG_STEREO_R)
                return tr->processor->stereo_out->r;
            }
          else if (id->flow == FLOW_INPUT)
            {
              g_return_val_if_fail (
                tr->processor->stereo_in, NULL);
              if (flags & PORT_FLAG_STEREO_L)
                return tr->processor->stereo_in->l;
              else if (flags & PORT_FLAG_STEREO_R)
                return tr->processor->stereo_in->r;
            }
          break;
        case TYPE_CONTROL:
          if (flags & PORT_FLAG_TP_MONO)
            {
              return tr->processor->mono;
            }
          else if (flags &
                     PORT_FLAG_TP_INPUT_GAIN)
            {
              return tr->processor->input_gain;
            }
          else if (flags2 &
                     PORT_FLAG2_TP_OUTPUT_GAIN)
            {
              return tr->processor->output_gain;
            }
          else if (flags &
                     PORT_FLAG_MIDI_AUTOMATABLE)
            {
              if (flags2 &
                    PORT_FLAG2_MIDI_PITCH_BEND)
                {
                  return
                    tr->processor->pitch_bend[
                      id->port_index];
                }
              else if (flags2 &
                    PORT_FLAG2_MIDI_POLY_KEY_PRESSURE)
                {
                  return
                    tr->processor->poly_key_pressure[
                      id->port_index];
                }
              else if (flags2 &
                    PORT_FLAG2_MIDI_CHANNEL_PRESSURE)
                {
                  return
                    tr->processor->channel_pressure[
                      id->port_index];
                }
              else
                {
                  return
                    tr->processor->midi_cc[
                      id->port_index];
                }
            }
          break;
        default:
          break;
        }
      break;
    case PORT_OWNER_TYPE_TRACK:
      tr = TRACKLIST->tracks[id->track_pos];
      g_warn_if_fail (tr);
      if (flags & PORT_FLAG_BPM)
        {
          return tr->bpm_port;
        }
      else if (flags2 & PORT_FLAG2_BEATS_PER_BAR)
        {
          return tr->beats_per_bar_port;
        }
      else if (flags2 & PORT_FLAG2_BEAT_UNIT)
        {
          return tr->beat_unit_port;
        }
      else if (
        flags & PORT_FLAG_MODULATOR_MACRO)
        {
          ModulatorMacroProcessor * processor =
            tr->modulator_macros[id->port_index];
          if (id->flow == FLOW_INPUT)
            {
              if (id->type == TYPE_CV)
                {
                  return processor->cv_in;
                }
              else if (id->type == TYPE_CONTROL)
                {
                  return processor->macro;
                }
            }
          else if (id->flow == FLOW_OUTPUT)
            {
              return processor->cv_out;
            }
        }
      ch = tr->channel;
      g_warn_if_fail (ch);
      switch (id->type)
        {
        case TYPE_EVENT:
          if (id->flow == FLOW_OUTPUT)
            {
              return ch->midi_out;
            }
          break;
        case TYPE_AUDIO:
          if (id->flow == FLOW_OUTPUT)
            {
              if (flags & PORT_FLAG_STEREO_L)
                return ch->stereo_out->l;
              else if (flags &
                         PORT_FLAG_STEREO_R)
                return ch->stereo_out->r;
            }
          break;
        default:
          break;
        }
      break;
    case PORT_OWNER_TYPE_FADER:
      g_warn_if_fail (id->track_pos >= 0);
      tr = TRACKLIST->tracks[id->track_pos];
      g_warn_if_fail (tr);
      ch = tr->channel;
      g_warn_if_fail (ch);
      switch (id->type)
        {
        case TYPE_EVENT:
          switch (id->flow)
            {
            case FLOW_INPUT:
              return ch->fader->midi_in;
              break;
            case FLOW_OUTPUT:
              return ch->fader->midi_out;
              break;
            default:
              break;
            }
          break;
        case TYPE_AUDIO:
          if (id->flow == FLOW_OUTPUT)
            {
              if (flags & PORT_FLAG_STEREO_L)
                return ch->fader->stereo_out->l;
              else if (flags &
                         PORT_FLAG_STEREO_R)
                return ch->fader->stereo_out->r;
            }
          else if (id->flow == FLOW_INPUT)
            {
              if (flags & PORT_FLAG_STEREO_L)
                return ch->fader->stereo_in->l;
              else if (flags &
                         PORT_FLAG_STEREO_R)
                return ch->fader->stereo_in->r;
            }
          break;
        case TYPE_CONTROL:
          if (id->flow == FLOW_INPUT)
            {
              if (flags &
                    PORT_FLAG_AMPLITUDE)
                {
                  return ch->fader->amp;
                }
              else if (flags &
                         PORT_FLAG_STEREO_BALANCE)
                {
                  return ch->fader->balance;
                }
              else if (flags &
                         PORT_FLAG_CHANNEL_MUTE)
                {
                  return ch->fader->mute;
                }
            }
          break;
        default:
          break;
        }
      break;
    case PORT_OWNER_TYPE_PREFADER:
      g_warn_if_fail (id->track_pos > -1);
      tr = TRACKLIST->tracks[id->track_pos];
      g_warn_if_fail (tr);
      ch = tr->channel;
      g_warn_if_fail (ch);
      switch (id->type)
        {
        case TYPE_EVENT:
          switch (id->flow)
            {
            case FLOW_INPUT:
              return ch->prefader->midi_in;
              break;
            case FLOW_OUTPUT:
              return ch->prefader->midi_out;
              break;
            default:
              break;
            }
          break;
        case TYPE_AUDIO:
          if (id->flow == FLOW_OUTPUT)
            {
              if (flags & PORT_FLAG_STEREO_L)
                return ch->prefader->stereo_out->l;
              else if (flags &
                         PORT_FLAG_STEREO_R)
                return ch->prefader->stereo_out->r;
            }
          else if (id->flow == FLOW_INPUT)
            {
              if (flags & PORT_FLAG_STEREO_L)
                return ch->prefader->stereo_in->l;
              else if (flags &
                         PORT_FLAG_STEREO_R)
                return ch->prefader->stereo_in->r;
            }
          break;
        case TYPE_CONTROL:
          if (id->flow == FLOW_INPUT)
            {
              if (flags &
                    PORT_FLAG_AMPLITUDE)
                {
                  return ch->prefader->amp;
                }
              else if (flags &
                         PORT_FLAG_STEREO_BALANCE)
                {
                  return ch->prefader->balance;
                }
              else if (flags &
                         PORT_FLAG_CHANNEL_MUTE)
                {
                  return ch->prefader->mute;
                }
            }
          break;
        default:
          break;
        }
      break;
    case PORT_OWNER_TYPE_CHANNEL_SEND:
      g_warn_if_fail (id->track_pos > -1);
      tr = TRACKLIST->tracks[id->track_pos];
      g_warn_if_fail (tr);
      ch = tr->channel;
      g_warn_if_fail (ch);
      if (id->flags2 &
            PORT_FLAG2_CHANNEL_SEND_ENABLED)
        {
          return ch->sends[id->port_index]->enabled;
        }
      else if (id->flags2 &
                 PORT_FLAG2_CHANNEL_SEND_AMOUNT)
        {
          return ch->sends[id->port_index]->amount;
        }
      else
        {
          g_return_val_if_reached (NULL);
        }
      break;
    case PORT_OWNER_TYPE_SAMPLE_PROCESSOR:
      if (flags & PORT_FLAG_STEREO_L)
        return SAMPLE_PROCESSOR->stereo_out->l;
      else if (flags &
                 PORT_FLAG_STEREO_R)
        return SAMPLE_PROCESSOR->stereo_out->r;
      else
        g_return_val_if_reached (NULL);
      break;
    case PORT_OWNER_TYPE_MONITOR_FADER:
      if (id->flow == FLOW_OUTPUT)
        {
          if (flags & PORT_FLAG_STEREO_L)
            return
              MONITOR_FADER->
                stereo_out->l;
          else if (flags &
                     PORT_FLAG_STEREO_R)
            return
              MONITOR_FADER->
                stereo_out->r;
        }
      else if (id->flow == FLOW_INPUT)
        {
          if (flags & PORT_FLAG_STEREO_L)
            {
              return
                MONITOR_FADER->stereo_in->l;
            }
          else if (flags & PORT_FLAG_STEREO_R)
            {
              return
                MONITOR_FADER->stereo_in->r;
            }
        }
      break;
    case PORT_OWNER_TYPE_HW:
      {
        Port * port = NULL;

        /* note: flows are reversed */
        if (id->flow == FLOW_OUTPUT)
          {
            port =
              hardware_processor_find_port (
                HW_IN_PROCESSOR, id->ext_port_id);
          }
        else if (id->flow == FLOW_INPUT)
          {
            port =
              hardware_processor_find_port (
                HW_OUT_PROCESSOR, id->ext_port_id);
          }

        /* only warn when hardware is not
         * connected anymore */
        g_warn_if_fail (port);
        /*g_return_val_if_fail (port, NULL);*/
        return port;
      }
      break;
    case PORT_OWNER_TYPE_TRANSPORT:
      switch (id->type)
        {
        case TYPE_EVENT:
          if (id->flow == FLOW_INPUT)
            {
              if (flags2 &
                    PORT_FLAG2_TRANSPORT_ROLL)
                return TRANSPORT->roll;
              if (flags2 &
                    PORT_FLAG2_TRANSPORT_STOP)
                return TRANSPORT->stop;
              if (flags2 &
                    PORT_FLAG2_TRANSPORT_BACKWARD)
                return TRANSPORT->backward;
              if (flags2 &
                    PORT_FLAG2_TRANSPORT_FORWARD)
                return TRANSPORT->forward;
              if (flags2 &
                    PORT_FLAG2_TRANSPORT_LOOP_TOGGLE)
                return TRANSPORT->loop_toggle;
              if (flags2 &
                    PORT_FLAG2_TRANSPORT_REC_TOGGLE)
                return TRANSPORT->rec_toggle;
            }
          break;
        default:
          break;
        }
      break;
    default:
      g_return_val_if_reached (NULL);
    }

  g_return_val_if_reached (NULL);
}

void
stereo_ports_init_loaded (
  StereoPorts * sp,
  bool          is_project)
{
  port_init_loaded (sp->l, is_project);
  port_init_loaded (sp->r, is_project);
}

/**
 * Creates port.
 *
 * Sets id and updates appropriate counters.
 */
RETURNS_NONNULL
static Port *
_port_new (
  const char * label)
{
  g_message (
    "Creating port %s...", label);

  Port * self = object_new (Port);

  self->schema_version = PORT_SCHEMA_VERSION;
  port_identifier_init (&self->id);
  self->magic = PORT_MAGIC;

  self->num_dests = 0;
  self->id.flow = FLOW_UNKNOWN;
  self->id.label = g_strdup (label);

  alloc_srcs (self);
  alloc_dests (self);

  /*g_message ("%s: done (%p)", __func__, self);*/

  return self;
}

/**
 * Creates port.
 */
Port *
port_new_with_type (
  PortType     type,
  PortFlow     flow,
  const char * label)
{
  Port * self = _port_new (label);

  self->id.type = type;
  if (self->id.type == TYPE_EVENT)
    self->midi_events = midi_events_new ();
  self->id.flow = flow;

  switch (type)
    {
    case TYPE_EVENT:
      self->maxf = 1.f;
      self->midi_events = midi_events_new ();
      self->midi_ring =
        zix_ring_new (
          sizeof (MidiEvent) * (size_t) 11);
#ifdef _WOE32
      if (AUDIO_ENGINE->midi_backend ==
            MIDI_BACKEND_WINDOWS_MME)
        {
          zix_sem_init (
            &self->mme_connections_sem, 1);
        }
#endif
      break;
    case TYPE_CONTROL:
      self->minf = 0.f;
      self->maxf = 1.f;
      self->zerof = 0.f;
      break;
    case TYPE_AUDIO:
      self->minf = 0.f;
      self->maxf = 2.f;
      self->zerof = 0.f;
      self->audio_ring =
        zix_ring_new (
          sizeof (float) * AUDIO_RING_SIZE);
      break;
    case TYPE_CV:
      self->minf = -1.f;
      self->maxf = 1.f;
      self->zerof = 0.f;
      self->audio_ring =
        zix_ring_new (
          sizeof (float) * AUDIO_RING_SIZE);
    default:
      break;
    }

  allocate_buf (self);

  g_return_val_if_fail (IS_PORT (self), NULL);

  g_return_val_if_fail (
    self->magic == PORT_MAGIC, NULL);

  return self;
}

/**
 * Creates stereo ports.
 */
StereoPorts *
stereo_ports_new_from_existing (
  Port * l, Port * r)
{
  StereoPorts * sp = object_new (StereoPorts);
  sp->schema_version = STEREO_PORTS_SCHEMA_VERSION;
  sp->l = l;
  l->id.flags |= PORT_FLAG_STEREO_L;
  r->id.flags |= PORT_FLAG_STEREO_R;
  sp->r = r;

  return sp;
}

void
stereo_ports_fill_from_clip (
  StereoPorts * self,
  AudioClip *   clip,
  long          g_start_frames,
  nframes_t     start_frame,
  nframes_t     nframes)
{
  channels_t max_channels = MAX (2, clip->channels);
  for (nframes_t i = start_frame;
       i < start_frame + nframes;
       i++)
    {
      /* no more frames to read */
      if (g_start_frames + i > clip->num_frames)
        {
          return;
        }

      if (max_channels == 1)
        {
          self->l->buf[i] =
            clip->frames[g_start_frames + i];
          self->r->buf[i] =
            clip->frames[g_start_frames + i];
        }
      else if (max_channels == 2)
        {
          self->l->buf[i] =
            clip->frames[
              (g_start_frames + i) * 2];
          self->r->buf[i] =
            clip->frames[
              (g_start_frames + i) * 2 + 1];
        }
    }
}

void
stereo_ports_disconnect (
  StereoPorts * self)
{
  port_disconnect_all (self->l);
  port_disconnect_all (self->r);
}

void
stereo_ports_free (
  StereoPorts * self)
{
  object_free_w_func_and_null (
    port_free, self->l);
  object_free_w_func_and_null (
    port_free, self->r);

  object_zero_and_free (self);
}

#ifdef HAVE_JACK
void
port_receive_midi_events_from_jack (
  Port *          self,
  const nframes_t start_frame,
  const nframes_t nframes)
{
  if (self->internal_type !=
        INTERNAL_JACK_PORT ||
      self->id.type !=
        TYPE_EVENT)
    return;

  void * port_buf =
    jack_port_get_buffer (
      JACK_PORT_T (self->data), nframes);
  uint32_t num_events =
    jack_midi_get_event_count (port_buf);

  jack_midi_event_t jack_ev;
  for(unsigned i = 0; i < num_events; i++)
    {
      jack_midi_event_get (
        &jack_ev, port_buf, i);

      if (jack_ev.time >= start_frame &&
          jack_ev.time < start_frame + nframes)
        {
          midi_byte_t channel =
            jack_ev.buffer[0] & 0xf;
          Track * track =
            port_get_track (self, 0);
          if (self->id.owner_type ==
                PORT_OWNER_TYPE_TRACK_PROCESSOR &&
              !track)
            {
              g_return_if_reached ();
            }

          if (self->id.owner_type ==
                PORT_OWNER_TYPE_TRACK_PROCESSOR &&
              (track->type ==
                   TRACK_TYPE_MIDI ||
                 track->type ==
                   TRACK_TYPE_INSTRUMENT) &&
                !track->channel->
                  all_midi_channels &&
                !track->channel->
                  midi_channels[channel])
            {
              /* different channel */
            }
          else if (jack_ev.size == 3)
            {
              midi_events_add_event_from_buf (
                self->midi_events,
                jack_ev.time, jack_ev.buffer,
                (int) jack_ev.size, F_NOT_QUEUED);
            }
        }
    }

  if (self->midi_events->num_events > 0)
    {
      MidiEvent * ev =
        &self->midi_events->events[0];
      char designation[600];
      port_get_full_designation (
        self, designation);
      g_debug (
        "JACK MIDI (%s): have %d events\n"
        "first event is: [%u] %hhx %hhx %hhx",
        designation, num_events,
        ev->time, ev->raw_buffer[0],
        ev->raw_buffer[1], ev->raw_buffer[2]);
    }
}

void
port_receive_audio_data_from_jack (
  Port *          self,
  const nframes_t start_frames,
  const nframes_t nframes)
{
  if (self->internal_type !=
        INTERNAL_JACK_PORT ||
      self->id.type != TYPE_AUDIO)
    return;

  float * in;
  in =
    (float *)
    jack_port_get_buffer (
      JACK_PORT_T (self->data),
      AUDIO_ENGINE->nframes);

  dsp_add2 (
    &self->buf[start_frames],
    &in[start_frames], nframes);
}

static void
send_midi_events_to_jack (
  Port *      port,
  const nframes_t         start_frames,
  const nframes_t   nframes)
{
  if (port->internal_type !=
        INTERNAL_JACK_PORT ||
      port->id.type != TYPE_EVENT)
    return;

  jack_port_t * jport = JACK_PORT_T (port->data);

  if (jack_port_connected (jport) <= 0)
    {
      return;
    }

  midi_events_copy_to_jack (
    port->midi_events,
    jack_port_get_buffer (
      jport, AUDIO_ENGINE->nframes));
}

static void
send_audio_data_to_jack (
  Port *      port,
  const nframes_t         start_frames,
  const nframes_t         nframes)
{
  if (port->internal_type !=
        INTERNAL_JACK_PORT ||
      port->id.type != TYPE_AUDIO)
    return;

  jack_port_t * jport = JACK_PORT_T (port->data);

  if (jack_port_connected (jport) <= 0)
    {
      return;
    }

  float * out;
  out =
    (float *)
    jack_port_get_buffer (
      jport, AUDIO_ENGINE->nframes);

#ifdef TRIAL_VER
  if (AUDIO_ENGINE->limit_reached)
    {
      dsp_fill (&out[start_frames], 0.f, nframes);
    }
  else
    {
#endif
      dsp_copy (
        &out[start_frames],
        &port->buf[start_frames], nframes);
#ifdef TRIAL_VER
    }
#endif
}

/**
 * Sums the inputs coming in from JACK, before the
 * port is processed.
 */
static void
sum_data_from_jack (
  Port * self,
  const nframes_t start_frame,
  const nframes_t nframes)
{
  if (self->id.owner_type ==
        PORT_OWNER_TYPE_BACKEND ||
      self->internal_type != INTERNAL_JACK_PORT ||
      self->id.flow != FLOW_INPUT)
    return;

  /* append events from JACK if any */
  if (AUDIO_ENGINE->midi_backend ==
        MIDI_BACKEND_JACK)
    {
      port_receive_midi_events_from_jack (
        self, start_frame, nframes);
    }

  /* audio */
  if (AUDIO_ENGINE->audio_backend ==
        AUDIO_BACKEND_JACK)
    {
      port_receive_audio_data_from_jack (
        self, start_frame, nframes);
    }
}

/**
 * Sends the port data to JACK, after the port
 * is processed.
 */
static void
send_data_to_jack (
  Port * self,
  const nframes_t start_frame,
  const nframes_t nframes)
{
  if (self->internal_type != INTERNAL_JACK_PORT ||
      self->id.flow != FLOW_OUTPUT)
    return;

  /* send midi events */
  if (AUDIO_ENGINE->midi_backend ==
        MIDI_BACKEND_JACK)
    {
      send_midi_events_to_jack (
        self, start_frame, nframes);
    }

  /* send audio data */
  if (AUDIO_ENGINE->audio_backend ==
        AUDIO_BACKEND_JACK)
    {
      send_audio_data_to_jack (
        self, start_frame, nframes);
    }
}

/**
 * Sets whether to expose the port to JACk and
 * exposes it or removes it from JACK.
 */
static void
expose_to_jack (
  Port *   self,
  bool     expose)
{
  enum JackPortFlags flags;
  if (self->id.owner_type == PORT_OWNER_TYPE_HW)
    {
      /* these are reversed */
      if (self->id.flow == FLOW_INPUT)
        flags = JackPortIsOutput;
      else if (self->id.flow == FLOW_OUTPUT)
        flags = JackPortIsInput;
      else
        {
          g_return_if_reached ();
        }
    }
  else
    {
      if (self->id.flow == FLOW_INPUT)
        flags = JackPortIsInput;
      else if (self->id.flow == FLOW_OUTPUT)
        flags = JackPortIsOutput;
      else
        {
          g_return_if_reached ();
        }
    }

  const char * type =
    engine_jack_get_jack_type (self->id.type);
  if (!type)
    g_return_if_reached ();

  char label[600];
  port_get_full_designation (self, label);
  if (expose)
    {
      g_message (
        "exposing port %s to JACK", label);
      if (!self->data)
        {
          self->data =
            (void *) jack_port_register (
              AUDIO_ENGINE->client,
              label, type, flags, 0);
        }
      g_return_if_fail (self->data);
      self->internal_type = INTERNAL_JACK_PORT;
    }
  else
    {
      g_message (
        "unexposing port %s from JACK", label);
      if (AUDIO_ENGINE->client)
        {
          g_warn_if_fail (self->data);
          int ret =
            jack_port_unregister (
              AUDIO_ENGINE->client,
              JACK_PORT_T (self->data));
          if (ret)
            {
              char jack_error[600];
              engine_jack_get_error_message (
                (jack_status_t) ret, jack_error);
              g_warning (
                "JACK port unregister error: %s",
                jack_error);
            }
        }
      self->internal_type = INTERNAL_NONE;
      self->data = NULL;
    }

  self->exposed_to_backend = expose;
}

#if 0
/**
 * Returns the JACK port attached to this port,
 * if any.
 */
static jack_port_t *
get_internal_jack_port (
  Port * port)
{
  if (port->internal_type ==
        INTERNAL_JACK_PORT &&
      port->data)
    return (jack_port_t *) port->data;
  else
    return NULL;
}
#endif
#endif /* HAVE_JACK */

/**
 * Sums the inputs coming in from dummy, before the
 * port is processed.
 */
static void
sum_data_from_dummy (
  Port * self,
  const nframes_t start_frame,
  const nframes_t nframes)
{
  if (self->id.owner_type ==
        PORT_OWNER_TYPE_BACKEND ||
      self->id.flow !=
        FLOW_INPUT ||
      self->id.type != TYPE_AUDIO ||
      AUDIO_ENGINE->audio_backend !=
        AUDIO_BACKEND_DUMMY ||
      AUDIO_ENGINE->midi_backend !=
        MIDI_BACKEND_DUMMY)
    return;

  if (AUDIO_ENGINE->dummy_input)
    {
      Port * port = NULL;
      if (self->id.flags & PORT_FLAG_STEREO_L)
        {
          port = AUDIO_ENGINE->dummy_input->l;
        }
      else if (self->id.flags & PORT_FLAG_STEREO_R)
        {
          port = AUDIO_ENGINE->dummy_input->r;
        }

      if (port)
        {
          dsp_add2 (
            &self->buf[start_frame],
            &port->buf[start_frame], nframes);
        }
    }
}

/**
 * Connects the internal ports using
 * port_connect().
 *
 * @param locked Lock the connection.
 */
void
stereo_ports_connect (
  StereoPorts * src,
  StereoPorts * dest,
  int           locked)
{
  port_connect (
    src->l, dest->l, locked);
  port_connect (
    src->r, dest->r, locked);
}

/**
 * Returns the number of unlocked (user-editable)
 * destinations.
 */
int
port_get_num_unlocked_dests (Port * port)
{
  int res = 0;
  for (int i = 0; i < port->num_dests; i++)
    {
      if (!port->dest_locked[i])
        res++;
    }
  return res;
}

/**
 * Returns the number of unlocked (user-editable)
 * sources.
 */
int
port_get_num_unlocked_srcs (Port * port)
{
  int res = 0;
  for (int i = 0; i < port->num_srcs; i++)
    {
      Port * src = port->srcs[i];
      int idx = port_get_dest_index (src, port);
      if (!src->dest_locked[idx])
        res++;
    }
  return res;
}

/**
 * Gathers all ports in the project and puts them
 * in the given array and size.
 *
 * @param size Current array count.
 * @param is_dynamic Whether the array can be
 *   dynamically resized.
 * @param max_size Current array size, if dynamic.
 */
void
port_get_all (
  Port *** ports,
  size_t * max_size,
  bool     is_dynamic,
  int *    size)
{
  *size = 0;

#define _ADD(port) \
  g_warn_if_fail (port); \
  if (is_dynamic) \
    { \
      array_double_size_if_full ( \
        *ports, (*size), (*max_size), Port *); \
    } \
  else if ((size_t) *size == *max_size) \
    { \
      g_return_if_reached (); \
    } \
  g_warn_if_fail (port); \
  array_append ( \
    *ports, (*size), port)

  /* add fader ports */
  _ADD (MONITOR_FADER->amp);
  _ADD (MONITOR_FADER->balance);
  _ADD (MONITOR_FADER->mute);
  _ADD (MONITOR_FADER->stereo_in->l);
  _ADD (MONITOR_FADER->stereo_in->r);
  _ADD (MONITOR_FADER->stereo_out->l);
  _ADD (MONITOR_FADER->stereo_out->r);

  _ADD (AUDIO_ENGINE->monitor_out->l);
  _ADD (AUDIO_ENGINE->monitor_out->r);
  _ADD (AUDIO_ENGINE->midi_editor_manual_press);
  _ADD (AUDIO_ENGINE->midi_in);
  _ADD (SAMPLE_PROCESSOR->stereo_out->l);
  _ADD (SAMPLE_PROCESSOR->stereo_out->r);

  _ADD (TRANSPORT->roll);
  _ADD (TRANSPORT->stop);
  _ADD (TRANSPORT->backward);
  _ADD (TRANSPORT->forward);
  _ADD (TRANSPORT->loop_toggle);
  _ADD (TRANSPORT->rec_toggle);

  for (int i = 0;
       i < HW_IN_PROCESSOR->num_audio_ports; i++)
    {
      _ADD (HW_IN_PROCESSOR->audio_ports[i]);
    }
  for (int i = 0;
       i < HW_IN_PROCESSOR->num_midi_ports; i++)
    {
      _ADD (HW_IN_PROCESSOR->midi_ports[i]);
    }

  for (int i = 0;
       i < HW_OUT_PROCESSOR->num_audio_ports; i++)
    {
      _ADD (HW_OUT_PROCESSOR->audio_ports[i]);
    }
  for (int i = 0;
       i < HW_OUT_PROCESSOR->num_midi_ports; i++)
    {
      _ADD (HW_OUT_PROCESSOR->midi_ports[i]);
    }

  for (int i = 0; i < TRACKLIST->num_tracks; i++)
    {
      Track * tr = TRACKLIST->tracks[i];
      track_append_all_ports (
        tr, ports, size, is_dynamic, max_size,
        F_INCLUDE_PLUGINS);
    }

#undef _ADD
}

#if 0
/**
 * Creates port and adds given data to it.
 */
Port *
port_new_with_data (
  PortInternalType internal_type,
  PortType     type,
  PortFlow     flow,
  const char * label,
  void *       data)
{
  Port * port =
    port_new_with_type (type, flow, label);

  /** TODO semaphore **/
  port->data = data;
  port->internal_type = internal_type;

  /** TODO end semaphore */

  return port;
}
#endif

/**
 * Sets the owner plugin & its slot.
 */
void
port_set_owner_plugin (
  Port *   port,
  Plugin * pl)
{
  plugin_identifier_copy (
    &port->id.plugin_id, &pl->id);
  port->id.track_pos = pl->id.track_pos;
  port->id.owner_type =
    PORT_OWNER_TYPE_PLUGIN;

  if (port->at)
    {
      port_identifier_copy (
        &port->at->port_id, &port->id);
    }
}

/**
 * Sets the owner sample processor.
 */
void
port_set_owner_sample_processor (
  Port *   port,
  SampleProcessor * sample_processor)
{
  port->sample_processor = sample_processor;
  port->id.owner_type =
    PORT_OWNER_TYPE_SAMPLE_PROCESSOR;
}

/**
 * Sets the owner track & its ID.
 */
void
port_set_owner_track (
  Port *    port,
  Track *   track)
{
  port->id.track_pos = track->pos;
  port->id.owner_type =
    PORT_OWNER_TYPE_TRACK;
}

/**
 * Sets the owner track & its ID.
 */
void
port_set_owner_track_from_channel (
  Port *    port,
  Channel * ch)
{
  port->id.track_pos = ch->track_pos;
  port->id.owner_type = PORT_OWNER_TYPE_TRACK;
}

/**
 * Sets the owner track & its ID.
 */
void
port_set_owner_track_processor (
  Port *           port,
  TrackProcessor * track_processor)
{
  port->id.track_pos = track_processor->track_pos;
  port->id.owner_type =
    PORT_OWNER_TYPE_TRACK_PROCESSOR;
}

/**
 * Sets the owner fader & its ID.
 */
void
port_set_owner_fader (
  Port *    port,
  Fader *   fader)
{
  PortIdentifier * id = &port->id;

  if (fader->type == FADER_TYPE_AUDIO_CHANNEL ||
      fader->type == FADER_TYPE_MIDI_CHANNEL)
    {
      id->track_pos = fader->track_pos;
      if (fader->passthrough)
        {
          id->owner_type = PORT_OWNER_TYPE_PREFADER;
        }
      else
        {
          id->owner_type = PORT_OWNER_TYPE_FADER;
        }
    }
  else
    {
      id->owner_type =
        PORT_OWNER_TYPE_MONITOR_FADER;
    }

  if (id->flags & PORT_FLAG_AMPLITUDE)
    {
      port->minf = 0.f;
      port->maxf = 2.f;
      port->zerof = 0.f;
    }
  else if (id->flags & PORT_FLAG_STEREO_BALANCE)
    {
      port->minf = 0.f;
      port->maxf = 1.f;
      port->zerof = 0.5f;
    }
}

/**
 * Sets the channel send as the port's owner.
 */
void
port_set_owner_channel_send (
  Port *        port,
  ChannelSend * send)
{
  PortIdentifier * id = &port->id;

  id->track_pos = send->track_pos;
  id->port_index = send->slot;
  id->owner_type = PORT_OWNER_TYPE_CHANNEL_SEND;

  if (id->flags2 & PORT_FLAG2_CHANNEL_SEND_ENABLED)
    {
      port->minf = 0.f;
      port->maxf = 1.f;
      port->zerof = 0.0f;
    }
  else if (id->flags2 &
             PORT_FLAG2_CHANNEL_SEND_AMOUNT)
    {
      port->minf = 0.f;
      port->maxf = 2.f;
      port->zerof = 0.f;
    }
  else
    {
      g_return_if_reached ();
    }
}

#if 0
/**
 * Sets the owner fader & its ID.
 */
void
port_set_owner_prefader (
  Port *                 port,
  PassthroughProcessor * fader)
{
  g_warn_if_fail (port && fader);

  port->id.track_pos = fader->track_pos;
  port->id.owner_type =
    PORT_OWNER_TYPE_PREFADER;
}
#endif

/**
 * Returns whether the Port's can be connected (if
 * the connection will be valid and won't break the
 * acyclicity of the graph).
 */
bool
ports_can_be_connected (
  const Port * src,
  const Port *dest)
{
  Graph * graph = graph_new (ROUTER);
  bool valid =
    graph_validate_with_connection (
      graph, src, dest);
  graph_free (graph);

  return valid;
}

/**
 * Disconnects all the given ports.
 */
void
ports_disconnect (
  Port ** ports,
  int     num_ports,
  int     deleting)
{
  int i, j;
  Port * port;

  /* go through each port */
  for (i = 0; i < num_ports; i++)
    {
      port = ports[i];
      g_message ("Attempting to disconnect %s ("
                 "current srcs %d)",
                 port->id.label,
                 port->num_srcs);
      port->deleting = deleting;

      /* disconnect incoming ports */
      for (j = port->num_srcs - 1; j >= 0; j--)
        {
          port_disconnect (port->srcs[j], port);
        }
      /* disconnect outgoing ports */
      for (j = port->num_dests - 1; j >= 0; j--)
        {
          port_disconnect (
            port, port->dests[j]);
        }
      g_message ("%s num srcs %d dests %d",
                 port->id.label,
                 port->num_srcs,
                 port->num_dests);
    }
}

void
port_set_is_project (
  Port * self,
  bool   is_project)
{
  g_return_if_fail (IS_PORT (self));

  self->is_project = is_project;
}

/**
 * Connets src to dest.
 *
 * @param locked Lock the connection or not.
 *
 * @return Non-zero if error.
 */
int
port_connect (
  Port * src,
  Port * dest,
  const int locked)
{
  g_return_val_if_fail (
    IS_PORT (src) && IS_PORT (dest) &&
    src != dest, -1);

  if (ports_connected (src, dest))
    {
      port_disconnect (src, dest);
    }

  if ((src->id.type !=
       dest->id.type) &&
      !(src->id.type == TYPE_CV &&
        dest->id.type == TYPE_CONTROL))
    {
      g_warning (
        "Cannot connect ports, incompatible types");
      return -1;
    }

  g_return_val_if_fail (
    (int) src->dests_size >= src->num_dests, -1);
  g_return_val_if_fail (
    (int) dest->srcs_size >= dest->num_srcs, -1);

#if 0
  g_debug (
    "before: dest srcs size %zu, num srcs %d",
    dest->srcs_size, dest->num_srcs);
#endif

  /* resize src arrays */
  if (src->num_dests == (int) src->dests_size)
    {
      realloc_dests (
        src, src->dests_size,
        (size_t) src->num_dests + 1);
    }

  /* resize dest arrays */
  if (dest->num_srcs == (int) dest->srcs_size)
    {
      realloc_srcs (
        dest, dest->srcs_size,
        (size_t) dest->num_srcs + 1);
    }

#if 0
  g_debug (
    "after realloc: dest srcs size %zu, num srcs %d",
    dest->srcs_size, dest->num_srcs);
#endif

  src->dests[src->num_dests] = dest;
  dest->srcs[dest->num_srcs] = src;
  port_identifier_copy (
    &src->dest_ids[src->num_dests],
    &dest->id);
  port_identifier_copy (
    &dest->src_ids[dest->num_srcs],
    &src->id);
  src->multipliers[src->num_dests] = 1.f;
  dest->src_multipliers[dest->num_srcs] = 1.f;
  src->dest_locked[src->num_dests] = locked;
  dest->src_locked[dest->num_srcs] = locked;
  src->dest_enabled[src->num_dests] = 1;
  dest->src_enabled[dest->num_srcs] = 1;
  src->num_dests++;
  dest->num_srcs++;

  /* set base value if cv -> control */
  if (src->id.type == TYPE_CV &&
      dest->id.type == TYPE_CONTROL)
    {
      dest->base_value = dest->control;
    }

#if 0
  g_debug ("after: dest srcs size %zu, num srcs %d",
    dest->srcs_size, dest->num_srcs);
#endif

  g_return_val_if_fail (
    (int) src->dests_size >= src->num_dests, -1);
  g_return_val_if_fail (
    (int) dest->srcs_size >= dest->num_srcs, -1);

  port_verify_src_and_dests (src);
  port_verify_src_and_dests (dest);

#if 0
  char sd[600], dd[600];
  port_get_full_designation (src, sd);
  port_get_full_designation (dest, dd);
  g_message (
    "Connected port \"%s\" to \"%s\"", sd, dd);
#endif
  g_message (
    "connected port <%s> to <%s> | "
    "dests for <%s> (%p): %d | "
    "sources for <%s> (%p): %d",
    src->id.label, dest->id.label,
    src->id.label, src, src->num_dests,
    dest->id.label, dest, dest->num_srcs);
  return 0;
}

/**
 * Disconnects src from dest.
 */
int
port_disconnect (Port * src, Port * dest)
{
  g_warn_if_fail (IS_PORT (src) && IS_PORT (dest));

  int pos = -1;
  /* disconnect dest from src */
  array_delete_return_pos (
    src->dests, src->num_dests, dest, pos);
  if (pos >= 0)
    {
      for (int i = pos;
           i < src->num_dests;
           i++)
        {
          port_identifier_copy (
            &src->dest_ids[i],
            &src->dest_ids[i + 1]);
          src->multipliers[i] =
            src->multipliers[i + 1];
          src->dest_locked[i] =
            src->dest_locked[i + 1];
          src->dest_enabled[i] =
            src->dest_enabled[i + 1];
        }
    }

  /* disconnect src from dest */
  pos = -1;
  array_delete_return_pos (
    dest->srcs, dest->num_srcs, src, pos);
  if (pos >= 0)
    {
      for (int i = pos;
           i < dest->num_srcs;
           i++)
        {
          port_identifier_copy (
            &dest->src_ids[i],
            &dest->src_ids[i + 1]);
          dest->src_multipliers[i] =
            dest->src_multipliers[i + 1];
          dest->src_locked[i] =
            dest->src_locked[i + 1];
          dest->src_enabled[i] =
            dest->src_enabled[i + 1];
        }
    }

#if 0
  char sd[600], dd[600];
  port_get_full_designation (src, sd);
  port_get_full_designation (dest, dd);
  g_message (
    "Disconnected port \"%s\" from \"%s\"",
    sd, dd);
#endif
  g_message (
    "disconnected port <%s> from <%s> | "
    "dests for <%s> (%p): %d | "
    "sources for <%s> (%p): %d",
    src->id.label, dest->id.label,
    src->id.label, src, src->num_dests,
    dest->id.label, dest, dest->num_srcs);
  return 0;
}

/**
 * Returns if the two ports are connected or not.
 */
bool
ports_connected (Port * src, Port * dest)
{
  g_return_val_if_fail (
    IS_PORT (src) && IS_PORT (dest), false);

  return
    array_contains (
      src->dests, src->num_dests, dest);
}

/**
 * Disconnects all srcs and dests from port.
 */
int
port_disconnect_all (
  Port * self)
{
  g_return_val_if_fail (
    IS_PORT (self), ERR_PORT_MAGIC_FAILED);

  if (!self->is_project)
    {
#if 0
      g_debug (
        "%s (%p) is not a project port, "
        "skipping",
        self->id.label, self);
#endif
      self->num_srcs = 0;
      self->num_dests = 0;
      return 0;
    }

  for (int i = self->num_srcs - 1; i >= 0; i--)
    {
      Port * src = self->srcs[i];
      port_disconnect (src, self);
    }

  for (int i = self->num_dests - 1; i >= 0; i--)
    {
      Port * dest = self->dests[i];
      port_disconnect (self, dest);
    }

#ifdef HAVE_JACK
  if (self->internal_type == INTERNAL_JACK_PORT)
    {
      expose_to_jack (self, 0);
    }
#endif

#ifdef HAVE_RTMIDI
  for (int i = self->num_rtmidi_ins - 1; i >= 0;
       i--)
    {
      rtmidi_device_close (
        self->rtmidi_ins[i], 1);
      self->num_rtmidi_ins--;
    }
#endif

  return 0;
}

/**
 * Verifies that the srcs and dests are correct
 * for project ports.
 */
void
port_verify_src_and_dests (
  Port * self)
{
  g_return_if_fail (
    self->num_srcs <= (int) self->srcs_size);
  g_return_if_fail (
    self->num_dests <= (int) self->dests_size);

  if (self->is_project)
    {
      /* verify all sources */
      for (int i = 0; i < self->num_srcs; i++)
        {
          Port * src = self->srcs[i];
          g_return_if_fail (
            IS_PORT (src) && src->is_project);
          int dest_idx =
            port_get_dest_index (src, self);
          g_warn_if_fail (
            src->dests[dest_idx] == self &&
            port_identifier_is_equal (
              &src->dest_ids[dest_idx],
              &self->id) &&
            port_identifier_is_equal (
              &src->id, &self->src_ids[i]));
          g_warn_if_fail (
            src->dest_enabled[dest_idx] ==
              self->src_enabled[i]);
          g_warn_if_fail (
            src->dest_enabled[dest_idx] >= 0 &&
            src->dest_enabled[dest_idx] <= 1);
        }

      /* verify all dests */
      for (int i = 0; i < self->num_dests; i++)
        {
          Port * dest = self->dests[i];
          g_return_if_fail (
            IS_PORT (dest) && dest->is_project);
          int src_idx =
            port_get_src_index (dest, self);
          g_warn_if_fail (
            dest->srcs[src_idx] == self &&
            port_identifier_is_equal (
              &dest->src_ids[src_idx],
              &self->id) &&
            port_identifier_is_equal (
              &dest->id, &self->dest_ids[i]));
          g_warn_if_fail (
            self->dest_enabled[i] ==
              dest->src_enabled[src_idx]);
          g_warn_if_fail (
            dest->src_enabled[src_idx] >= 0 &&
            dest->src_enabled[src_idx] <= 1);
        }
    }
}

/**
 * To be called when the port's identifier changes
 * to update corresponding identifiers.
 *
 * @param track The track that owns this port.
 * @param update_automation_track Whether to update
 *   the identifier in the corresponding automation
 *   track as well. This should be false when
 *   moving a plugin.
 */
void
port_update_identifier (
  Port *  self,
  Track * track,
  bool    update_automation_track)
{
  /*g_message (*/
    /*"updating identifier for %p %s (track pos %d)", */
    /*self, self->id.label, self->id.track_pos);*/

  if (self->is_project)
    {
      /* update in all sources */
      for (int i = 0; i < self->num_srcs; i++)
        {
          Port * src = self->srcs[i];
          int dest_idx =
            port_get_dest_index (src, self);
          port_identifier_copy (
            &src->dest_ids[dest_idx], &self->id);
          g_warn_if_fail (
            src->dests[dest_idx] == self);
        }

      /* update in all dests */
      for (int i = 0; i < self->num_dests; i++)
        {
          Port * dest = self->dests[i];
          int src_idx =
            port_get_src_index (dest, self);
          port_identifier_copy (
            &dest->src_ids[src_idx], &self->id);
          g_warn_if_fail (
            dest->srcs[src_idx] == self);
        }

      if (update_automation_track &&
          self->id.track_pos > -1 &&
          self->id.flags & PORT_FLAG_AUTOMATABLE)
        {
          /* update automation track's port id */
          self->at =
            automation_track_find_from_port (
              self, track, true);
          AutomationTrack * at = self->at;
          g_return_if_fail (at);
          port_identifier_copy (
            &at->port_id, &self->id);
        }
    }

#if 0
  if (self->lv2_port)
    {
      Lv2Port * port = self->lv2_port;
      port_identifier_copy (
        &port->port_id, &self->id);
    }
#endif
}

/**
 * Updates the track pos on a track port and
 * all its source/destination identifiers.
 *
 * @param track The track that owns this port.
 */
void
port_update_track_pos (
  Port *  self,
  Track * track,
  int     pos)
{
  if (self->id.flags & PORT_FLAG_SEND_RECEIVABLE)
    {
      /* this could be a send receiver, so update
       * the sending channel if matching */
      for (int i = 0; i < self->num_srcs; i++)
        {
          Port * src = self->srcs[i];

          if (src->id.owner_type ==
                 PORT_OWNER_TYPE_PREFADER ||
              src->id.owner_type ==
                PORT_OWNER_TYPE_FADER)
            {
              Track * src_track =
                port_get_track (src, true);
              Channel * src_ch =
                track_get_channel (src_track);
              for (int j = 0; j < STRIP_SIZE; j++)
                {
                  ChannelSend * send =
                    src_ch->sends[j];
                  if (channel_send_is_empty (send))
                    continue;

                  switch (src_track->out_signal_type)
                    {
                    case TYPE_EVENT:
                      if (port_identifier_is_equal (
                            &send->dest_midi_id,
                            &self->id))
                        {
                          send->dest_midi_id.track_pos =
                            pos;
                          g_message ("updating midi send");
                        }
                      break;
                    case TYPE_AUDIO:
                      if (port_identifier_is_equal (
                            &send->dest_l_id,
                            &self->id))
                        {
                          send->dest_l_id.track_pos =
                            pos;
                          g_message ("updating audio L send");
                        }
                      else if (port_identifier_is_equal (
                            &send->dest_r_id,
                            &self->id))
                        {
                          send->dest_r_id.track_pos =
                            pos;
                          g_message ("updating audio R send");
                        }
                      break;
                    default:
                      break;
                    }
                }
            }
        }
    }

  self->id.track_pos = pos;
  if (self->id.owner_type == PORT_OWNER_TYPE_PLUGIN)
    {
      self->id.plugin_id.track_pos = pos;
    }
  port_update_identifier (
    self, track, F_UPDATE_AUTOMATION_TRACK);
}

#ifdef HAVE_ALSA
#if 0

/**
 * Returns the Port matching the given ALSA
 * sequencer port ID.
 */
Port *
port_find_by_alsa_seq_id (
  const int id)
{
  Port * ports[10000];
  int num_ports;
  Port * port;
  port_get_all (ports, &num_ports);
  for (int i = 0; i < num_ports; i++)
    {
      port = ports[i];

      if (port->internal_type ==
            INTERNAL_ALSA_SEQ_PORT &&
          snd_seq_port_info_get_port (
            (snd_seq_port_info_t *) port->data) ==
            id)
        return port;
    }
  g_return_val_if_reached (NULL);
}

static void
expose_to_alsa (
  Port * self,
  int    expose)
{
  if (self->id.type == TYPE_EVENT)
    {
      unsigned int flags = 0;
      if (self->id.flow == FLOW_INPUT)
        flags =
          SND_SEQ_PORT_CAP_WRITE |
          SND_SEQ_PORT_CAP_SUBS_WRITE;
      else if (self->id.flow == FLOW_OUTPUT)
        flags =
          SND_SEQ_PORT_CAP_READ |
          SND_SEQ_PORT_CAP_SUBS_READ;
      else
        g_return_if_reached ();

      if (expose)
        {
          char lbl[600];
          port_get_full_designation (self, lbl);

          g_return_if_fail (
            AUDIO_ENGINE->seq_handle);

          int id =
            snd_seq_create_simple_port (
              AUDIO_ENGINE->seq_handle,
              lbl, flags,
              SND_SEQ_PORT_TYPE_APPLICATION);
          g_return_if_fail (id >= 0);
          snd_seq_port_info_t * info;
          snd_seq_port_info_malloc (&info);
          snd_seq_get_port_info (
            AUDIO_ENGINE->seq_handle,
            id, info);
          self->data = (void *) info;
          self->internal_type =
            INTERNAL_ALSA_SEQ_PORT;
        }
      else
        {
          snd_seq_delete_port (
            AUDIO_ENGINE->seq_handle,
            snd_seq_port_info_get_port (
              (snd_seq_port_info_t *)
                self->data));
          self->internal_type = INTERNAL_NONE;
          self->data = NULL;
        }
    }
  self->exposed_to_backend = expose;
}
#endif
#endif // HAVE_ALSA

#ifdef HAVE_RTMIDI
static void
expose_to_rtmidi (
  Port * self,
  int    expose)
{
  char lbl[600];
  port_get_full_designation (self, lbl);
  if (expose)
    {
#if 0

      if (self->id.flow == FLOW_INPUT)
        {
          self->rtmidi_in =
            rtmidi_in_create (
#ifdef _WOE32
              RTMIDI_API_WINDOWS_MM,
#elif defined(__APPLE__)
              RTMIDI_API_MACOSX_CORE,
#else
              RTMIDI_API_LINUX_ALSA,
#endif
              "Zrythm",
              AUDIO_ENGINE->midi_buf_size);

          /* don't ignore any messages */
          rtmidi_in_ignore_types (
            self->rtmidi_in, 0, 0, 0);

          rtmidi_open_port (
            self->rtmidi_in, 1, lbl);
        }
#endif
      g_message ("exposing %s", lbl);
    }
  else
    {
#if 0
      if (self->id.flow == FLOW_INPUT &&
          self->rtmidi_in)
        {
          rtmidi_close_port (self->rtmidi_in);
          self->rtmidi_in = NULL;
        }
#endif
      g_message ("unexposing %s", lbl);
    }
  self->exposed_to_backend = expose;
}

/**
 * Sums the inputs coming in from RtMidi
 * before the port is processed.
 */
void
port_sum_data_from_rtmidi (
  Port * self,
  const nframes_t start_frame,
  const nframes_t nframes)
{
  g_return_if_fail (
    /*self->id.flow == FLOW_INPUT &&*/
    midi_backend_is_rtmidi (
      AUDIO_ENGINE->midi_backend));

  for (int i = 0; i < self->num_rtmidi_ins; i++)
    {
      RtMidiDevice * dev = self->rtmidi_ins[i];
      MidiEvent * ev;
      for (int j = 0; j < dev->events->num_events;
           j++)
        {
          ev = &dev->events->events[j];

          if (ev->time >= start_frame &&
              ev->time < start_frame + nframes)
            {
              midi_byte_t channel =
                ev->raw_buffer[0] & 0xf;
              Track * track =
                port_get_track (self, 0);
              if (self->id.owner_type ==
                    PORT_OWNER_TYPE_TRACK_PROCESSOR &&
                  !track)
                {
                  g_return_if_reached ();
                }

              if (self->id.owner_type ==
                    PORT_OWNER_TYPE_TRACK_PROCESSOR &&
                  (track->type ==
                       TRACK_TYPE_MIDI ||
                     track->type ==
                       TRACK_TYPE_INSTRUMENT) &&
                    !track->channel->
                      all_midi_channels &&
                    !track->channel->
                      midi_channels[channel])
                {
                  /* different channel */
                }
              else
                {
                  midi_events_add_event_from_buf (
                    self->midi_events,
                    ev->time, ev->raw_buffer,
                    3, F_NOT_QUEUED);
                }
            }
        }
    }

  if (DEBUGGING &&
      self->midi_events->num_events > 0)
    {
      MidiEvent * ev =
        &self->midi_events->events[0];
      char designation[600];
      port_get_full_designation (
        self, designation);
      g_message (
        "RtMidi (%s): have %d events\n"
        "first event is: [%u] %hhx %hhx %hhx",
        designation, self->midi_events->num_events,
        ev->time, ev->raw_buffer[0],
        ev->raw_buffer[1], ev->raw_buffer[2]);
    }
}

/**
 * Dequeue the midi events from the ring
 * buffers into \ref RtMidiDevice.events.
 */
void
port_prepare_rtmidi_events (
  Port * self)
{
  g_return_if_fail (
    /*self->id.flow == FLOW_INPUT &&*/
    midi_backend_is_rtmidi (
      AUDIO_ENGINE->midi_backend));

  gint64 cur_time = g_get_monotonic_time ();
  for (int i = 0; i < self->num_rtmidi_ins; i++)
    {
      RtMidiDevice * dev = self->rtmidi_ins[i];

      /* clear the events */
      midi_events_clear (dev->events, 0);

      uint32_t read_space = 0;
      zix_sem_wait (&dev->midi_ring_sem);
      do
        {
          read_space =
            zix_ring_read_space (dev->midi_ring);
          if (read_space <= sizeof(MidiEventHeader))
            {
              /* no more events */
              break;
            }

          /* peek the next event header to check
           * the time */
          MidiEventHeader h = { 0, 0 };
          zix_ring_peek (
            dev->midi_ring, &h, sizeof (h));
          g_return_if_fail (h.size > 0);

          /* read event header */
          zix_ring_read (
            dev->midi_ring, &h, sizeof (h));

          /* read event body */
          midi_byte_t raw[h.size];
          zix_ring_read (
            dev->midi_ring, raw, sizeof (raw));

          /* calculate event timestamp */
          gint64 length =
            cur_time - self->last_midi_dequeue;
          midi_time_t ev_time =
            (midi_time_t)
            (((double) h.time / (double) length) *
            (double) AUDIO_ENGINE->block_length);

          if (ev_time >= AUDIO_ENGINE->block_length)
            {
              g_warning (
                "event with invalid time %u "
                "received. the maximum allowed time "
                "is %" PRIu32 ". setting it to "
                "%u...",
                ev_time,
                AUDIO_ENGINE->block_length - 1,
                AUDIO_ENGINE->block_length - 1);
              ev_time =
                (midi_byte_t)
                (AUDIO_ENGINE->block_length - 1);
            }

          midi_events_add_event_from_buf (
            dev->events,
            ev_time, raw, (int) h.size,
            F_NOT_QUEUED);
        } while (
            read_space > sizeof (MidiEventHeader));
      zix_sem_post (&dev->midi_ring_sem);
    }
  self->last_midi_dequeue = cur_time;
}
#endif // HAVE_RTMIDI

#ifdef HAVE_RTAUDIO
static void
expose_to_rtaudio (
  Port * self,
  int    expose)
{
  Track * track = port_get_track (self, 0);
  if (!track)
    return;

  Channel * ch = track->channel;
  if (!ch)
    return;

  char lbl[600];
  port_get_full_designation (self, lbl);
  if (expose)
    {
      if (self->id.flow == FLOW_INPUT)
        {
          if (self->id.flags & PORT_FLAG_STEREO_L)
            {
              if (ch->all_stereo_l_ins)
                {
                }
              else
                {
                  for (int i = 0;
                       i < ch->num_ext_stereo_l_ins; i++)
                    {
                      int idx =
                        self->num_rtaudio_ins;
                      ExtPort * ext_port =
                        ch->ext_stereo_l_ins[i];
                      ext_port_print (ext_port);
                      self->rtaudio_ins[idx] =
                        rtaudio_device_new (
                          ext_port->rtaudio_is_input,
                          NULL,
                          ext_port->rtaudio_id,
                          ext_port->rtaudio_channel_idx,
                          self);
                      rtaudio_device_open (
                        self->rtaudio_ins[idx], true);
                      self->num_rtaudio_ins++;
                    }
                }
            }
          else if (self->id.flags & PORT_FLAG_STEREO_R)
            {
              if (ch->all_stereo_r_ins)
                {
                }
              else
                {
                  for (int i = 0;
                       i < ch->num_ext_stereo_r_ins; i++)
                    {
                      int idx =
                        self->num_rtaudio_ins;
                      ExtPort * ext_port =
                        ch->ext_stereo_r_ins[i];
                      ext_port_print (ext_port);
                      self->rtaudio_ins[idx] =
                        rtaudio_device_new (
                          ext_port->rtaudio_is_input,
                          NULL,
                          ext_port->rtaudio_id,
                          ext_port->rtaudio_channel_idx,
                          self);
                      rtaudio_device_open (
                        self->rtaudio_ins[idx], true);
                      self->num_rtaudio_ins++;
                    }
                }
            }
        }
      g_message ("exposing %s", lbl);
    }
  else
    {
      if (self->id.flow == FLOW_INPUT)
        {
          for (int i = 0; i < self->num_rtaudio_ins;
               i++)
            {
              rtaudio_device_close (
                self->rtaudio_ins[i], true);
            }
          self->num_rtaudio_ins = 0;
        }
      g_message ("unexposing %s", lbl);
    }
  self->exposed_to_backend = expose;
}

/**
 * Dequeue the audio data from the ring buffer
 * into @ref RtAudioDevice.buf.
 */
void
port_prepare_rtaudio_data (
  Port * self)
{
  g_return_if_fail (
    /*self->id.flow == FLOW_INPUT &&*/
    audio_backend_is_rtaudio (
      AUDIO_ENGINE->audio_backend));

  for (int i = 0; i < self->num_rtaudio_ins; i++)
    {
      RtAudioDevice * dev = self->rtaudio_ins[i];

      /* clear the data */
      dsp_fill (
        dev->buf, 0, AUDIO_ENGINE->nframes);

      zix_sem_wait (&dev->audio_ring_sem);

      uint32_t read_space =
        zix_ring_read_space (dev->audio_ring);

      /* only process if data exists */
      if (read_space >=
            AUDIO_ENGINE->nframes *
            sizeof (float))
        {
          /* read the buffer */
          zix_ring_read (
            dev->audio_ring,
            &dev->buf[0],
            AUDIO_ENGINE->nframes *
              sizeof (float));
        }

      zix_sem_post (&dev->audio_ring_sem);
    }
}

/**
 * Sums the inputs coming in from RtAudio
 * before the port is processed.
 */
void
port_sum_data_from_rtaudio (
  Port * self,
  const nframes_t start_frame,
  const nframes_t nframes)
{
  g_return_if_fail (
    /*self->id.flow == FLOW_INPUT &&*/
    audio_backend_is_rtaudio (
      AUDIO_ENGINE->audio_backend));

  for (int i = 0; i < self->num_rtaudio_ins; i++)
    {
      RtAudioDevice * dev = self->rtaudio_ins[i];

      dsp_add2 (
        &self->buf[start_frame],
        &dev->buf[start_frame], nframes);
    }
}
#endif // HAVE_RTAUDIO

#ifdef _WOE32
/**
 * Sums the inputs coming in from Windows MME,
 * before the port is processed.
 */
static void
sum_data_from_windows_mme (
  Port * self,
  const nframes_t start_frame,
  const nframes_t nframes)
{
  g_return_if_fail (
    self->id.flow == FLOW_INPUT &&
    AUDIO_ENGINE->midi_backend ==
      MIDI_BACKEND_WINDOWS_MME);

  if (self->id.owner_type ==
        PORT_OWNER_TYPE_BACKEND)
    return;

  /* append events from Windows MME if any */
  for (int i = 0; i < self->num_mme_connections;
       i++)
    {
      WindowsMmeDevice * dev =
        self->mme_connections[i];
      if (!dev)
        {
          g_warn_if_reached ();
          continue;
        }

      MidiEvent ev;
      gint64 cur_time = g_get_monotonic_time ();
      while (
        windows_mme_device_dequeue_midi_event_struct (
          dev, self->last_midi_dequeue,
          cur_time, &ev))
        {
          int is_valid =
            ev.time >= start_frame &&
            ev.time < start_frame + nframes;
          if (!is_valid)
            {
              g_warning (
                "Invalid event time %u", ev.time);
              continue;
            }

          if (ev.time >= start_frame &&
              ev.time < start_frame + nframes)
            {
              midi_byte_t channel =
                ev.raw_buffer[0] & 0xf;
              Track * track =
                port_get_track (self, 0);
              if (self->id.owner_type ==
                    PORT_OWNER_TYPE_TRACK_PROCESSOR &&
                  (track->type ==
                     TRACK_TYPE_MIDI ||
                   track->type ==
                     TRACK_TYPE_INSTRUMENT) &&
                  !track->channel->
                    all_midi_channels &&
                  !track->channel->
                    midi_channels[channel])
                {
                  /* different channel */
                }
              else
                {
                  midi_events_add_event_from_buf (
                    self->midi_events,
                    ev.time, ev.raw_buffer, 3, 0);
                }
            }
        }
      self->last_midi_dequeue = cur_time;

      if (self->midi_events->num_events > 0)
        {
          MidiEvent * ev =
            &self->midi_events->events[0];
          char designation[600];
          port_get_full_designation (
            self, designation);
          g_message (
            "MME MIDI (%s): have %d events\n"
            "first event is: [%u] %hhx %hhx %hhx",
            designation,
            self->midi_events->num_events,
            ev->time, ev->raw_buffer[0],
            ev->raw_buffer[1], ev->raw_buffer[2]);
        }
    }
}

/**
 * Sends the port data to Windows MME, after the
 * port is processed.
 */
static void
send_data_to_windows_mme (
  Port * self,
  const nframes_t start_frame,
  const nframes_t nframes)
{
  g_return_if_fail (
    self->id.flow == FLOW_OUTPUT &&
    AUDIO_ENGINE->midi_backend ==
      MIDI_BACKEND_WINDOWS_MME);

  /* TODO send midi events */
}
#endif

/**
 * To be called when a control's value changes
 * so that a message can be sent to the UI.
 */
static void
port_forward_control_change_event (
  Port * self)
{
  /* if lv2 port/parameter */
  if (self->value_type > 0)
    {
      Plugin * pl = port_get_plugin (self, 1);
      g_return_if_fail (
        IS_PLUGIN_AND_NONNULL (pl) && pl->lv2);
      lv2_ui_send_control_val_event_from_plugin_to_ui (
        pl->lv2, self);
    }
  else if (
    self->id.owner_type ==
      PORT_OWNER_TYPE_PLUGIN)
    {
      Plugin * pl = port_get_plugin (self, 1);
      if (pl)
        {
#ifdef HAVE_CARLA
          if (pl->setting->open_with_carla &&
              self->carla_param_id >= 0)
            {
              g_return_if_fail (pl->carla);
              carla_native_plugin_set_param_value (
                pl->carla,
                (uint32_t) self->carla_param_id,
                self->control);
            }
#endif
          EVENTS_PUSH (
            ET_PLUGIN_STATE_CHANGED, pl);
        }
    }
  else if (self->id.owner_type ==
             PORT_OWNER_TYPE_FADER &&
           self->id.flags &
             PORT_FLAG_AMPLITUDE)
    {
      Track * track = port_get_track (self, 1);
      g_return_if_fail (
        track && track->channel);
      if (ZRYTHM_HAVE_UI)
        g_return_if_fail (
          track->channel->widget);
      fader_update_volume_and_fader_val (
        track->channel->fader);
      EVENTS_PUSH (
        ET_CHANNEL_FADER_VAL_CHANGED,
        track->channel);
    }
  else if (self->id.owner_type ==
             PORT_OWNER_TYPE_TRACK)
    {
      Track * track = port_get_track (self, 1);
      EVENTS_PUSH (
        ET_TRACK_STATE_CHANGED, track);
    }
}

/**
 * Sets the given control value to the
 * corresponding underlying structure in the Port.
 *
 * Note: this is only for setting the base values
 * (eg when automating via an automation lane). For
 * CV automations this should not be used.
 *
 * @param is_normalized Whether the given value is
 *   normalized between 0 and 1.
 * @param forward_event Whether to forward a port
 *   control change event to the plugin UI. Only
 *   applicable for plugin control ports.
 *   If the control is being changed manually or
 *   from within Zrythm, this should be true to
 *   notify the plugin of the change.
 */
void
port_set_control_value (
  Port *      self,
  const float val,
  const bool  is_normalized,
  const bool  forward_event)
{
  PortIdentifier * id = &self->id;

  /* set the base value */
  if (is_normalized)
    {
      float minf = self->minf;
      float maxf = self->maxf;
      self->base_value =
        minf + val * (maxf - minf);
    }
  else
    {
      self->base_value = val;
    }

  self->unsnapped_control = self->base_value;
  self->base_value =
    control_port_get_snapped_val_from_val (
      self, self->unsnapped_control);

  if (!math_floats_equal (
        self->control, self->base_value))
    {
      self->control = self->base_value;

      /* remember time */
      self->last_change = g_get_monotonic_time ();
      self->value_changed_from_reading = false;

      /* if bpm, update engine */
      if (id->flags & PORT_FLAG_BPM)
        {
          int beats_per_bar =
            tempo_track_get_beats_per_bar (
              P_TEMPO_TRACK);
          engine_update_frames_per_tick (
            AUDIO_ENGINE, beats_per_bar,
            self->control,
            AUDIO_ENGINE->sample_rate, false);
          EVENTS_PUSH (ET_BPM_CHANGED, NULL);
        }

      /* if time sig value, update transport
       * caches */
      if (id->flags2 & PORT_FLAG2_BEATS_PER_BAR ||
          id->flags2 & PORT_FLAG2_BEAT_UNIT)
        {
          int beats_per_bar =
            tempo_track_get_beats_per_bar (
              P_TEMPO_TRACK);
          int beat_unit =
            tempo_track_get_beat_unit (
              P_TEMPO_TRACK);
          bpm_t bpm =
            tempo_track_get_current_bpm (
              P_TEMPO_TRACK);
          transport_update_caches (
            TRANSPORT, beats_per_bar, beat_unit);
          engine_update_frames_per_tick (
            AUDIO_ENGINE, beats_per_bar, bpm,
            AUDIO_ENGINE->sample_rate, false);
          EVENTS_PUSH (
            ET_TIME_SIGNATURE_CHANGED, NULL);
        }

      /* if plugin enabled port, also set
       * plugin's own enabled port value and
       * vice versa */
      if (self->is_project &&
          self->id.flags & PORT_FLAG_PLUGIN_ENABLED)
        {
          Plugin * pl = port_get_plugin (self, 1);
          g_return_if_fail (pl);

          if (self->id.flags &
                PORT_FLAG_GENERIC_PLUGIN_PORT)
            {
              if (
                pl->own_enabled_port &&
                !math_floats_equal (
                  pl->own_enabled_port->
                    control,
                  self->control))
                {
                  g_debug (
                    "generic enabled "
                    "changed - changing "
                    "plugin's own enabled");
                  port_set_control_value (
                    pl->own_enabled_port,
                    self->control, false,
                    F_PUBLISH_EVENTS);
                }
            }
          else if (!math_floats_equal (
                     pl->enabled->control,
                     self->control))
            {
              g_debug (
                "plugin's own enabled "
                "changed - changing "
                "generic enabled");
              port_set_control_value (
                pl->enabled,
                self->control, false,
                F_PUBLISH_EVENTS);
            }
        }
    }

  if (forward_event)
    {
      port_forward_control_change_event (self);
    }
}

/**
 * Gets the given control value from the
 * corresponding underlying structure in the Port.
 *
 * @param normalized Whether to get the value
 *   normalized or not.
 */
float
port_get_control_value (
  Port *      self,
  const bool  normalize)
{
  g_return_val_if_fail (
    self->id.type == TYPE_CONTROL, 0.f);

  /* verify that plugin exists if plugin control */
  if (ZRYTHM_TESTING && self->is_project &&
      self->id.flags & PORT_FLAG_PLUGIN_CONTROL)
    {
      Plugin * pl = port_get_plugin (self, 1);
      g_return_val_if_fail (
        IS_PLUGIN_AND_NONNULL (pl), 0.f);
    }

  if (normalize)
    {
      return
        control_port_real_val_to_normalized (
          self, self->control);
    }
  else
    {
      return self->control;
    }
}

int
port_scale_point_cmp (
  const void * _a,
  const void * _b)
{
  const PortScalePoint * a =
    (PortScalePoint const *) _a;
  const PortScalePoint * b =
    (PortScalePoint const *) _b;
  return a->val - b->val > 0.f;
}

/**
 * Copies the metadata from a project port to
 * the given port.
 *
 * Used when doing delete actions so that ports
 * can be restored on undo.
 */
void
port_copy_metadata_from_project (
  Port * clone_port,
  Port * prj_port)
{
  clone_port->control = prj_port->control;
  clone_port->num_srcs = prj_port->num_srcs;
  clone_port->num_dests = prj_port->num_dests;

  /* realloc arrays */
  if ((int) clone_port->dests_size <
        clone_port->num_dests)
    {
      realloc_dests (
        clone_port, clone_port->dests_size,
        (size_t) clone_port->num_dests);
    }
  if ((int) clone_port->srcs_size <
        clone_port->num_srcs)
    {
      realloc_srcs (
        clone_port, clone_port->srcs_size,
        (size_t) clone_port->num_srcs);
    }

  for (int k = 0; k < prj_port->num_srcs; k++)
    {
      Port * src_port = prj_port->srcs[k];
      port_identifier_copy (
        &clone_port->src_ids[k], &src_port->id);
      clone_port->src_multipliers[k] =
        prj_port->src_multipliers[k];
      clone_port->src_enabled[k] =
        prj_port->src_enabled[k];
      clone_port->src_locked[k] =
        prj_port->src_locked[k];
    }
  for (int k = 0; k < prj_port->num_dests; k++)
    {
      Port * dest_port = prj_port->dests[k];
      port_identifier_copy (
        &clone_port->dest_ids[k], &dest_port->id);
      clone_port->multipliers[k] =
        prj_port->multipliers[k];
      clone_port->dest_enabled[k] =
        prj_port->dest_enabled[k];
      clone_port->dest_locked[k] =
        prj_port->dest_locked[k];
    }
}

/**
 * Reverts the data on the corresponding project
 * port for the given non-project port.
 *
 * This restores src/dest connections and the
 * control value.
 *
 * @param self Project port.
 * @param non_project Non-project port.
 */
void
port_restore_from_non_project (
  Port * self,
  Port * non_project)
{
  Port * prj_port = self;

  /* set value */
  prj_port->control = non_project->control;

  g_return_if_fail (
    non_project->num_srcs <=
      (int) non_project->srcs_size);
  g_return_if_fail (
    non_project->num_dests <=
      (int) non_project->dests_size);

  /* re-connect previously connected ports */
  for (int k = 0; k < non_project->num_srcs; k++)
    {
      Port * src_port =
        port_find_from_identifier (
          &non_project->src_ids[k]);
      g_return_if_fail (
        IS_PORT_AND_NONNULL (src_port));

      g_debug (
        "restoring source '%s' for port '%s'",
        non_project->id.label, src_port->id.label);
      port_connect (
        src_port, prj_port,
        non_project->src_locked[k]);
      int src_idx =
        port_get_src_index (
          prj_port, src_port);
      prj_port->src_multipliers[src_idx] =
        non_project->src_multipliers[k];
      prj_port->src_enabled[src_idx] =
        non_project->src_enabled[k];
      int dest_idx =
        port_get_dest_index (
          src_port, prj_port);
      src_port->multipliers[dest_idx] =
        non_project->src_multipliers[k];
      src_port->dest_enabled[dest_idx] =
        non_project->src_enabled[k];
    }
  for (int k = 0; k < non_project->num_dests; k++)
    {
      Port * dest_port =
        port_find_from_identifier (
          &non_project->dest_ids[k]);
      g_return_if_fail (
        IS_PORT_AND_NONNULL (dest_port));

      g_debug (
        "restoring dest '%s' for port '%s'",
        non_project->id.label, dest_port->id.label);
      port_connect (
        prj_port, dest_port,
        non_project->dest_locked[k]);
      int dest_idx =
        port_get_dest_index (
          prj_port, dest_port);
      prj_port->multipliers[dest_idx] =
        non_project->multipliers[k];
      prj_port->dest_enabled[dest_idx] =
        non_project->dest_enabled[k];
      int src_idx =
        port_get_src_index (
          dest_port, prj_port);
      dest_port->src_multipliers[src_idx] =
        non_project->multipliers[k];
      dest_port->src_enabled[src_idx] =
        non_project->dest_enabled[k];
    }
}

/**
 * Creates stereo ports for generic use.
 *
 * @param in 1 for in, 0 for out.
 * @param owner Pointer to the owner. The type is
 *   determined by owner_type.
 */
StereoPorts *
stereo_ports_new_generic (
  int           in,
  const char *  name,
  PortOwnerType owner_type,
  void *        owner)
{
  char * pll =
    g_strdup_printf (
      "%s L", name);
  char * plr =
    g_strdup_printf (
      "%s R", name);

  StereoPorts * ports =
    stereo_ports_new_from_existing (
      port_new_with_type (
        TYPE_AUDIO,
        in ? FLOW_INPUT : FLOW_OUTPUT,
        pll),
      port_new_with_type (
        TYPE_AUDIO,
        in ? FLOW_INPUT : FLOW_OUTPUT,
        plr));
  ports->l->id.flags |=
    PORT_FLAG_STEREO_L;
  ports->r->id.flags |=
    PORT_FLAG_STEREO_R;

  switch (owner_type)
    {
    case PORT_OWNER_TYPE_FADER:
    case PORT_OWNER_TYPE_PREFADER:
      port_set_owner_fader (
        ports->l, (Fader *) owner);
      port_set_owner_fader (
        ports->r, (Fader *) owner);
      break;
    case PORT_OWNER_TYPE_TRACK:
      port_set_owner_track (
        ports->l, (Track *) owner);
      port_set_owner_track (
        ports->r, (Track *) owner);
      break;
    case PORT_OWNER_TYPE_TRACK_PROCESSOR:
      port_set_owner_track_processor (
        ports->l, (TrackProcessor *) owner);
      port_set_owner_track_processor (
        ports->r, (TrackProcessor *) owner);
      break;
    case PORT_OWNER_TYPE_SAMPLE_PROCESSOR:
      port_set_owner_sample_processor (
        ports->l, (SampleProcessor *) owner);
      port_set_owner_sample_processor (
        ports->r, (SampleProcessor *) owner);
      break;
    case PORT_OWNER_TYPE_MONITOR_FADER:
      ports->l->id.owner_type =
        PORT_OWNER_TYPE_MONITOR_FADER;
      ports->r->id.owner_type =
        PORT_OWNER_TYPE_MONITOR_FADER;
    default:
      break;
    }

  g_free (pll);
  g_free (plr);

  return ports;
}

/**
 * First sets port buf to 0, then sums the given
 * port signal from its inputs.
 *
 * @param local_offset The start frame offset from
 *   0 in this cycle.
 * @param nframes The number of frames to process.
 * @param noroll Clear the port buffer in this
 *   range.
 */
void
port_process (
  Port *          port,
  const long      g_start_frames,
  const nframes_t local_offset,
  const nframes_t nframes,
  const bool      noroll)
{
  Port * src_port;
  int k;

  g_warn_if_fail (
    local_offset + nframes <=
      AUDIO_ENGINE->nframes);
  g_return_if_fail (IS_PORT (port));

  Track * track = NULL;
  if (port->id.owner_type ==
        PORT_OWNER_TYPE_TRACK_PROCESSOR ||
      port->id.owner_type ==
        PORT_OWNER_TYPE_TRACK ||
      port->id.owner_type ==
        PORT_OWNER_TYPE_FADER ||
      port->id.owner_type ==
        PORT_OWNER_TYPE_PREFADER ||
      (port->id.owner_type ==
        PORT_OWNER_TYPE_PLUGIN &&
       port->id.plugin_id.slot_type ==
         PLUGIN_SLOT_INSTRUMENT))
    {
      track = port_get_track (port, true);
      g_return_if_fail (
        IS_TRACK_AND_NONNULL (track));
    }

  bool is_stereo_port =
    port->id.flags & PORT_FLAG_STEREO_L ||
    port->id.flags & PORT_FLAG_STEREO_R;

  switch (port->id.type)
    {
    case TYPE_EVENT:
      if (noroll)
        break;

      if (G_UNLIKELY (
            port->id.owner_type ==
              PORT_OWNER_TYPE_TRACK_PROCESSOR &&
            !track))
        {
          g_return_if_reached ();
        }

      /* only consider incoming external data if
       * armed for recording (if the port is owner
       * by a track), otherwise always consider
       * incoming external data */
      if ((port->id.owner_type !=
             PORT_OWNER_TYPE_TRACK_PROCESSOR ||
           (port->id.owner_type ==
              PORT_OWNER_TYPE_TRACK_PROCESSOR &&
            track && track->recording)) &&
           port->id.flow == FLOW_INPUT)
        {
          switch (AUDIO_ENGINE->midi_backend)
            {
#ifdef HAVE_JACK
            case MIDI_BACKEND_JACK:
              sum_data_from_jack (
                port, local_offset, nframes);
              break;
#endif
#ifdef _WOE32
            case MIDI_BACKEND_WINDOWS_MME:
              sum_data_from_windows_mme (
                port, local_offset, nframes);
              break;
#endif
#ifdef HAVE_RTMIDI
            case MIDI_BACKEND_ALSA_RTMIDI:
            case MIDI_BACKEND_JACK_RTMIDI:
            case MIDI_BACKEND_WINDOWS_MME_RTMIDI:
            case MIDI_BACKEND_COREMIDI_RTMIDI:
              port_sum_data_from_rtmidi (
                port, local_offset, nframes);
              break;
#endif
            default:
              break;
            }
        }

      /* set midi capture if hardware */
      if (port->id.owner_type ==
            PORT_OWNER_TYPE_HW)
        {
          MidiEvents * events = port->midi_events;
          if (events->num_events > 0)
            {
              AUDIO_ENGINE->trigger_midi_activity =
                1;

              /* capture cc if capturing */
              if (AUDIO_ENGINE->capture_cc)
                {
                  memcpy (
                    AUDIO_ENGINE->last_cc,
                    events->events[
                      events->num_events - 1].
                        raw_buffer,
                    sizeof (midi_byte_t) * 3);
                }

              /* send cc to mapped ports */
              for (int i = 0; i < events->num_events; i++)
                {
                  MidiEvent * ev = &events->events[i];
                  midi_mappings_apply (
                    MIDI_MAPPINGS, ev->raw_buffer);
                }
            }
        }

      for (k = 0; k < port->num_srcs; k++)
        {
          src_port = port->srcs[k];
          int dest_idx =
            port_get_dest_index (
              src_port, port);
          if (src_port->dest_enabled[dest_idx])
            {
              g_return_if_fail (
                src_port->id.type == TYPE_EVENT);

              /* if hardware device connected to
               * track processor input, only allow
               * signal to pass if armed and
               * MIDI channel is valid */
              if (src_port->id.owner_type ==
                    PORT_OWNER_TYPE_HW &&
                  port->id.owner_type ==
                    PORT_OWNER_TYPE_TRACK_PROCESSOR)
                {
                  g_return_if_fail (track);

                  /* skip if not armed */
                  if (!track->recording)
                    {
                      continue;
                    }

                  /* if not set to "all channels",
                   * filter-append */
                  if ((track->type ==
                         TRACK_TYPE_MIDI ||
                       track->type ==
                         TRACK_TYPE_INSTRUMENT) &&
                       !track->channel->
                         all_midi_channels)
                    {
                      midi_events_append_w_filter (
                        src_port->midi_events,
                        port->midi_events,
                        track->channel->
                          midi_channels,
                        local_offset,
                        nframes, F_NOT_QUEUED);
                      continue;
                    }

                  /* otherwise append normally */
                }

              midi_events_append (
                src_port->midi_events,
                port->midi_events, local_offset,
                nframes, F_NOT_QUEUED);
            }
        }

      if (port->id.flow == FLOW_OUTPUT)
        {
          switch (AUDIO_ENGINE->midi_backend)
            {
#ifdef HAVE_JACK
            case MIDI_BACKEND_JACK:
              send_data_to_jack (
                port, local_offset, nframes);
              break;
#endif
#ifdef _WOE32
            case MIDI_BACKEND_WINDOWS_MME:
              send_data_to_windows_mme (
                port, local_offset, nframes);
              break;
#endif
            default:
              break;
            }
        }

      /* send UI notification */
      if (port->midi_events->num_events > 0)
        {
#if 0
          g_message (
            "port %s has %d events",
            port->id.label,
            port->midi_events->num_events);
#endif

          if (port->id.owner_type ==
                PORT_OWNER_TYPE_TRACK_PROCESSOR)
            {
              g_return_if_fail (
                IS_TRACK_AND_NONNULL (track));

              track->trigger_midi_activity = 1;
            }
        }

      if (local_offset + nframes ==
            AUDIO_ENGINE->block_length)
        {
          if (port->write_ring_buffers)
            {
              for (int i = port->midi_events->num_events - 1;
                   i >= 0; i--)
                {
                  if (zix_ring_write_space (
                        port->midi_ring) <
                        sizeof (MidiEvent))
                    {
                      zix_ring_skip (
                        port->midi_ring,
                        sizeof (MidiEvent));
                    }

                  MidiEvent * ev =
                    &port->midi_events->events[i];
                  ev->systime =
                    g_get_monotonic_time ();
                  zix_ring_write (
                    port->midi_ring, ev,
                    sizeof (MidiEvent));
                }
            }
          else
            {
              if (port->midi_events->num_events >
                    0)
                {
                  port->last_midi_event_time =
                    g_get_monotonic_time ();
                  g_atomic_int_set (
                    &port->has_midi_events, 1);
                }
            }
        }
      break;
    case TYPE_AUDIO:
    case TYPE_CV:
      if (noroll)
        {
          dsp_fill (
            &port->buf[local_offset],
            DENORMAL_PREVENTION_VAL, nframes);
          break;
        }

      if (port->id.owner_type ==
            PORT_OWNER_TYPE_TRACK_PROCESSOR &&
          !track)
        {
          g_return_if_reached ();
        }

      /* only consider incoming external data if
       * armed for recording (if the port is owner
       * by a track), otherwise always consider
       * incoming external data */
      if ((port->id.owner_type !=
             PORT_OWNER_TYPE_TRACK_PROCESSOR ||
           (port->id.owner_type ==
              PORT_OWNER_TYPE_TRACK_PROCESSOR &&
            track && track->recording)) &&
           port->id.flow == FLOW_INPUT)
        {
          switch (AUDIO_ENGINE->audio_backend)
            {
#ifdef HAVE_JACK
            case AUDIO_BACKEND_JACK:
              sum_data_from_jack (
                port, local_offset, nframes);
              break;
#endif
            case AUDIO_BACKEND_DUMMY:
              sum_data_from_dummy (
                port, local_offset, nframes);
              break;
            default:
              break;
            }
        }

      for (k = 0; k < port->num_srcs; k++)
        {
          src_port = port->srcs[k];
          int dest_idx =
            port_get_dest_index (
              src_port, port);
          if (!src_port->dest_enabled[dest_idx])
            continue;

          float minf = 0.f, maxf = 0.f,
                depth_range;
          if (G_LIKELY (
                port->id.type == TYPE_AUDIO))
            {
              minf = -1.f;
              maxf = 1.f;
            }
          else if (port->id.type == TYPE_CV)
            {
              maxf = port->maxf;
              minf = port->minf;
            }
          depth_range =
            (maxf - minf) / 2.f;

          if (port->id.type == TYPE_AUDIO)
            {
              minf = -2.f;
              maxf = 2.f;
            }

          /* sum the signals */
          float multiplier =
            depth_range *
              src_port->multipliers[
                port_get_dest_index (
                  src_port, port)];
          dsp_mix2 (
            &port->buf[local_offset],
            &src_port->buf[local_offset],
            1.f, multiplier, nframes);
          dsp_limit1 (
            &port->buf[local_offset],
            minf, maxf, nframes);
        }

      if (port->id.flow == FLOW_OUTPUT)
        {
          switch (AUDIO_ENGINE->audio_backend)
            {
#ifdef HAVE_JACK
            case AUDIO_BACKEND_JACK:
              send_data_to_jack (
                port, local_offset, nframes);
              break;
#endif
            default:
              break;
            }
        }

      if (local_offset + nframes ==
            AUDIO_ENGINE->block_length)
        {
          size_t size =
            sizeof (float) *
            (size_t) AUDIO_ENGINE->block_length;
          size_t write_space_avail =
            zix_ring_write_space (
              port->audio_ring);

          /* move the read head 8 blocks to make
           * space if no space avail to write */
          if (write_space_avail / size < 1)
            {
              zix_ring_skip (
                port->audio_ring, size * 8);
            }

          zix_ring_write (
            port->audio_ring, &port->buf[0],
            size);
        }

      /* if track output (to be shown on mixer) */
      if (port->id.owner_type ==
            PORT_OWNER_TYPE_TRACK &&
          is_stereo_port &&
          port->id.flow == FLOW_OUTPUT)
        {
          g_return_if_fail (
            IS_TRACK_AND_NONNULL (track));
          Channel * ch = track->channel;
          g_return_if_fail (ch);

          /* calculate meter values */
          if (port == ch->stereo_out->l ||
              port == ch->stereo_out->r)
            {
              /* reset peak if needed */
              gint64 time_now =
                g_get_monotonic_time ();
              if (time_now -
                    port->peak_timestamp >
                  TIME_TO_RESET_PEAK)
                port->peak = -1.f;

              bool changed =
                dsp_abs_max (
                  &port->buf[local_offset],
                  &port->peak,
                  nframes);
              if (changed)
                {
                  port->peak_timestamp =
                    g_get_monotonic_time ();
                }
            }
        }

        /* if bouncing tracks directly to
         * master (e.g., when bouncing the
         * track on its own without parents),
         * clear master input */
        if (G_UNLIKELY (
              AUDIO_ENGINE->bounce_mode >
                BOUNCE_OFF &&
              !AUDIO_ENGINE->bounce_with_parents &&
              (port ==
                 P_MASTER_TRACK->processor->stereo_in->l ||
               port ==
                 P_MASTER_TRACK->processor->stereo_in->r)))

          {
            dsp_fill (
              &port->buf[local_offset],
              AUDIO_ENGINE->denormal_prevention_val,
              nframes);
          }

        /* if bouncing track directly to
         * master (e.g., when bouncing the
         * track on its own without parents),
         * add the buffer to master output */
        if (G_UNLIKELY (
              AUDIO_ENGINE->bounce_mode >
                BOUNCE_OFF &&
              (port->id.owner_type ==
                 PORT_OWNER_TYPE_TRACK ||
               port->id.owner_type ==
                 PORT_OWNER_TYPE_TRACK_PROCESSOR ||
               port->id.owner_type ==
                 PORT_OWNER_TYPE_PREFADER ||
               (port->id.owner_type ==
                 PORT_OWNER_TYPE_PLUGIN &&
                port->id.plugin_id.slot_type ==
                  PLUGIN_SLOT_INSTRUMENT)) &&
              is_stereo_port &&
              port->id.flow == FLOW_OUTPUT &&
              track && track->bounce_to_master))
          {
#if 0
            char id_str[6000];
            port_get_full_designation (port, id_str);
            g_message ("%s:", id_str);
#endif

#define _ADD(l_or_r) \
  dsp_add2 ( \
    &P_MASTER_TRACK->channel->stereo_out-> \
       l_or_r->buf[local_offset], \
    &port->buf[local_offset], nframes)

            Channel * ch;
            Fader * prefader;
            TrackProcessor * tp;
            switch (AUDIO_ENGINE->bounce_step)
              {
              case BOUNCE_STEP_BEFORE_INSERTS:
                tp = track->processor;
                g_return_if_fail (tp);
                if (track->type ==
                      TRACK_TYPE_INSTRUMENT)
                  {
                    if (port == track->channel->instrument->l_out)
                      {
                        _ADD (l);
                      }
                    if (port == track->channel->instrument->r_out)
                      {
                        _ADD (r);
                      }
                  }
                else if (tp->stereo_out &&
                           track->bounce)
                  {
                    if (port == tp->stereo_out->l)
                      {
                        _ADD (l);
                      }
                    else if (port ==
                               tp->stereo_out->r)
                      {
                        _ADD (r);
                      }
                  }
                break;
              case BOUNCE_STEP_PRE_FADER:
                ch = track->channel;
                g_return_if_fail (ch);
                prefader = ch->prefader;
                if (port == prefader->stereo_out->l)
                  {
                    _ADD (l);
                  }
                else if (port ==
                           prefader->stereo_out->r)
                  {
                    _ADD (r);
                  }
                break;
              case BOUNCE_STEP_POST_FADER:
                ch = track->channel;
                g_return_if_fail (ch);
                if (track->type !=
                      TRACK_TYPE_MASTER)
                  {
                    if (port == ch->stereo_out->l)
                      {
                        _ADD (l);
                      }
                    else if (port ==
                               ch->stereo_out->r)
                      {
                        _ADD (r);
                      }
                  }
                break;
              }
#undef _ADD

          }
      break;
    case TYPE_CONTROL:
      {
        if (port->id.flow != FLOW_INPUT ||
            port->id.owner_type ==
              PORT_OWNER_TYPE_MONITOR_FADER ||
            port->id.owner_type ==
              PORT_OWNER_TYPE_PREFADER ||
            port->id.flags & PORT_FLAG_TP_MONO ||
            port->id.flags &
              PORT_FLAG_TP_INPUT_GAIN ||
            !(port->id.flags &
                PORT_FLAG_AUTOMATABLE))
          {
            break;
          }

        /* calculate value from automation track */
        g_return_if_fail (
          port->id.flags & PORT_FLAG_AUTOMATABLE);
        AutomationTrack * at = port->at;
        if (G_UNLIKELY (!at))
          {
            g_critical (
              "No automation track found for port "
              "%s", port->id.label);
          }
        if (ZRYTHM_TESTING && at)
          {
            AutomationTrack * found_at =
              automation_track_find_from_port (
                port, NULL, true);
            g_return_if_fail (
              at == found_at);
          }
        if (at &&
            port->id.flags &
              PORT_FLAG_AUTOMATABLE &&
            automation_track_should_read_automation (
              at, AUDIO_ENGINE->timestamp_start))
          {
            /* FIXME optimize, calculate this
             * once at the start of each cycle */
            Position pos;
            position_from_frames (
              &pos, g_start_frames);

            /* if playhead pos changed manually
             * recently or transport is rolling,
             * we will force the last known
             * automation point value regardless
             * of whether there is a region at
             * current pos */
            bool can_read_previous_automation =
              TRANSPORT_IS_ROLLING ||
              TRANSPORT->last_manual_playhead_change -
              AUDIO_ENGINE->last_timestamp_start > 0;

            /* if there was an automation event
             * at the playhead position, set val
             * and flag */
            AutomationPoint * ap =
              automation_track_get_ap_before_pos (
                at, &pos,
                !can_read_previous_automation);
            if (ap)
              {
                float val =
                  automation_track_get_val_at_pos (
                    at, &pos, true,
                    !can_read_previous_automation);
                control_port_set_val_from_normalized (
                  port, val, true);
                port->value_changed_from_reading =
                  true;
              }
          }

        float maxf, minf, depth_range, val_to_use;
        /* whether this is the first CV processed
         * on this control port */
        bool first_cv = true;
        for (k = 0; k < port->num_srcs; k++)
          {
            src_port = port->srcs[k];
            int dest_idx =
              port_get_dest_index (
                src_port, port);
            if (!src_port->dest_enabled[
                   dest_idx])
              continue;

            if (src_port->id.type == TYPE_CV)
              {
                maxf = port->maxf;
                minf = port->minf;

                /*float deff =*/
                  /*port->lv2_port->lv2_control->deff;*/
                /*port->lv2_port->control =*/
                  /*deff + (maxf - deff) **/
                    /*src_port->buf[0];*/
                depth_range =
                  (maxf - minf) / 2.f;

                /* figure out whether to use base
                 * value or the current value */
                if (first_cv)
                  {
                    val_to_use = port->base_value;
                    first_cv = false;
                  }
                else
                  {
                    val_to_use = port->control;
                  }

                float result =
                  CLAMP (
                    val_to_use +
                      depth_range *
                        src_port->buf[0] *
                        src_port->multipliers[
                          port_get_dest_index (
                            src_port, port)],
                    minf, maxf);
                port->control = result;
                port_forward_control_change_event (
                  port);
              }
          }
      }
      break;
    default:
      break;
    }
}

/**
 * Disconnects all hardware inputs from the port.
 */
void
port_disconnect_hw_inputs (
  Port * self)
{
  for (int i = 0; i < self->num_srcs; i++)
    {
      Port * src = self->srcs[i];
      if (src->id.owner_type == PORT_OWNER_TYPE_HW)
        {
          port_disconnect (src, self);
        }
    }
}

/**
 * Sets whether to expose the port to the backend
 * and exposes it or removes it.
 *
 * It checks what the backend is using the engine's
 * audio backend or midi backend settings.
 */
void
port_set_expose_to_backend (
  Port * self,
  int    expose)
{
  g_return_if_fail (
    AUDIO_ENGINE && AUDIO_ENGINE->setup);

  if (self->id.type == TYPE_AUDIO)
    {
      switch (AUDIO_ENGINE->audio_backend)
        {
#ifdef HAVE_JACK
        case AUDIO_BACKEND_JACK:
          expose_to_jack (self, expose);
          break;
#endif
#ifdef HAVE_RTAUDIO
        case AUDIO_BACKEND_ALSA_RTAUDIO:
        case AUDIO_BACKEND_JACK_RTAUDIO:
        case AUDIO_BACKEND_PULSEAUDIO_RTAUDIO:
        case AUDIO_BACKEND_COREAUDIO_RTAUDIO:
        case AUDIO_BACKEND_WASAPI_RTAUDIO:
        case AUDIO_BACKEND_ASIO_RTAUDIO:
          expose_to_rtaudio (self, expose);
          break;
#endif
        default:
          break;
        }
    }
  else if (self->id.type == TYPE_EVENT)
    {
      switch (AUDIO_ENGINE->midi_backend)
        {
#ifdef HAVE_JACK
        case MIDI_BACKEND_JACK:
          expose_to_jack (self, expose);
          break;
#endif
#ifdef HAVE_ALSA
        case MIDI_BACKEND_ALSA:
#if 0
          expose_to_alsa (self, expose);
#endif
          break;
#endif
#ifdef HAVE_RTMIDI
        case MIDI_BACKEND_ALSA_RTMIDI:
        case MIDI_BACKEND_JACK_RTMIDI:
        case MIDI_BACKEND_WINDOWS_MME_RTMIDI:
        case MIDI_BACKEND_COREMIDI_RTMIDI:
          expose_to_rtmidi (self, expose);
          break;
#endif
        default:
          break;
        }
    }
  else
    g_return_if_reached ();
}

/**
 * Returns if the port is exposed to the backend.
 */
int
port_is_exposed_to_backend (
  const Port * self)
{
  return
    self->internal_type == INTERNAL_JACK_PORT ||
    self->internal_type ==
      INTERNAL_ALSA_SEQ_PORT ||
    self->id.owner_type ==
      PORT_OWNER_TYPE_BACKEND ||
    self->exposed_to_backend;
}

/**
 * Renames the port on the backend side.
 */
void
port_rename_backend (
  Port * self)
{
  if ((!port_is_exposed_to_backend (self)))
    return;

  switch (self->internal_type)
    {
#ifdef HAVE_JACK
    case INTERNAL_JACK_PORT:
      {
        char str[600];
        port_get_full_designation (self, str);
        jack_port_rename (
          AUDIO_ENGINE->client,
          (jack_port_t *) self->data,
          str);
      }
      break;
#endif
    case INTERNAL_ALSA_SEQ_PORT:
      break;
    default:
      break;
    }
}

/**
 * If MIDI port, returns if there are any events,
 * if audio port, returns if there is sound in the
 * buffer.
 */
bool
port_has_sound (
  Port * self)
{
  switch (self->id.type)
    {
    case TYPE_AUDIO:
      g_return_val_if_fail (self->buf, false);
      for (nframes_t i = 0;
           i < AUDIO_ENGINE->block_length; i++)
        {
          if (fabsf (self->buf[i]) > 0.0000001f)
            {
              return true;
            }
        }
      break;
    case TYPE_EVENT:
      /* TODO */
      break;
    default:
      break;
    }

  return false;
}

/**
 * Copies a full designation of \p self in the
 * format "Track/Port" or "Track/Plugin/Port" in
 * \p buf.
 */
void
port_get_full_designation (
  Port * self,
  char * buf)
{
  const PortIdentifier * id = &self->id;

  switch (id->owner_type)
    {
    case PORT_OWNER_TYPE_BACKEND:
    case PORT_OWNER_TYPE_SAMPLE_PROCESSOR:
      strcpy (buf, id->label);
      return;
    case PORT_OWNER_TYPE_PLUGIN:
      {
        Plugin * pl = port_get_plugin (self, 1);
        g_return_if_fail (pl);
        Track * track =
          plugin_get_track (pl);
        g_return_if_fail (track);
        sprintf (
          buf, "%s/%s/%s",
          track->name, pl->setting->descr->name,
          id->label);
      }
      return;
    case PORT_OWNER_TYPE_TRACK:
    case PORT_OWNER_TYPE_TRACK_PROCESSOR:
    case PORT_OWNER_TYPE_PREFADER:
    case PORT_OWNER_TYPE_FADER:
    case PORT_OWNER_TYPE_CHANNEL_SEND:
      {
        Track * tr = port_get_track (self, 1);
        g_return_if_fail (
          IS_TRACK_AND_NONNULL (tr));
        sprintf (
          buf, "%s/%s", tr->name, id->label);
      }
      return;
    case PORT_OWNER_TYPE_MONITOR_FADER:
      sprintf (buf, "Engine/%s", id->label);
      return;
    case PORT_OWNER_TYPE_HW:
      sprintf (buf, "HW/%s", id->label);
      return;
    case PORT_OWNER_TYPE_TRANSPORT:
      sprintf (buf, "Transport/%s", id->label);
      return;
    default:
      g_return_if_reached ();
    }
}

/**
 * Prints all connections.
 */
void
port_print_connections_all ()
{
  /*Port * src, * dest;*/
  /*int i, j;*/

  /*for (i = 0; i < PROJECT->num_ports; i++)*/
    /*{*/
      /*src = project_get_port (i);*/
      /*if (!src->owner_pl &&*/
          /*!src->owner_tr &&*/
          /*!src->owner_backend)*/
        /*{*/
          /*g_warning ("Port %s has no owner",*/
                     /*src->identifier.label);*/
        /*}*/
      /*for (j = 0; j < src->num_dests; j++)*/
        /*{*/
          /*dest = src->dests[j];*/
          /*g_warn_if_fail (dest);*/
          /*g_message ("%s connected to %s", src->identifier.label, dest->identifier.label);*/
        /*}*/
    /*}*/
}

/**
 * Clears the port buffer.
 */
void
port_clear_buffer (Port * port)
{
  PortIdentifier * pi = &port->id;
  if ((pi->type == TYPE_AUDIO ||
       pi->type == TYPE_CV) && port->buf)
    {
      dsp_fill (
        port->buf, DENORMAL_PREVENTION_VAL,
        AUDIO_ENGINE->block_length);

      return;
    }
  else if (port->id.type == TYPE_EVENT &&
      port->midi_events)
    {
      port->midi_events->num_events = 0;
    }
}

/**
 * Returns if the connection from \p src to \p
 * dest is locked or not.
 */
int
port_is_connection_locked (
  Port * src,
  Port * dest)
{
  for (int i = 0; i < src->num_dests; i++)
    {
      if (src->dests[i] == dest)
        {
          if (src->dest_locked[i])
            return 1;
          else
            return 0;
        }
    }
  g_return_val_if_reached (0);
}

Track *
port_get_track (
  const Port * self,
  int   warn_if_fail)
{
  g_return_val_if_fail (IS_PORT (self), NULL);

  Track * track = NULL;
  if (self->id.track_pos != -1)
    {
      g_return_val_if_fail (
        ZRYTHM && TRACKLIST, NULL);

      track =
        TRACKLIST->tracks[self->id.track_pos];
    }

  if (!track && warn_if_fail)
    {
      g_warning ("not found");
    }

  return track;
}

Plugin *
port_get_plugin (
  Port * self,
  bool   warn_if_fail)
{
  g_return_val_if_fail (IS_PORT (self), NULL);

  Track * track = port_get_track (self, 0);
  if (!track && self->tmp_plugin)
    {
      return self->tmp_plugin;
    }
  if (!track ||
      (track->type != TRACK_TYPE_MODULATOR &&
         !track->channel))
    {
      if (warn_if_fail)
        {
          g_warning (
            "No track found for port");
        }
      return NULL;
    }

  Plugin * pl = NULL;
  PluginIdentifier * pl_id = &self->id.plugin_id;
  switch (pl_id->slot_type)
    {
    case PLUGIN_SLOT_MIDI_FX:
      pl = track->channel->midi_fx[pl_id->slot];
      break;
    case PLUGIN_SLOT_INSTRUMENT:
      pl = track->channel->instrument;
      break;
    case PLUGIN_SLOT_INSERT:
      pl = track->channel->inserts[pl_id->slot];
      break;
    case PLUGIN_SLOT_MODULATOR:
      pl = track->modulators[pl_id->slot];
      break;
    default:
      g_return_val_if_reached (NULL);
      break;
    }
  if (!pl && self->tmp_plugin)
    return self->tmp_plugin;

  if (!pl && warn_if_fail)
    {
      g_critical (
        "plugin at slot type %s (slot %d) not "
        "found for port %s",
        plugin_slot_type_to_string (
          pl_id->slot_type),
        pl_id->slot,
        self->id.label);
      return NULL;
    }

  /* unset \ref Port.tmp_plugin if a Plugin was
   * found */
  self->tmp_plugin = NULL;

  return pl;
}

/**
 * Applies the pan to the given L/R ports.
 */
void
port_apply_pan_stereo (
  Port *       l,
  Port *       r,
  float        pan,
  PanLaw       pan_law,
  PanAlgorithm pan_algo)
{
  /* FIXME not used */
  g_warn_if_reached ();
  /*int nframes = AUDIO_ENGINE->block_length;*/
  /*int i;*/
  /*switch (pan_algo)*/
    /*{*/
    /*case PAN_ALGORITHM_SINE_LAW:*/
      /*for (i = 0; i < nframes; i++)*/
        /*{*/
          /*if (r->buf[i] != 0.f)*/
            /*r->buf[i] *= sinf (pan * (M_PIF / 2.f));*/
          /*if (l->buf[i] != 0.f)*/
            /*l->buf[i] *= sinf ((1.f - pan) * (M_PIF / 2.f));*/
        /*}*/
      /*break;*/
    /*case PAN_ALGORITHM_SQUARE_ROOT:*/
      /*break;*/
    /*case PAN_ALGORITHM_LINEAR:*/
      /*for (i = 0; i < nframes; i++)*/
        /*{*/
          /*if (r->buf[i] != 0.f)*/
            /*r->buf[i] *= pan;*/
          /*if (l->buf[i] != 0.f)*/
            /*l->buf[i] *= (1.f - pan);*/
        /*}*/
      /*break;*/
    /*}*/
}

/**
 * Applies the pan to the given port.
 *
 * @param start_frame The start frame offset from
 *   0 in this cycle.
 * @param nframes The number of frames to process.
 */
void
port_apply_pan (
  Port *       port,
  float        pan,
  PanLaw       pan_law,
  PanAlgorithm pan_algo,
  nframes_t    start_frame,
  const nframes_t    nframes)
{
  float calc_r, calc_l;
  pan_get_calc_lr (
    pan_law, pan_algo, pan, &calc_l, &calc_r);

  /* if stereo R */
  if (port->id.flags & PORT_FLAG_STEREO_R)
    {
      dsp_mul_k2 (
        &port->buf[start_frame], calc_r, nframes);
    }
  /* else if stereo L */
  else
    {
      dsp_mul_k2 (
        &port->buf[start_frame], calc_l, nframes);
    }
}

void
port_set_multiplier (
  Port * src,
  Port * dest,
  float  val)
{
  int dest_idx = port_get_dest_index (src, dest);
  int src_idx = port_get_src_index (dest, src);
  port_set_multiplier_by_index (src, dest_idx, val);
  port_set_src_multiplier_by_index (
    dest, src_idx, val);
}

float
port_get_multiplier (
  Port * src,
  Port * dest)
{
  int dest_idx = port_get_dest_index (src, dest);
  return
    port_get_multiplier_by_index (src, dest_idx);
}

void
port_set_enabled (
  Port * src,
  Port * dest,
  bool   enabled)
{
  int dest_idx = port_get_dest_index (src, dest);
  int src_idx = port_get_src_index (dest, src);
  src->dest_enabled[dest_idx] = enabled;
  dest->src_enabled[src_idx] = enabled;
}

bool
port_get_enabled (
  Port * src,
  Port * dest)
{
  int dest_idx = port_get_dest_index (src, dest);
  return src->dest_enabled[dest_idx];
}

/**
 * Deletes port, doing required cleanup and
 * updating counters.
 */
void
port_free (Port * self)
{
#if 0
  g_debug ("freeing %s...", self->id.label);
#endif

  /* assert no connections. some ports need to
   * have fake connections (see delete tracks
   * action) so check the first actual element
   * instead */
  g_warn_if_fail (
    self->num_srcs == 0 ||
    !self->srcs ||
    self->srcs[0] == 0);
  g_warn_if_fail (
    self->num_dests == 0 ||
    !self->dests ||
    self->dests[0] == 0);

  object_zero_and_free (self->buf);
  object_free_w_func_and_null (
    zix_ring_free, self->audio_ring);
  object_free_w_func_and_null (
    zix_ring_free, self->midi_ring);

  object_free_w_func_and_null (
    midi_events_free, self->midi_events);

#ifdef HAVE_RTMIDI
  for (int i = 0; i < self->num_rtmidi_ins; i++)
    {
      rtmidi_device_close (
        self->rtmidi_ins[i], 1);
    }
#endif

  object_zero_and_free (self->srcs);
  object_zero_and_free (self->dests);
  object_zero_and_free (self->multipliers);
  object_zero_and_free (self->src_multipliers);
  object_zero_and_free (self->dest_locked);
  object_zero_and_free (self->src_locked);
  object_zero_and_free (self->dest_enabled);
  object_zero_and_free (self->src_enabled);

  object_free_w_func_and_null (
    lv2_evbuf_free, self->evbuf);

  port_identifier_free_members (&self->id);

  object_zero_and_free (self);
}
