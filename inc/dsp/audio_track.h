
// SPDX-FileCopyrightText: © 2018-2020, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#ifndef __AUDIO_AUDIO_TRACK_H__
#define __AUDIO_AUDIO_TRACK_H__

#include "dsp/audio_region.h"
#include "dsp/automatable_track.h"
#include "dsp/channel_track.h"
#include "dsp/laned_track.h"
#include "dsp/recordable_track.h"

struct Stretcher;

/**
 * The AudioTrack class represents an audio track in the project. It
 * inherits from ChannelTrack, LanedTrack, and AutomatableTrack, providing
 * functionality for managing audio channels, lanes, and automation.
 */
class AudioTrack final
    : public ChannelTrack,
      public LanedTrackImpl<AudioRegion>,
      public RecordableTrack,
      public ICloneable<AudioTrack>,
      public ISerializable<AudioTrack>,
      public InitializableObjectFactory<AudioTrack>
{
  friend class InitializableObjectFactory<AudioTrack>;

public:
  // Uncopyable
  AudioTrack (const AudioTrack &) = delete;
  AudioTrack &operator= (const AudioTrack &) = delete;

  ~AudioTrack ();

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

  void clear_objects () override
  {
    LanedTrackImpl::clear_objects ();
    AutomatableTrack::clear_objects ();
  }

  bool validate () const override
  {
    return LanedTrackImpl::validate () && RecordableTrack::validate ()
           && ChannelTrack::validate ();
  }

  void get_regions_in_range (
    std::vector<Region *> &regions,
    const Position *       p1,
    const Position *       p2) override
  {
    LanedTrackImpl::get_regions_in_range (regions, p1, p2);
    AutomatableTrack::get_regions_in_range (regions, p1, p2);
  }

  DECLARE_DEFINE_FIELDS_METHOD ();

private:
  AudioTrack () = default;
  AudioTrack (const std::string &name, int pos, unsigned int samplerate);

  bool initialize () override;

  void set_playback_caches () override;

  void update_name_hash (NameHashT new_name_hash) override;

public:
  /** Real-time time stretcher. */
  Stretcher * rt_stretcher_ = nullptr;

private:
  /**
   * The samplerate @ref rt_stretcher_ is working with.
   *
   * Should be initialized with the samplerate of the audio engine.
   *
   * Not to be serialized.
   */
  unsigned int samplerate_ = 0;
};

void
audio_track_init (Track * track);

/**
 * Fills the buffers in the given StereoPorts with the frames from the current
 * clip.
 */
void
audio_track_fill_stereo_ports_from_clip (
  StereoPorts &stereo_ports,
  const long   g_start_frames,
  nframes_t    nframes);

#endif // __AUDIO_TRACK_H__
