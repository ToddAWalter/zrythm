// SPDX-FileCopyrightText: © 2019-2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#ifndef __AUDIO_MIDI_REGION_H__
#define __AUDIO_MIDI_REGION_H__

#include "gui/dsp/arranger_object_owner.h"
#include "gui/dsp/lane_owned_object.h"
#include "gui/dsp/midi_note.h"
#include "gui/dsp/region.h"

using namespace zrythm;

class Track;
class MidiNote;
class MidiEventVector;
namespace zrythm::dsp
{
class ChordDescriptor;
}
class Velocity;
using MIDI_FILE = void;

/**
 * @addtogroup dsp
 *
 * @{
 */

/**
 * @brief A Region containing MIDI events.
 *
 * MidiRegion represents a region in the timeline that holds MIDI note and
 * controller data. It is specific to instrument/MIDI tracks and can be
 * constructed from a MIDI file or a chord descriptor.
 */
class MidiRegion final
    : public QObject,
      public LaneOwnedObject,
      public RegionImpl<MidiRegion>,
      public ArrangerObjectOwner<MidiNote>,
      public ICloneable<MidiRegion>,
      public zrythm::utils::serialization::ISerializable<MidiRegion>
{
  Q_OBJECT
  QML_ELEMENT
  DEFINE_ARRANGER_OBJECT_QML_PROPERTIES (MidiRegion)
  DEFINE_REGION_QML_PROPERTIES (MidiRegion)
  DEFINE_ARRANGER_OBJECT_OWNER_QML_PROPERTIES (MidiRegion, midiNotes, MidiNote)

  friend class RegionImpl<MidiRegion>;
  friend class ArrangerObjectFactory;

public:
  using ChordDescriptor = zrythm::dsp::ChordDescriptor;

public:
  DECLARE_FINAL_ARRANGER_OBJECT_CONSTRUCTORS (MidiRegion)

  /**
   * Starts an unended note with the given pitch and velocity and adds it to
   * @ref midi_notes_.
   *
   * @param end_pos If this is nullptr, it will be set to 1 tick after the
   * start_pos.
   */
  void start_unended_note (
    Position * start_pos,
    Position * end_pos,
    int        pitch,
    int        vel,
    bool       pub_events);

  /**
   * Returns the midi note with the given pitch from the unended notes.
   *
   * Used when recording.
   *
   * @param pitch The pitch. If -1, it returns any unended note. This is useful
   * when the loop point is met and we want to end them all.
   */
  MidiNote * pop_unended_note (int pitch);

  /**
   * Prints the MidiNotes in the Region.
   *
   * Used for debugging.
   */
  void print_midi_notes () const;

  /**
   * Gets first midi note
   */
  MidiNote * get_first_midi_note ();

  /**
   * Gets last midi note
   */
  MidiNote * get_last_midi_note ();

  /**
   * Gets highest midi note
   */
  MidiNote * get_highest_midi_note ();

  /**
   * Gets lowest midi note
   */
  MidiNote * get_lowest_midi_note ();

  /**
   * Exports the Region to an existing MIDI file instance.
   *
   * This must only be called when exporting single regions.
   *
   * @param add_region_start Add the region start offset to the positions.
   * @param export_full Traverse loops and export the MIDI file as it would be
   * played inside Zrythm. If this is 0, only the original region (from true
   * start to true end) is exported.
   * @param lanes_as_tracks Export lanes as separate tracks (only possible with
   * MIDI type 1). This will calculate a unique MIDI track number for the
   * region's lane.
   * @param use_track_or_lane_pos Whether to use the track/lane position in the
   * MIDI data. The MIDI track will be set to 1 if false.
   */
  void
  write_to_midi_file (MIDI_FILE * mf, bool add_region_start, bool export_full)
    const;

  /**
   * Exports the Region to a specified MIDI file.
   *
   * @param full_path Absolute path to the MIDI file.
   * @param export_full Traverse loops and export the
   *   MIDI file as it would be played inside Zrythm.
   *   If this is 0, only the original region (from
   *   true start to true end) is exported.
   */
  void export_to_midi_file (
    const std::string &full_path,
    int                midi_version,
    bool               export_full) const;

  /**
   * Returns the MIDI channel that this region should be played on, starting
   * from 1.
   */
  uint8_t get_midi_ch () const;

  /**
   * Returns whether the given note is not muted and starts within any
   * playable part of the region.
   */
  bool is_note_playable (const MidiNote &midi_note) const;

  /**
   * Adds the contents of the region converted into events.
   *
   * @param add_region_start Add the region start offset to the
   *   positions.
   * @param export_full Traverse loops and export the MIDI file
   *   as it would be played inside Zrythm. If this is 0, only
   *   the original region (from true start to true end) is
   *   exported.
   * @param start Events before this (global) position will be skipped.
   * @param end Events after this (global) position will be skipped.
   */
  void add_events (
    MidiEventVector &events,
    const Position * start,
    const Position * end,
    bool             add_region_start,
    bool             full) const;

  /**
   * Fills in the array with all the velocities in the project that are within
   * or outside the range given.
   *
   * @param inside Whether to find velocities inside the range (true) or
   * outside (false).
   */
  void get_velocities_in_range (
    const Position *         start_pos,
    const Position *         end_pos,
    std::vector<Velocity *> &velocities,
    bool                     inside);

  bool
  validate (bool is_project, dsp::FramesPerTick frames_per_tick) const override;

  void init_after_cloning (const MidiRegion &other, ObjectCloneType clone_type)
    override;

  /**
   * Set positions to the exact values in the export region as it is played
   * inside Zrythm.
   *
   * @param[in,out] start_pos start position of the event
   * @param[in,out] end_pos end position of the event
   * @param repeat_index repetition counter for loop offset
   */
  void get_note_positions_in_export (
    Position * start_pos,
    Position * end_pos,
    int        repeat_index) const;

  /**
   * Returns if the given positions are in a given
   * region as it is played inside Zrythm.
   *
   * @param offset_in_ticks Offset value if note is
   * repeated inside a loop
   */
  bool is_note_export_start_pos_in_full_region (Position start_pos) const;

  Location get_location (const MidiNote &) const override
  {
    return { .track_id_ = track_id_, .owner_ = get_uuid () };
  }

  std::string get_field_name_for_serialization (const MidiNote *) const override
  {
    return "midiNotes";
  }

  DECLARE_DEFINE_FIELDS_METHOD ();

public:
  /**
   * Unended notes started in recording with MIDI NOTE ON signal but haven't
   * received a NOTE OFF yet.
   *
   * This is also used temporarily when reading from MIDI files.
   *
   * @note These are present in @ref midi_notes_ and must not be deleted
   * separately.
   */
  std::vector<MidiNote *> unended_notes_;
};

