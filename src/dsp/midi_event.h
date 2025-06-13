// SPDX-FileCopyrightText: © 2018-2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#pragma once

#include "zrythm-config.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

#include "utils/types.h"

#include <midilib/src/midifile.h>
#include <moodycamel/lightweightsemaphore.h>

namespace zrythm::dsp
{
class ChordDescriptor;

/** Max events to hold in queues. */
constexpr int MAX_MIDI_EVENTS = 2560;

/**
 * Timed MIDI event.
 */
struct MidiEvent final
{
public:
  // Rule of 0
  MidiEvent () = default;
  MidiEvent (
    midi_byte_t byte1,
    midi_byte_t byte2,
    midi_byte_t byte3,
    midi_time_t time)
      : raw_buffer_sz_ (3), time_ (time)
  {
    raw_buffer_[0] = byte1;
    raw_buffer_[1] = byte2;
    raw_buffer_[2] = byte3;
  }

  void set_velocity (midi_byte_t vel);

  void print () const;

public:
  /** Raw MIDI data. */
  std::array<midi_byte_t, 3> raw_buffer_{ 0, 0, 0 };

  uint_fast8_t raw_buffer_sz_ = 0;

  /** Time of the MIDI event, in frames from the start of the current cycle. */
  midi_time_t time_ = 0;

  /** Time using g_get_monotonic_time(). */
  std::int64_t systime_ = 0;
};

inline bool
operator== (const MidiEvent &lhs, const MidiEvent &rhs)
{
  return lhs.time_ == rhs.time_ && lhs.raw_buffer_[0] == rhs.raw_buffer_[0]
         && lhs.raw_buffer_[1] == rhs.raw_buffer_[1]
         && lhs.raw_buffer_[2] == rhs.raw_buffer_[2]
         && lhs.raw_buffer_sz_ == rhs.raw_buffer_sz_;
}

/**
 * @brief A lock-free thread-safe vector of `MidiEvent`s.
 *
 * Not necessarily the best implementation, but it's good enough for now.
 */
class MidiEventVector final
{
public:
  MidiEventVector () { events_.reserve (MAX_MIDI_EVENTS); }

  using ChordDescriptor = dsp::ChordDescriptor;

  // Iterator types
  using Iterator = std::vector<MidiEvent>::iterator;
  using ConstIterator = std::vector<MidiEvent>::const_iterator;

  // Iterator methods
  Iterator begin ()
  {
    SemaphoreRAII<decltype (lock_)> lock (lock_);
    return events_.begin ();
  }

  Iterator end ()
  {
    SemaphoreRAII<decltype (lock_)> lock (lock_);
    return events_.end ();
  }

  void erase (Iterator it, Iterator it_end)
  {
    SemaphoreRAII<decltype (lock_)> lock (lock_);
    events_.erase (it, it_end);
  }

  ConstIterator begin () const
  {
    SemaphoreRAII<decltype (lock_)> lock (lock_);
    return events_.begin ();
  }

  ConstIterator end () const
  {
    SemaphoreRAII<decltype (lock_)> lock (lock_);
    return events_.end ();
  }

  void push_back (const MidiEvent &ev)
  {
    SemaphoreRAII<decltype (lock_)> lock (lock_);
    events_.push_back (ev);
  }

  void push_back (const std::vector<MidiEvent> &events)
  {
    SemaphoreRAII<decltype (lock_)> lock (lock_);
    events_.insert (events_.end (), events.begin (), events.end ());
  }

  MidiEvent pop_front ()
  {
    SemaphoreRAII<decltype (lock_)> lock (lock_);
    MidiEvent                       ev = events_.front ();
    events_.erase (events_.begin ());
    return ev;
  }

  MidiEvent pop_back ()
  {
    SemaphoreRAII<decltype (lock_)> lock (lock_);
    MidiEvent                       ev = events_.back ();
    events_.pop_back ();
    return ev;
  }

  void clear ()
  {
    SemaphoreRAII<decltype (lock_)> lock (lock_);
    events_.clear ();
  }

  size_t size () const
  {
    SemaphoreRAII<decltype (lock_)> lock (lock_);
    return events_.size ();
  }

  MidiEvent front () const
  {
    SemaphoreRAII<decltype (lock_)> lock (lock_);
    return events_.front ();
  }

  MidiEvent back () const
  {
    SemaphoreRAII<decltype (lock_)> lock (lock_);
    return events_.back ();
  }

  MidiEvent at (size_t index) const
  {
    SemaphoreRAII<decltype (lock_)> lock (lock_);
    return events_.at (index);
  }

  void swap (MidiEventVector &other)
  {
    SemaphoreRAII<decltype (lock_)> lock (lock_);
    events_.swap (other.events_);
  }

  void remove_if (std::function<bool (const MidiEvent &)> predicate)
  {
    SemaphoreRAII<decltype (lock_)> lock (lock_);
    std::erase_if (events_, std::move (predicate));
  }

  /**
   * @brief Removes all events that match @p event.
   */
  void remove (const MidiEvent &event)
  {
    remove_if ([&event] (const MidiEvent &e) { return e == event; });
  }

  void foreach_event (std::function<void (const MidiEvent &)> func) const
  {
    SemaphoreRAII<decltype (lock_)> lock (lock_);
    std::ranges::for_each (events_, func);
  }

  size_t capacity () const
  {
    SemaphoreRAII<decltype (lock_)> lock (lock_);
    return events_.capacity ();
  }

  void print () const;

  /**
   * Appends the events from @p src.
   *
   * @param channels Allowed channels (array of 16 booleans).
   * @param local_offset The local offset from 0 in this cycle.
   * @param nframes Number of frames to process.
   */
  [[gnu::hot]] void append_w_filter (
    const MidiEventVector              &src,
    std::optional<std::array<bool, 16>> channels,
    nframes_t                           local_offset,
    nframes_t                           nframes);

