// SPDX-FileCopyrightText: © 2019-2021, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#ifndef __AUDIO_MIDI_TRACK_H__
#define __AUDIO_MIDI_TRACK_H__

#include "dsp/automatable_track.h"
#include "dsp/channel_track.h"
#include "dsp/piano_roll_track.h"
#include "utils/object_factory.h"

/**
 * @addtogroup dsp
 *
 * @{
 */

class MidiTrack final
    : public PianoRollTrack,
      public ChannelTrack,
      public ICloneable<MidiTrack>,
      public ISerializable<MidiTrack>,
      public InitializableObjectFactory<MidiTrack>
{
  friend class InitializableObjectFactory<MidiTrack>;

public:
  void init_loaded () override
  {
    PianoRollTrack::init_loaded ();
    ChannelTrack::init_loaded ();
  }

  bool validate () const override
  {
    return ChannelTrack::validate () && PianoRollTrack::validate ();
  }

  void init_after_cloning (const MidiTrack &other) override
  {
    PianoRollTrack::copy_members_from (other);
    ChannelTrack::copy_members_from (other);
    ProcessableTrack::copy_members_from (other);
    AutomatableTrack::copy_members_from (other);
    RecordableTrack::copy_members_from (other);
    LanedTrackImpl::copy_members_from (other);
  }

private:
  MidiTrack () = default;
  MidiTrack (const std::string &label, int pos);

  bool initialize () override;

  DECLARE_DEFINE_FIELDS_METHOD ();
};

/**
 * @}
 */

#endif // __AUDIO_MIDI_TRACK_H__
