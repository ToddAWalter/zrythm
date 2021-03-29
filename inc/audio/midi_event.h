/*
 * Copyright (C) 2018-2020 Alexandros Theodotou <alex at zrythm dot org>
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

/**
 * \file
 *
 * MIDI events.
 */

#ifndef __AUDIO_MIDI_EVENT_H__
#define __AUDIO_MIDI_EVENT_H__

#include "zrythm-config.h"

#include <stdint.h>
#include <string.h>

#include "utils/types.h"
#include "zix/sem.h"

#ifdef HAVE_JACK
#include "weak_libjack.h"
#endif

#include <gtk/gtk.h>

typedef struct ChordDescriptor ChordDescriptor;

/**
 * @addtogroup audio
 *
 * @{
 */

/** Max events to hold in queues. */
#define MAX_MIDI_EVENTS 2560

/**
 * Type of MIDI event.
 *
 * These are in order of precedence.
 */
typedef enum MidiEventType
{
  MIDI_EVENT_TYPE_PITCHBEND,
  MIDI_EVENT_TYPE_CONTROLLER,
  MIDI_EVENT_TYPE_NOTE_OFF,
  MIDI_EVENT_TYPE_NOTE_ON,
  MIDI_EVENT_TYPE_ALL_NOTES_OFF,
} MidiEventType;

/**
 * Backend-agnostic MIDI event descriptor.
 */
typedef struct MidiEvent
{
  /** The values below will be filled in
   * depending on what event this is. */
  MidiEventType  type;

  /** -8192 to 8191. */
  int            pitchbend;

  /** The controller, for control events. */
  midi_byte_t    controller;

  /** Control value (also used for modulation
   * wheel (0 ~ 127). */
  midi_byte_t    control;

  /** MIDI channel, starting from 1. */
  midi_byte_t    channel;

  /** Note value (0 ~ 127). */
  midi_byte_t    note_pitch;

  /** Velocity (0 ~ 127). */
  midi_byte_t    velocity;

  /** Time of the MIDI event, in frames from the
   * start of the current cycle. */
  midi_time_t    time;

  /** Time using g_get_monotonic_time (). */
  gint64         systime;

  /** Raw MIDI data. */
  midi_byte_t    raw_buffer[3];

} MidiEvent;

typedef struct MidiEvents MidiEvents;
typedef struct Port Port;

/**
 * Container for passing midi events through ports.
 * This should be passed in the data field of MIDI Ports
 */
typedef struct MidiEvents
{
  /** Event count. */
  volatile int num_events;

  /** Events to use in this cycle. */
  MidiEvent  events[MAX_MIDI_EVENTS];

  /**
   * For queueing events from the GUI or from ALSA
   * at random times, since they run in different
   * threads.
   *
   * Engine will copy them to the
   * original MIDI events when ready to be processed
   *
   * Also has other uses.
   */
  MidiEvent  queued_events[MAX_MIDI_EVENTS];
  volatile int num_queued_events;

  /** Semaphore for exclusive read/write. */
  ZixSem     access_sem;

  /** Cache, pointer back to owner Port. */
  /* FIXME not needed */
  Port *     port;

} MidiEvents;

/**
 * Used by Windows MME and RtMidi when adding events
 * to the ring.
 */
typedef struct MidiEventHeader
{
  uint64_t time;
  size_t   size;
} MidiEventHeader;

/**
 * Inits the MidiEvents struct.
 */
void
midi_events_init (
  MidiEvents * self);

/**
 * Allocates and inits a MidiEvents struct.
 *
 * @param port Owner Port.
 */
MidiEvents *
midi_events_new (
  Port * port);

/**
 * Copies the members from one MidiEvent to another.
 */
static inline void
midi_event_copy (
  MidiEvent * dest,
  MidiEvent * src)
{
  memcpy (dest, src, sizeof (MidiEvent));
}

NONNULL
void
midi_event_set_velocity (
  MidiEvent * ev,
  midi_byte_t vel);

void
midi_event_print (
  const MidiEvent * ev);

int
midi_events_are_equal (
  const MidiEvent * src,
  const MidiEvent * dest);

/**
 * Sorts the MIDI events by time ascendingly.
 */
void
midi_events_sort_by_time (
  MidiEvents * self);

void
midi_events_print (
  MidiEvents * self,
  const int    queued);

/**
 * Appends the events from src to dest
 *
 * @param queued Append queued events instead of
 *   main events.
 * @param start_frame The start frame offset from 0
 *   in this cycle.
 * @param nframes Number of frames to process.
 */
void
midi_events_append (
  MidiEvents * src,
  MidiEvents * dest,
  const nframes_t    start_frame,
  const nframes_t    nframes,
  bool          queued);

/**
 * Appends the events from src to dest
 *
 * @param queued Append queued events instead of
 *   main events.
 * @param channels Allowed channels (array of 16
 *   booleans).
 * @param start_frame The start frame offset from 0
 *   in this cycle.
 * @param nframes Number of frames to process.
 */
