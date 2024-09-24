// SPDX-FileCopyrightText: © 2019-2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#ifndef __AUDIO_SAMPLE_PROCESSOR_H__
#define __AUDIO_SAMPLE_PROCESSOR_H__

#include "dsp/fader.h"
#include "dsp/graph.h"
#include "dsp/metronome.h"
#include "dsp/midi_event.h"
#include "dsp/port.h"
#include "dsp/position.h"
#include "dsp/sample_playback.h"
#include "dsp/tracklist.h"
#include "settings/plugin_settings.h"
#include "utils/types.h"

class FileDescriptor;
class ChordPreset;
class AudioEngine;

/**
 * @addtogroup dsp
 *
 * @{
 */

#define SAMPLE_PROCESSOR (AUDIO_ENGINE->sample_processor_)

/**
 * A processor to be used in the routing graph for playing samples independent
 * of the timeline.
 *
 * Also used for auditioning files.
 */
class SampleProcessor final
    : public ICloneable<SampleProcessor>,
      public ISerializable<SampleProcessor>
{
public:
  SampleProcessor () = default;
  SampleProcessor (AudioEngine * engine);
  ~SampleProcessor ();

  bool is_in_active_project () const;

  void init_after_cloning (const SampleProcessor &other) override;

  void init_loaded (AudioEngine * engine);

  /**
   * @brief Loads the instrument from the settings.
   *
   * To be called when the engine is activated, once.
   */
  void load_instrument_if_empty ();

  /**
   * Clears the buffers.
   */
  void prepare_process (const nframes_t nframes);

  /**
   * Process the samples for the given number of frames.
   *
   * @param offset The local offset in the processing cycle.
   * @param nframes The number of frames to process in this call.
   */
  void process (const nframes_t offset, const nframes_t nframes);

  /**
   * Removes a SamplePlayback from the array.
   */
  void remove_sample_playback (SamplePlayback &sp);

  /**
   * Queues a metronomem tick at the given offset.
   *
   * Used for countin.
   */
  void queue_metronome_countin ();

  /**
   * Queues a metronomem tick at the given local
   * offset.
   *
   * Realtime function.
   */
  void queue_metronome (Metronome::Type type, nframes_t offset);

  /**
   * Adds a sample to play to the queue from a file path.
   */
  void queue_sample_from_file (const char * path);

  /**
   * Adds a file (audio or MIDI) to the queue.
   */
  void queue_file (const FileDescriptor &file);

  /**
   * Adds a chord preset to the queue.
   */
  void queue_chord_preset (const ChordPreset &chord_pset);

  /**
   * Stops playback of files (auditioning).
   */
  void stop_file_playback ();

  void disconnect ();

  /**
   * Finds all metronome events (beat and bar changes)
   * within the given range and adds them to the
   * queue.
   *
   * @param end_pos End position, exclusive.
   * @param loffset Local offset (this is where @p start_pos starts at).
   */
  void find_and_queue_metronome (
    const Position  start_pos,
    const Position  end_pos,
    const nframes_t loffset);

  DECLARE_DEFINE_FIELDS_METHOD ();

private:
  void init_common ();

  void queue_file_or_chord_preset (
    const FileDescriptor * file,
    const ChordPreset *    chord_pset);

public:
  /** An array of samples currently being played. */
  std::vector<SamplePlayback> current_samples_;

  /** Tracklist for file auditioning. */
  std::unique_ptr<Tracklist> tracklist_;

  /** Instrument for MIDI auditioning. */
  std::unique_ptr<PluginSetting> instrument_setting_;

  std::unique_ptr<MidiEvents> midi_events_;

  /** Fader connected to the main output. */
  std::unique_ptr<Fader> fader_;

  /** Playhead for the tracklist (used when auditioning files). */
  Position playhead_;

  /**
   * Position the file ends at.
   *
   * Once this position is reached,
   * SampleProcessor.roll will be set to false.
   */
  Position file_end_pos_;

  /** Whether to roll or not. */
  bool roll_ = false;

  /** Pointer to owner audio engin, if any. */
  AudioEngine * audio_engine_ = nullptr;

  /** Temp processing graph. */
  std::unique_ptr<Graph> graph_;

  /** Semaphore to be locked while rebuilding the sample processor tracklist and
   * graph. */
  std::binary_semaphore rebuilding_sem_{ 1 };
};

/**
 * @}
 */

#endif