inline bool
operator== (const MidiRegion &lhs, const MidiRegion &rhs)
{
  return static_cast<const Region &> (lhs) == static_cast<const Region &> (rhs)
         && static_cast<const TimelineObject &> (lhs)
              == static_cast<const TimelineObject &> (rhs)
         && static_cast<const NamedObject &> (lhs)
              == static_cast<const NamedObject &> (rhs)
         && static_cast<const LoopableObject &> (lhs)
              == static_cast<const LoopableObject &> (rhs)
         && static_cast<const ColoredObject &> (lhs)
              == static_cast<const ColoredObject &> (rhs)
         && static_cast<const MuteableObject &> (lhs)
              == static_cast<const MuteableObject &> (rhs)
         && static_cast<const BoundedObject &> (lhs)
              == static_cast<const BoundedObject &> (rhs)
         && static_cast<const ArrangerObject &> (lhs)
              == static_cast<const ArrangerObject &> (rhs);
}

DEFINE_OBJECT_FORMATTER (MidiRegion, MidiRegion, [] (const MidiRegion &mr) {
  return fmt::format (
    "MidiRegion[id: {}, {}]", mr.get_uuid (), static_cast<const Region &> (mr));
})

/**
 * @brief Generates a filename for the given MIDI region.
 *
 * @param track The track that owns the region.
 * @param mr The MIDI region.
 * @return constexpr std::string
 */
constexpr std::string
generate_filename_for_midi_region (const auto &track, const MidiRegion &mr)
{
  constexpr const char * REGION_PRINTF_FILENAME = "{}_{}.mid";
  return fmt::format (REGION_PRINTF_FILENAME, track.get_name (), mr.get_name ());
}

/**
 * @}
 */

#endif // __AUDIO_MIDI_REGION_H__
