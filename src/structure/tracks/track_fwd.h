// SPDX-FileCopyrightText: © 2024-2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#pragma once

#include "utils/uuid_identifiable_object.h"
#include "utils/variant_helpers.h"

namespace zrythm::structure::tracks
{
Q_NAMESPACE

class Track;
class MarkerTrack;
class InstrumentTrack;
class MidiTrack;
class MasterTrack;
class MidiGroupTrack;
class AudioGroupTrack;
class FolderTrack;
class MidiBusTrack;
class AudioBusTrack;
class AudioTrack;
class ChordTrack;
class ModulatorTrack;

using TrackVariant = std::variant<
  MarkerTrack,
  InstrumentTrack,
  MidiTrack,
  MasterTrack,
  MidiGroupTrack,
  AudioGroupTrack,
  FolderTrack,
  MidiBusTrack,
  AudioBusTrack,
  AudioTrack,
  ChordTrack,
  ModulatorTrack>;
using TrackPtrVariant = to_pointer_variant<TrackVariant>;
using TrackRefVariant = to_reference_variant<TrackVariant>;
using TrackConstRefVariant = to_const_reference_variant<TrackVariant>;
using TrackUniquePtrVariant = to_unique_ptr_variant<TrackVariant>;
using OptionalTrackPtrVariant = std::optional<TrackPtrVariant>;

using TrackUuid = utils::UuidIdentifiableObject<Track>::Uuid;

using TrackResolver =
  utils::UuidIdentifiablObjectResolver<TrackPtrVariant, TrackUuid>;

template <typename TrackT>
concept FinalTrackSubclass =
  std::is_same_v<TrackT, MarkerTrack> || std::is_same_v<TrackT, InstrumentTrack>
  || std::is_same_v<TrackT, MidiTrack> || std::is_same_v<TrackT, MasterTrack>
  || std::is_same_v<TrackT, MidiGroupTrack>
  || std::is_same_v<TrackT, AudioGroupTrack>
  || std::is_same_v<TrackT, FolderTrack> || std::is_same_v<TrackT, MidiBusTrack>
  || std::is_same_v<TrackT, AudioBusTrack> || std::is_same_v<TrackT, AudioTrack>
  || std::is_same_v<TrackT, ChordTrack>
  || std::is_same_v<TrackT, ModulatorTrack>;

template <typename TrackT>
concept FoldableTrack =
  std::is_same_v<TrackT, AudioGroupTrack>
  || std::is_same_v<TrackT, MidiGroupTrack>
  || std::is_same_v<TrackT, FolderTrack>;

template <typename TrackT>
concept RecordableTrack =
  std::is_same_v<TrackT, AudioTrack> || std::is_same_v<TrackT, ChordTrack>
  || std::is_same_v<TrackT, InstrumentTrack>
  || std::is_same_v<TrackT, MidiTrack>;

template <typename TrackT>
concept ProcessableTrack =
  std::is_same_v<TrackT, InstrumentTrack> || std::is_same_v<TrackT, MidiTrack>
  || std::is_same_v<TrackT, MasterTrack>
  || std::is_same_v<TrackT, MidiGroupTrack>
  || std::is_same_v<TrackT, AudioGroupTrack>
  || std::is_same_v<TrackT, MidiBusTrack>
  || std::is_same_v<TrackT, AudioBusTrack> || std::is_same_v<TrackT, AudioTrack>
  || std::is_same_v<TrackT, ChordTrack>;

template <typename TrackT>
concept AutomatableTrack =
  ProcessableTrack<TrackT> || std::is_same_v<TrackT, ModulatorTrack>;

template <typename TrackT>
concept PianoRollTrack =
  std::is_same_v<TrackT, InstrumentTrack> || std::is_same_v<TrackT, MidiTrack>;

enum class ClipQuantizeOption : uint8_t
{
  Immediate,
  NextBar,
  NextBeat,
  NextQuarterBeat,
  NextEighthBeat,
  NextSixteenthBeat,
  NextThirtySecondBeat
};
Q_ENUM_NS (ClipQuantizeOption)
}

DEFINE_UUID_HASH_SPECIALIZATION (zrythm::structure::tracks::TrackUuid);