void
midi_events_append_w_filter (
  MidiEvents *    src,
  MidiEvents *    dest,
  int *           channels,
  const nframes_t start_frame,
  const nframes_t nframes,
  bool            queued);

/**
 * Adds a note on event to the given MidiEvents.
 *
 * @param channel MIDI channel starting from 1.
 * @param queued Add to queued events instead.
 */
void
midi_events_add_note_on (
  MidiEvents * self,
  midi_byte_t  channel,
  midi_byte_t  note_pitch,
  midi_byte_t  velocity,
  midi_time_t  time,
  int          queued);

/**
 * Adds a note on for each note in the chord.
 */
void
midi_events_add_note_ons_from_chord_descr (
  MidiEvents *      self,
  ChordDescriptor * descr,
  midi_byte_t       channel,
  midi_byte_t       velocity,
  midi_time_t       time,
  bool              queued);

/**
 * Adds a note off for each note in the chord.
 */
void
midi_events_add_note_offs_from_chord_descr (
  MidiEvents *      self,
  ChordDescriptor * descr,
  midi_byte_t       channel,
  midi_time_t       time,
  bool              queued);

/**
 * Add CC volume event.
 *
 * TODO
 */
void
midi_events_add_cc_volume (
  MidiEvents *      self,
  midi_byte_t       channel,
  midi_byte_t       volume,
  midi_time_t       time,
  bool              queued);

/**
 * Returrns if the MidiEvents have any note on
 * events.
 *
 * @param check_main Check the main events.
 * @param check_queued Check the queued events.
 */
int
midi_events_has_note_on (
  MidiEvents * self,
  int          check_main,
  int          check_queued);

/**
 * Parses a MidiEvent from a raw MIDI buffer.
 *
 * This must be a full 3-byte message. If in
 * 'running status' mode, the caller is responsible
 * for prepending the status byte.
 */
void
midi_events_add_event_from_buf (
  MidiEvents *  self,
  midi_time_t   time,
  midi_byte_t * buf,
  int           buf_size,
  int           queued);

/**
 * Adds a note off event to the given MidiEvents.
 *
 * @param channel MIDI channel starting from 1.
 * @param queued Add to queued events instead.
 */
void
midi_events_add_note_off (
  MidiEvents * self,
  midi_byte_t  channel,
  midi_byte_t  note_pitch,
  midi_time_t  time,
  int          queued);

/**
 * Adds a control event to the given MidiEvents.
 *
 * @param channel MIDI channel starting from 1.
 * @param queued Add to queued events instead.
 */
void
midi_events_add_control_change (
  MidiEvents * self,
  midi_byte_t  channel,
  midi_byte_t  controller,
  midi_byte_t  control,
  midi_time_t  time,
  int          queued);

/**
 * Adds a control event to the given MidiEvents.
 *
 * @param channel MIDI channel starting from 1.
 * @param pitchbend -8192 to 8191
 * @param queued Add to queued events instead.
 */
void
midi_events_add_pitchbend (
  MidiEvents * self,
  midi_byte_t  channel,
  int          pitchbend,
  midi_time_t  time,
  int          queued);

/**
 * Queues MIDI note off to event queue.
 */
void
midi_events_add_all_notes_off (
  MidiEvents * self,
  midi_byte_t  channel,
  midi_time_t  time,
  bool         queued);

void
midi_events_panic (
  MidiEvents * self,
  bool         queued);

/**
 * Clears midi events.
 *
 * @param queued Clear queued events instead.
 */
void
midi_events_clear (
  MidiEvents * midi_events,
  int          queued);

/**
 * Clears duplicates.
 *
 * @param queued Clear duplicates from queued events
 * instead.
 */
void
midi_events_clear_duplicates (
  MidiEvents * midi_events,
  const int    queued);

/**
 * Copies the queue contents to the original struct
 */
void
midi_events_dequeue (
  MidiEvents * midi_events);

/**
 * Returns if a note on event for the given note
 * exists in the given events.
 */
int
midi_events_check_for_note_on (
  MidiEvents * midi_events,
  int          note,
  int          queued);

/**
 * Deletes the midi event with a note on signal
 * from the queue, and returns if it deleted or not.
 */
int
midi_events_delete_note_on (
  MidiEvents * midi_events,
  int          note,
  int          queued);

#ifdef HAVE_JACK
/**
 * Writes the events to the given JACK buffer.
 */
void
midi_events_copy_to_jack (
  MidiEvents * self,
  void *       buff);
#endif

/**
 * Sorts the MidiEvents by time.
 */
void
midi_events_sort (
  MidiEvents * self,
  const bool   queued);

/**
 * Sets the given MIDI channel on all applicable
 * MIDI events.
 */
void
midi_events_set_channel (
  MidiEvents *      self,
  const int         queued,
  const midi_byte_t channel);

void
midi_events_delete_event (
  MidiEvents *      events,
  const MidiEvent * ev,
  const bool        queued);

/**
 * Frees the MIDI events.
 */
void
midi_events_free (
  MidiEvents * self);

/**
 * @}
 */

#endif
