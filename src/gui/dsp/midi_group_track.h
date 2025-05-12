// SPDX-FileCopyrightText: © 2024-2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#pragma once

#include "gui/dsp/foldable_track.h"
#include "gui/dsp/group_target_track.h"

class MidiGroupTrack final
    : public QObject,
      public FoldableTrack,
      public GroupTargetTrack,
      public ICloneable<MidiGroupTrack>,
      public utils::InitializableObject<MidiGroupTrack>
{
  Q_OBJECT
  QML_ELEMENT
  DEFINE_TRACK_QML_PROPERTIES (MidiGroupTrack)
  DEFINE_AUTOMATABLE_TRACK_QML_PROPERTIES (MidiGroupTrack)
  DEFINE_CHANNEL_TRACK_QML_PROPERTIES (MidiGroupTrack)

  friend class InitializableObject;

  DECLARE_FINAL_TRACK_CONSTRUCTORS (MidiGroupTrack)

public:
  void
  init_after_cloning (const MidiGroupTrack &other, ObjectCloneType clone_type)
    override;

  void
  init_loaded (PluginRegistry &plugin_registry, PortRegistry &port_registry)
    override;

  bool validate () const override;

  void
  append_ports (std::vector<Port *> &ports, bool include_plugins) const final;

private:
  friend void to_json (nlohmann::json &j, const MidiGroupTrack &track)
  {
    to_json (j, static_cast<const Track &> (track));
    to_json (j, static_cast<const ProcessableTrack &> (track));
    to_json (j, static_cast<const AutomatableTrack &> (track));
    to_json (j, static_cast<const ChannelTrack &> (track));
    to_json (j, static_cast<const GroupTargetTrack &> (track));
    to_json (j, static_cast<const FoldableTrack &> (track));
  }
  friend void from_json (const nlohmann::json &j, MidiGroupTrack &track)
  {
    from_json (j, static_cast<Track &> (track));
    from_json (j, static_cast<ProcessableTrack &> (track));
    from_json (j, static_cast<AutomatableTrack &> (track));
    from_json (j, static_cast<ChannelTrack &> (track));
    from_json (j, static_cast<GroupTargetTrack &> (track));
    from_json (j, static_cast<FoldableTrack &> (track));
  }

  bool initialize ();
};
