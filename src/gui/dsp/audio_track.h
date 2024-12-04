// SPDX-FileCopyrightText: © 2018-2020, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#ifndef __AUDIO_AUDIO_TRACK_H__
#define __AUDIO_AUDIO_TRACK_H__

#include "dsp/stretcher.h"
#include "gui/dsp/audio_region.h"
#include "gui/dsp/automatable_track.h"
#include "gui/dsp/channel_track.h"
#include "gui/dsp/laned_track.h"
#include "gui/dsp/recordable_track.h"

/**
 * The AudioTrack class represents an audio track in the project. It
 * inherits from ChannelTrack, LanedTrack, and AutomatableTrack, providing
 * functionality for managing audio channels, lanes, and automation.
 */
class AudioTrack final
    : public QObject,
      public ChannelTrack,
      public LanedTrackImpl<AudioLane>,
      public RecordableTrack,
      public ICloneable<AudioTrack>,
      public zrythm::utils::serialization::ISerializable<AudioTrack>,
      public InitializableObjectFactory<AudioTrack>
{
  Q_OBJECT
  QML_ELEMENT
  DEFINE_TRACK_QML_PROPERTIES (AudioTrack)
  DEFINE_LANED_TRACK_QML_PROPERTIES (AudioTrack)
  DEFINE_AUTOMATABLE_TRACK_QML_PROPERTIES (AudioTrack)
  DEFINE_CHANNEL_TRACK_QML_PROPERTIES (AudioTrack)

  friend class InitializableObjectFactory<AudioTrack>;

public:
  void init_loaded () override;

  void init_after_cloning (const AudioTrack &other) override;

  /**
   * Wrapper for audio tracks to fill in StereoPorts from the timeline data.
   *
   * @note The engine splits the cycle so transport loop related logic is not
   * needed.
   *
   * @param stereo_ports StereoPorts to fill.
   */
  void
  fill_events (const EngineProcessTimeInfo &time_nfo, StereoPorts &stereo_ports);

  void clear_objects () override;

  bool validate () const override;

  void get_regions_in_range (
    std::vector<Region *> &regions,
    const Position *       p1,
    const Position *       p2) override;

  void
  append_ports (std::vector<Port *> &ports, bool include_plugins) const final;

  void timestretch_buf (
    const AudioRegion * r,
    AudioClip *         clip,
    unsigned_frame_t    in_frame_offset,
    double              timestretch_ratio,
    float *             lbuf_after_ts,
    float *             rbuf_after_ts,
    unsigned_frame_t    out_frame_offset,
    unsigned_frame_t    frames_to_process);

  DECLARE_DEFINE_FIELDS_METHOD ();

private:
  AudioTrack (
    const std::string &name = "",
    int                pos = 0,
    unsigned int       samplerate = 44000);

  bool initialize () override;

  void set_playback_caches () override;

  void update_name_hash (NameHashT new_name_hash) override;

private:
  /** Real-time time stretcher. */
  std::unique_ptr<zrythm::dsp::Stretcher> rt_stretcher_;

  /**
   * The samplerate @ref rt_stretcher_ is working with.
   *
   * Should be initialized with the samplerate of the audio engine.
   *
   * Not to be serialized.
   */
  unsigned int samplerate_ = 0;
};

#endif // __AUDIO_TRACK_H__