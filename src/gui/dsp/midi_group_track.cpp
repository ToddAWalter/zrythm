// SPDX-FileCopyrightText: © 2019, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "gui/dsp/midi_group_track.h"

MidiGroupTrack::MidiGroupTrack (const std::string &name, int pos)
    : Track (Track::Type::MidiGroup, name, pos, PortType::Event, PortType::Event)
{
  color_ = Color (QColor ("#E66100"));
  icon_name_ = "signal-midi";
  automation_tracklist_->setParent (this);
}

bool
MidiGroupTrack::initialize ()
{
  init_channel ();
  generate_automation_tracks ();

  return true;
}

void
MidiGroupTrack::init_loaded ()
{
  // ChannelTrack must be initialized before AutomatableTrack
  ChannelTrack::init_loaded ();
  AutomatableTrack::init_loaded ();
  ProcessableTrack::init_loaded ();
}

void
MidiGroupTrack::init_after_cloning (const MidiGroupTrack &other)
{
  FoldableTrack::copy_members_from (other);
  ChannelTrack::copy_members_from (other);
  ProcessableTrack::copy_members_from (other);
  AutomatableTrack::copy_members_from (other);
  Track::copy_members_from (other);
}

void
MidiGroupTrack::append_ports (std::vector<Port *> &ports, bool include_plugins)
  const
{
  ChannelTrack::append_member_ports (ports, include_plugins);
  ProcessableTrack::append_member_ports (ports, include_plugins);
}

bool
MidiGroupTrack::validate () const
{
  return Track::validate_base () && GroupTargetTrack::validate_base ()
         && ChannelTrack::validate_base () && AutomatableTrack::validate_base ();
}
