// SPDX-FileCopyrightText: © 2018-2019, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "zrythm-config.h"

#include "gui/dsp/folder_track.h"

FolderTrack::FolderTrack (
  TrackRegistry  &track_registry,
  PluginRegistry &plugin_registry,
  PortRegistry   &port_registry,
  bool            new_identity)
    : Track (Track::Type::Folder, PortType::Unknown, PortType::Unknown)
{
  if (new_identity)
    {
      color_ = Color (QColor ("#865E3C"));
      icon_name_ = "fluentui-folder-regular";
    }
}

bool
FolderTrack::initialize ()
{
  // init_channel ();
  // generate_automation_tracks ();

  return true;
}

void
FolderTrack::append_ports (std::vector<Port *> &ports, bool include_plugins) const
{
  // ChannelTrack::append_member_ports (ports, include_plugins);
  // ProcessableTrack::append_member_ports (ports, include_plugins);
}

bool
FolderTrack::validate () const
{
  return Track::validate_base ();
  // && ChannelTrack::validate_base () && AutomatableTrack::validate_base ();
}

void
FolderTrack::init_after_cloning (
  const FolderTrack &other,
  ObjectCloneType    clone_type)
{
  FoldableTrack::copy_members_from (other, clone_type);
  Track::copy_members_from (other, clone_type);
}

void
FolderTrack::init_loaded (
  PluginRegistry &plugin_registry,
  PortRegistry   &port_registry)
{
}
