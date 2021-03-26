/*
 * Copyright (C) 2019-2020 Alexandros Theodotou <alex at zrythm dot org>
 *
 * This file is part of Zrythm
 *
 * Zrythm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, version 3.
 *
 * Zrythm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Zrythm.  If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * \file
 *
 * Recording events to queue for the recording
 * thread to handle.
 */

#ifndef __AUDIO_RECORDING_EVENTS_H__
#define __AUDIO_RECORDING_EVENTS_H__

#include "audio/midi_event.h"
#include "audio/port_identifier.h"
#include "utils/types.h"

/**
 * @addtogroup audio
 *
 * @{
 */

#define recording_event_queue_push_back_event(q,x) \
  mpmc_queue_push_back (q, (void *) x)

#define recording_event_queue_dequeue_event(q,x) \
  mpmc_queue_dequeue (q, (void *) x)

/**
 * Push events.
 */
#define RECORDING_EVENTS_PUSH_AUDIO(et,_arg) \
  if (RECORDING_MANAGER->event_queue) \
    { \
      RecordingEvent * ev = \
        (RecordingEvent *) \
        object_pool_get ( \
          RECORDING_MANAGER->event_obj_pool); \
      ev->type = et; \
      ev->arg = (void *) _arg; \
      event_queue_push_back_event ( \
        RECORDING_MANAGER->event_queue, _ev); \
    }

typedef enum RecordingEventType
{
  RECORDING_EVENT_TYPE_START_TRACK_RECORDING,
  RECORDING_EVENT_TYPE_START_AUTOMATION_RECORDING,

  /** These events are for processing any range. */
  RECORDING_EVENT_TYPE_MIDI,
  RECORDING_EVENT_TYPE_AUDIO,
  RECORDING_EVENT_TYPE_AUTOMATION,

  /**
   * These events are for temporarily stopping
   * recording (eg, when outside the punch range or
   * when looping).
   *
   * The nframes must always be 0 for these events.
   */
  RECORDING_EVENT_TYPE_PAUSE_TRACK_RECORDING,
  RECORDING_EVENT_TYPE_PAUSE_AUTOMATION_RECORDING,

  RECORDING_EVENT_TYPE_STOP_TRACK_RECORDING,
  RECORDING_EVENT_TYPE_STOP_AUTOMATION_RECORDING,
} RecordingEventType;

/**
 * A recording event.
 *
 * During recording, a recording event must be sent
 * in each cycle for all record-enabled tracks.
 */
typedef struct RecordingEvent
{
  RecordingEventType type;

  /** The name of the track this event is for. */
  char       track_name[200];

  /** ZRegion name, if applicable. */
  char       region_name[200];

  /** Global start frames of the event. */
  long       g_start_frames;

  /** Offset from \ref RecordingEvent.g_start_frames
   * that this event starts from. */
  nframes_t  local_offset;

  /**
   * The actual data (if audio).
   *
   * This will be \ref RecordingEvent.nframes times
   * the number of channels in the track.
   */
  float      lbuf[9000];
  float      rbuf[9000];

  int        has_midi_event;

  /**
   * MidiEvent, if midi.
   */
  MidiEvent  midi_event;

  /** Port if automation. */
  PortIdentifier   port_id;

  /** Automation value, if automation. */
  //float             control_val;

  /** Number of frames processed in this event. */
  nframes_t  nframes;

  /* debug info */
  const char * file;
  const char * func;
  int          lineno;
} RecordingEvent;

/**
 * Inits an already allocated recording event.
 */
#define recording_event_init(re) \
  re->file = __FILE__; \
  re->func = __func__; \
  re->lineno = __LINE__

COLD
RecordingEvent *
recording_event_new (void);

void
recording_event_print (
  RecordingEvent * self);

void
recording_event_free (
  RecordingEvent * self);

/**
 * @}
 */

#endif