  /**
   * Appends the events from @p src.
   *
   * @param local_offset The start frame offset from 0 in this cycle.
   * @param nframes Number of frames to process.
   */
  void
  append (const MidiEventVector &src, nframes_t local_offset, nframes_t nframes);

  /**
   * Transforms the given MIDI input to the MIDI notes of the corresponding
   * chord.
   *
   * Only C0~B0 are considered.
   */
  void transform_chord_and_append (
    MidiEventVector &src,
    std::function<const ChordDescriptor *(midi_byte_t)>
                note_number_to_chord_descriptor,
    midi_byte_t velocity_to_use,
    nframes_t   local_offset,
    nframes_t   nframes);

  /**
   * Adds a note on event to the given MidiEvents.
   *
   * @param channel MIDI channel starting from 1.
   * @param queued Add to queued events instead.
   */
  void add_note_on (
    midi_byte_t channel,
    midi_byte_t note_pitch,
    midi_byte_t velocity,
    midi_time_t time);

  /**
   * Adds a note on for each note in the chord.
   */
  void add_note_ons_from_chord_descr (
    const ChordDescriptor &descr,
    midi_byte_t            channel,
    midi_byte_t            velocity,
    midi_time_t            time);

  /**
   * Adds a note off for each note in the chord.
   */
  void add_note_offs_from_chord_descr (
    const ChordDescriptor &descr,
    midi_byte_t            channel,
    midi_time_t            time);

  /**
   * Add CC volume event.
   *
   * TODO
   */
  void
  add_cc_volume (midi_byte_t channel, midi_byte_t volume, midi_time_t time);

#if 0
  /**
   * Returrns if the MidiEvents have any note on events.
   *
   * @param check_main Check the main events.
   * @param check_queued Check the queued events.
   */
  bool has_note_on (bool check_main, bool check_queued);
#endif

  bool has_any () const { return !empty (); }
  bool empty () const { return size () == 0; }

  /**
   * Parses a MidiEvent from a raw MIDI buffer.
   *
   * This must be a full 3-byte message. If in 'running status' mode, the
   * caller is responsible for prepending the status byte.
   */
  void add_event_from_buf (midi_time_t time, midi_byte_t * buf, int buf_size);

  /**
   * Adds a note off event to the given MidiEvents.
   *
   * @param channel MIDI channel starting from 1.
   * @param queued Add to queued events instead.
   */
  void
  add_note_off (midi_byte_t channel, midi_byte_t note_pitch, midi_time_t time);

  /**
   * Adds a control event to the given MidiEvents.
   *
   * @param channel MIDI channel starting from 1.
   */
  void add_control_change (
    midi_byte_t channel,
    midi_byte_t controller,
    midi_byte_t control,
    midi_time_t time);

  /**
   * Adds a song position event to the queue.
   *
   * @param total_sixteenths Total sixteenths.
   */
  void add_song_pos (int64_t total_sixteenths, midi_time_t time);

  void add_raw (uint8_t * buf, size_t buf_sz, midi_time_t time);

  void add_simple (
    midi_byte_t byte1,
    midi_byte_t byte2,
    midi_byte_t byte3,
    midi_time_t time)
  {
    push_back (MidiEvent (byte1, byte2, byte3, time));
  }

  /**
   * Adds a control event to the given MidiEvents.
   *
   * @param channel MIDI channel starting from 1.
   * @param pitchbend 0 to 16384.
   */
  void
  add_pitchbend (midi_byte_t channel, uint32_t pitchbend, midi_time_t time);

  void
  add_channel_pressure (midi_byte_t channel, midi_byte_t value, midi_time_t time);

  /**
   * Queues MIDI note off to event queue.
   */
  void
  add_all_notes_off (midi_byte_t channel, midi_time_t time, bool with_lock);

  /**
   * Adds a note off message to every MIDI channel.
   */
  void panic_without_lock ()
  {
    for (midi_byte_t i = 1; i < 17; i++)
      {
        add_all_notes_off (i, 0, false);
      }
  }

  /**
   * Must only be called from the UI thread.
   */
  void panic ();

  void write_to_midi_file (MIDI_FILE * mf, int midi_track) const;

  /**
   * Clears duplicates.
   */
  void clear_duplicates ();

#if 0
  /**
   * Returns if a note on event for the given note exists in the given events.
   */
  bool check_for_note_on (int note);

  /**
   * Deletes the midi event with a note on signal from the queue, and returns if
   * it deleted or not.
   */
  bool delete_note_on (int note);
#endif

  /**
   * Sorts the MidiEvents by time.
   */
  void sort ();

  /**
   * Sets the given MIDI channel on all applicable
   * MIDI events.
   */
  void set_channel (midi_byte_t channel);

  void delete_event (const MidiEvent * ev);

private:
  std::vector<MidiEvent> events_;

  /** Spinlock for exclusive read/write. */
  mutable moodycamel::LightweightSemaphore lock_{ 1 };
};

/**
 * Container for passing midi events through ports.
 *
 * This should be passed in the data field of MIDI Ports.
 */
class MidiEvents final
{
public:
  /**
   * Copies the queue contents to the original struct
   *
   * @param local_offset The start frame offset from 0 in this cycle.
   * @param nframes Number of frames to process.
   */
  void dequeue (nframes_t local_offset, nframes_t nframes);

public:
  /** Events to use in this cycle. */
  MidiEventVector active_events_;

  /**
   * For queueing events from the GUI or from hardware, since they run in
   * different threads.
   *
   * Engine will copy them to the unqueued MIDI events when ready to be
   * processed.
   */
  MidiEventVector queued_events_;
};

} // namespace zrythm::dsp
