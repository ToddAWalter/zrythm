// SPDX-FileCopyrightText: © 2020-2021, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#ifndef __IO_MIDI_FILE_H__
#define __IO_MIDI_FILE_H__

#include "zrythm-config.h"

#include <filesystem>

#include "utils/types.h"

#include "juce_wrapper.h"

namespace zrythm::structure::arrangement
{
class MidiRegion;
}

/**
 * @addtogroup io
 *
 * @{
 */

/**
 * @brief MIDI file handling.
 */
class MidiFile
{
public:
  enum class Format
  {
    MIDI0,
    MIDI1,
    MIDI2,
  };

  using TrackIndex = unsigned int;

  /**
   * @brief Construct a new Midi File object for writing.
   */
  MidiFile (Format format);

  /**
   * @brief Construct a new Midi File object for reading.
   *
   * @param path Path to read.
   * @throw ZrythmException If the file could not be read.
   */
  MidiFile (const fs::path &path);

  /**
   * Returns whether the given track in the midi file has data.
   */
  bool track_has_midi_note_events (TrackIndex track_idx) const;

  /**
   * Returns the number of tracks in the MIDI file.
   */
  int get_num_tracks (bool non_empty_only) const;

  [[nodiscard]] Format get_format () const;

  /**
   * @brief Get the PPQN (Parts Per Quarter Note) of the MIDI file.
   *
   * @return int
   * @throw ZrythmException If the MIDI file does not contain a PPQN value.
   */
  int get_ppqn () const;

  /**
   * @brief Reads the contents of the MIDI file into a region.
   *
   * @param region A freshly created region to fill.
   * @param midi_track_idx The index of this track, starting from 0. This will
   * be sequential, ie, if idx 1 is requested and the MIDI file only has tracks
   * 5 and 7, it will use track 7.
   * @throw ZrythmException On error.
   */
  void
  into_region (structure::arrangement::MidiRegion &region, int midi_track_idx)
    const;

  /**
   * Exports the Region to a specified MIDI file.
   *
   * FIXME: this needs refactoring. taken out of MidiRegion class.
   *
   * @param full_path Absolute path to the MIDI file.
   * @param export_full Traverse loops and export the MIDI file as it would be
   * played inside Zrythm. If this is false, only the original region (from true
   * start to true end) is exported.
   */
  static void export_midi_region_to_midi_file (
    const structure::arrangement::MidiRegion &region,
    const fs::path                           &full_path,
    int                                       midi_version,
    bool                                      export_full);

private:
  juce::MidiFile midi_file_;
  Format         format_ = Format::MIDI0;
  bool           for_reading_ = false;
};

/**
 * @}
 */

#endif
