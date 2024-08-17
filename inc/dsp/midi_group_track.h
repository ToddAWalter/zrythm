// SPDX-FileCopyrightText: © 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#ifndef __MIDI_MIDI_GROUP_TRACK_H__
#define __MIDI_MIDI_GROUP_TRACK_H__

#include "dsp/foldable_track.h"
#include "dsp/group_target_track.h"

class MidiGroupTrack final
    : public FoldableTrack,
      public GroupTargetTrack,
      public ICloneable<MidiGroupTrack>,
      public ISerializable<MidiGroupTrack>,
      public InitializableObjectFactory<MidiGroupTrack>
{
  friend class InitializableObjectFactory<MidiGroupTrack>;

public:
  void init_after_cloning (const MidiGroupTrack &other) override;

  DECLARE_DEFINE_FIELDS_METHOD ();

private:
  MidiGroupTrack () = default;
  MidiGroupTrack (const std::string &name, int pos);

  bool initialize () override;
};

#endif // __MIDI_MIDI_BUS_TRACK_H__
