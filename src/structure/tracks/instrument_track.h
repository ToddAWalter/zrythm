// SPDX-FileCopyrightText: © 2018-2020, 2024-2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#pragma once

#include "structure/tracks/group_target_track.h"
#include "structure/tracks/piano_roll_track.h"
#include "utils/initializable_object.h"

namespace zrythm::structure::tracks
{
class InstrumentTrack final
    : public QObject,
      public GroupTargetTrack,
      public PianoRollTrack,
      public utils::InitializableObject<InstrumentTrack>
{
  Q_OBJECT
  QML_ELEMENT
  DEFINE_TRACK_QML_PROPERTIES (InstrumentTrack)
  DEFINE_LANED_TRACK_QML_PROPERTIES (InstrumentTrack)
  DEFINE_PROCESSABLE_TRACK_QML_PROPERTIES (InstrumentTrack)
  DEFINE_CHANNEL_TRACK_QML_PROPERTIES (InstrumentTrack)
  DEFINE_PIANO_ROLL_TRACK_QML_PROPERTIES (InstrumentTrack)

  friend class InitializableObject;

  DECLARE_FINAL_TRACK_CONSTRUCTORS (InstrumentTrack)

public:
  void init_loaded (
    gui::old_dsp::plugins::PluginRegistry &plugin_registry,
    dsp::PortRegistry                     &port_registry,
    dsp::ProcessorParameterRegistry       &param_registry) override;

  friend void init_from (
    InstrumentTrack       &obj,
    const InstrumentTrack &other,
    utils::ObjectCloneType clone_type);

  zrythm::gui::old_dsp::plugins::Plugin * get_instrument ();

  const zrythm::gui::old_dsp::plugins::Plugin * get_instrument () const;

  /**
   * Returns if the first plugin's UI in the instrument track is visible.
   */
  bool is_plugin_visible () const;

  /**
   * Toggles whether the first plugin's UI in the instrument Track is visible.
   */
  void toggle_plugin_visible ();

  void temporary_virtual_method_hack () const override { }

private:
  friend void
  to_json (nlohmann::json &j, const InstrumentTrack &instrument_track)
  {
    to_json (j, static_cast<const Track &> (instrument_track));
    to_json (j, static_cast<const ProcessableTrack &> (instrument_track));
    to_json (j, static_cast<const RecordableTrack &> (instrument_track));
    to_json (j, static_cast<const PianoRollTrack &> (instrument_track));
    to_json (j, static_cast<const ChannelTrack &> (instrument_track));
    to_json (j, static_cast<const LanedTrackImpl &> (instrument_track));
    to_json (j, static_cast<const GroupTargetTrack &> (instrument_track));
  }
  friend void
  from_json (const nlohmann::json &j, InstrumentTrack &instrument_track)
  {
    from_json (j, static_cast<Track &> (instrument_track));
    from_json (j, static_cast<ProcessableTrack &> (instrument_track));
    from_json (j, static_cast<RecordableTrack &> (instrument_track));
    from_json (j, static_cast<PianoRollTrack &> (instrument_track));
    from_json (j, static_cast<ChannelTrack &> (instrument_track));
    from_json (j, static_cast<LanedTrackImpl &> (instrument_track));
    from_json (j, static_cast<GroupTargetTrack &> (instrument_track));
  }

  bool initialize ();

public:
};

}
