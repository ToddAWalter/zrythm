// SPDX-FileCopyrightText: © 2019-2020, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#ifndef __AUDIO_MARKER_TRACK_H__
#define __AUDIO_MARKER_TRACK_H__

#include "common/dsp/marker.h"
#include "common/dsp/track.h"

/**
 * @addtogroup dsp
 *
 * @{
 */

#define P_MARKER_TRACK (TRACKLIST->marker_track_)

class MarkerTrack final
    : public QObject,
      public Track,
      public ICloneable<MarkerTrack>,
      public ISerializable<MarkerTrack>,
      public InitializableObjectFactory<MarkerTrack>
{
  Q_OBJECT
  QML_ELEMENT
  DEFINE_TRACK_QML_PROPERTIES

  friend class InitializableObjectFactory<MarkerTrack>;

public:
  using MarkerPtr = std::shared_ptr<Marker>;

  void init_loaded () override;

  /**
   * @brief Adds the start/end markers.
   */
  void add_default_markers (const Transport &transport);

  /**
   * Inserts a marker to the track.
   */
  MarkerPtr insert_marker (MarkerPtr marker, int pos);

  /**
   * Adds a marker to the track.
   */
  MarkerPtr add_marker (MarkerPtr marker)
  {
    return insert_marker (marker, markers_.size ());
  }

  /**
   * Removes all objects from the marker track.
   *
   * Mainly used in testing.
   */
  void clear_objects () override;

  /**
   * Removes a marker.
   */
  MarkerPtr remove_marker (Marker &marker, bool fire_events);

  bool validate () const override;

  /**
   * Returns the start marker.
   */
  MarkerPtr get_start_marker () const;

  /**
   * Returns the end marker.
   */
  MarkerPtr get_end_marker () const;

  void init_after_cloning (const MarkerTrack &other) override;

  DECLARE_DEFINE_FIELDS_METHOD ();

private:
  MarkerTrack (int track_pos = 0);

  bool initialize () override;
  void set_playback_caches () override;

public:
  std::vector<MarkerPtr> markers_;

  /** Snapshots used during playback TODO unimplemented. */
  std::vector<std::unique_ptr<Marker>> marker_snapshots_;
};

/**
 * @}
 */

#endif
