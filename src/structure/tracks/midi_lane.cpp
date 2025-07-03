// SPDX-FileCopyrightText: © 2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "structure/tracks/laned_track.h"

#include "./midi_lane.h"

namespace zrythm::structure::tracks
{
MidiLane::MidiLane (
  structure::arrangement::ArrangerObjectRegistry &registry,
  dsp::FileAudioSourceRegistry                   &file_audio_source_registry,
  LanedTrackImpl<MidiLane> *                      track)
    : TrackLaneImpl<MidiRegion> (registry, file_audio_source_registry, track, *this)
{
}

void
init_from (MidiLane &obj, const MidiLane &other, utils::ObjectCloneType clone_type)
{
  init_from (
    static_cast<arrangement::ArrangerObjectOwner<arrangement::MidiRegion> &> (
      obj),
    static_cast<
      const arrangement::ArrangerObjectOwner<arrangement::MidiRegion> &> (other),
    clone_type);
  init_from (
    static_cast<TrackLaneImpl<arrangement::MidiRegion> &> (obj),
    static_cast<const TrackLaneImpl<arrangement::MidiRegion> &> (other),
    clone_type);
}
}
