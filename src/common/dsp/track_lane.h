// SPDX-FileCopyrightText: © 2019-2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#pragma once

#include "common/dsp/midi_event.h"
#include "common/dsp/region.h"
#include "common/dsp/region_owner.h"

using MIDI_FILE = void;
class MidiLane;
class AudioLane;
class Tracklist;

template <typename TrackLaneT> class LanedTrackImpl;

/**
 * @addtogroup dsp
 *
 * @{
 */

#define DEFINE_TRACK_LANE_QML_PROPERTIES(ClassType) \
public: \
  /* ================================================================ */ \
  /* name */ \
  /* ================================================================ */ \
  Q_PROPERTY (QString name READ getName WRITE setName NOTIFY nameChanged) \
  QString getName () const \
  { \
    return QString::fromStdString (name_); \
  } \
  void setName (const QString &name) \
  { \
    auto std_name = name.toStdString (); \
    if (name_ == std_name) \
      return; \
\
    name_ = std_name; \
    Q_EMIT nameChanged (name); \
  } \
\
  Q_SIGNAL void nameChanged (const QString &name); \
  /* ================================================================ */ \
  /* height */ \
  /* ================================================================ */ \
  Q_PROPERTY (double height READ getHeight WRITE setHeight NOTIFY heightChanged) \
  double getHeight () const \
  { \
    return height_; \
  } \
  void setHeight (const double height) \
  { \
    if (math_doubles_equal(height_, height)) \
      return; \
\
    height_ = height; \
    Q_EMIT heightChanged (height); \
  } \
\
  Q_SIGNAL void heightChanged (double height);


constexpr double TRACK_LANE_DEF_HEIGHT = 48;

/**
 * A TrackLane belongs to a Track (can have many TrackLanes in a Track) and
 * contains Regions.
 *
 * Only Tracks that have Regions can have TrackLanes, such as InstrumentTrack
 * and AudioTrack.
 */
class TrackLane : virtual public RegionOwner
{
public:
  ~TrackLane () override = default;

  std::string get_name () const { return this->name_; }

  bool get_soloed () const { return solo_; }

  /**
   * @brief Returns if the lane is explicitly marked as muted.
   *
   * @note Doesn't check soloed state or track state.
   */
  bool get_muted () const { return this->mute_; }

protected:
  TrackLane () = default;
  TrackLane (int pos, std::string name);

public:
  /** Position in the Track. */
  int pos_ = 0;

  /** Name of lane, e.g. "Lane 1". */
  std::string name_;

  /** Position of handle. */
  double height_ = TRACK_LANE_DEF_HEIGHT;

  /** Muted or not. */
  bool mute_ = false;

  /** Soloed or not. */
  bool solo_ = false;

  /**
   * MIDI channel, if MIDI lane, starting at 1.
   *
   * If this is set to 0, the value will be inherited from the Track.
   */
  uint8_t midi_ch_ = 0;
};

/**
 * A TrackLane belongs to a Track (can have many TrackLanes in a Track) and
 * contains Regions.
 *
 * Only Tracks that have Regions can have TrackLanes, such as InstrumentTrack
 * and AudioTrack.
 */
template <typename RegionT>
class TrackLaneImpl
    : public TrackLane,
      public RegionOwnerImpl<RegionT>,
      public ISerializable<TrackLaneImpl<RegionT>>
{
public:
  using TrackLaneT =
    std::conditional_t<std::is_same_v<RegionT, MidiRegion>, MidiLane, AudioLane>;
  using LanedTrackT = LanedTrackImpl<TrackLaneT>;

public:
  TrackLaneImpl () = default;
  ~TrackLaneImpl () override = default;

  /**
   * Creates a new TrackLane at the given pos in the given Track.
   *
   * @param track The Track to create the TrackLane for.
   * @param pos The position (index) in the Track that this lane will be placed
   * in.
   */
  TrackLaneImpl (LanedTrackT * track, int pos);

  bool is_in_active_project () const override;

  bool is_auditioner () const override;

  void init_loaded (LanedTrackT * track);

  /**
   * Sets track lane soloed, updates UI and optionally
   * adds the action to the undo stack.
   *
   * @param trigger_undo Create and perform an
   *   undoable action.
   * @param fire_events Fire UI events.
   */
  void set_soloed (bool solo, bool trigger_undo, bool fire_events);

  /**
   * Sets track lane muted, updates UI and optionally
   * adds the action to the undo stack.
   *
   * @param trigger_undo Create and perform an
   *   undoable action.
   * @param fire_events Fire UI events.
   */
  void set_muted (bool mute, bool trigger_undo, bool fire_events);

  /**
   * @brief Returns if the lane is effectively muted (explicitly or implicitly
   * muted).
   */
  bool is_effectively_muted () const;

  /**
   * Rename the lane.
   *
   * @param with_action Whether to make this an
   *   undoable action.
   */
  void rename (const std::string &new_name, bool with_action);

  /**
   * Wrapper over track_lane_rename().
   */
  void rename_with_action (const std::string &new_name);

  /**
   * Unselects all arranger objects.
   */
  void unselect_all ();

  /**
   * Sets the new track name hash to all the lane's objects recursively.
   */
  void update_track_name_hash ();

  /**
   * Writes the lane to the given MIDI file.
   *
   * @param lanes_as_tracks Export lanes as separate MIDI tracks.
   * @param use_track_or_lane_pos Whether to use the track position (or lane
   * position if @ref lanes_as_tracks is true) in the MIDI data. The MIDI track
   * will be set to 1 if false.
   * @param events Track events, if not using lanes as tracks.
   * @param start Events before this position will be skipped.
   * @param end Events after this position will be skipped.
   */
  void write_to_midi_file (
    MIDI_FILE *       mf,
    MidiEventVector * events,
    const Position *  start,
    const Position *  end,
    bool              lanes_as_tracks,
    bool              use_track_or_lane_pos)
    requires std::derived_from<MidiRegion, RegionT>;

  Tracklist * get_tracklist () const;

  LanedTrackT * get_track () const
  {
    z_return_val_if_fail (track_, nullptr);
    return track_;
  }

  /**
   * Calculates a unique index for this lane.
   */
  int calculate_lane_idx () const;

  /**
   * Generate a snapshot for playback.
   */
  std::unique_ptr<TrackLaneT> gen_snapshot () const;

  protected:
    void copy_members_from (const TrackLaneImpl &other);

    DECLARE_DEFINE_BASE_FIELDS_METHOD();

  private:
    void after_remove_region () final;

  public:
    /** Owner track. */
    LanedTrackT * track_ = nullptr;

    static_assert(FinalRegionSubclass<RegionT>);
};

using TrackLaneVariant =
  std::variant<MidiLane, AudioLane>;
using TrackLanePtrVariant = to_pointer_variant<TrackLaneVariant>;

template <typename T>
concept TrackLaneSubclass = std::derived_from<T, TrackLane>;

extern template class TrackLaneImpl<MidiRegion>;
extern template class TrackLaneImpl<AudioRegion>;

/**
 * @}
 */
