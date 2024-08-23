// SPDX-FileCopyrightText: © 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#ifndef __AUDIO_PROCESSABLE_TRACK_H__
#define __AUDIO_PROCESSABLE_TRACK_H__

#include "dsp/automatable_track.h"
#include "dsp/track_processor.h"

class TrackProcessor;

/**
 * The ProcessableTrack class is the base class for all processable
 * tracks.
 *
 * @ref processor_ is the starting point when processing a Track.
 */
class ProcessableTrack
    : virtual public AutomatableTrack,
      public ISerializable<ProcessableTrack>
{
public:
  // Rule of 0
  ProcessableTrack ();

  virtual ~ProcessableTrack () = default;

  void init_loaded () override;

  /**
   * Returns whether monitor audio is on.
   */
  bool get_monitor_audio () const;

  /**
   * Sets whether monitor audio is on.
   */
  void set_monitor_audio (bool monitor, bool auto_select, bool fire_events);

  /**
   * Wrapper for MIDI/instrument/chord tracks to fill in MidiEvents from the
   * timeline data.
   *
   * @note The engine splits the cycle so transport loop related logic is not
   * needed.
   *
   * @param midi_events MidiEvents to fill.
   */
  void fill_midi_events (
    const EngineProcessTimeInfo &time_nfo,
    MidiEventVector             &midi_events);

protected:
  /**
   * Common logic for audio and MIDI/instrument tracks to fill in MidiEvents or
   * StereoPorts from the timeline data.
   *
   * @note The engine splits the cycle so transport loop related logic is not
   * needed.
   *
   * @param stereo_ports StereoPorts to fill.
   * @param midi_events MidiEvents to fill (from Piano Roll Port for example).
   */
  void fill_events_common (
    const EngineProcessTimeInfo &time_nfo,
    MidiEventVector *            midi_events,
    StereoPorts *                stereo_ports) const;

  void copy_members_from (const ProcessableTrack &other)
  {
    processor_ = other.processor_->clone_unique ();
    processor_->track_ = this;
  }

  DECLARE_DEFINE_BASE_FIELDS_METHOD ();

public:
  /**
   * The TrackProcessor, used for processing.
   *
   * This is the starting point when processing a Track.
   */
  std::unique_ptr<TrackProcessor> processor_;
};

#endif // __AUDIO_PROCESSABLE_TRACK_H__
