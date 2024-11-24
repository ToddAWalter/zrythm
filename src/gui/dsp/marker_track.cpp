// SPDX-FileCopyrightText: © 2019-2022, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "gui/backend/backend/project.h"
#include "gui/backend/backend/timeline_selections.h"
#include "gui/backend/backend/zrythm.h"
#include "gui/dsp/marker_track.h"
#include "gui/dsp/track.h"

#include "utils/rt_thread_id.h"

MarkerTrack::MarkerTrack (int track_pos)
    : Track (
        Track::Type::Marker,
        tr ("Markers").toStdString (),
        track_pos,
        PortType::Unknown,
        PortType::Unknown)
{
  main_height_ = TRACK_DEF_HEIGHT / 2;
  icon_name_ = "gnome-icon-library-flag-filled-symbolic";
  color_ = Color (QColor ("#7C009B"));
}

void
MarkerTrack::add_default_markers (int ticks_per_bar, double frames_per_tick)
{
  /* add start and end markers */
  auto     marker_name = fmt::format ("[{}]", QObject::tr ("start"));
  auto *   marker = new Marker (marker_name, this);
  Position pos;
  pos.set_to_bar (1, ticks_per_bar, frames_per_tick);
  marker->pos_setter (&pos);
  marker->marker_type_ = Marker::Type::Start;
  add_marker (marker);

  marker_name = fmt::format ("[{}]", QObject::tr ("end"));
  marker = new Marker (marker_name, this);
  pos.set_to_bar (129, ticks_per_bar, frames_per_tick);
  marker->pos_setter (&pos);
  marker->marker_type_ = Marker::Type::End;
  add_marker (marker);
}

// ========================================================================
// QML Interface
// ========================================================================
QHash<int, QByteArray>
MarkerTrack::roleNames () const
{
  QHash<int, QByteArray> roles;
  roles[MarkerPtrRole] = "marker";
  return roles;
}

int
MarkerTrack::rowCount (const QModelIndex &parent) const
{
  return static_cast<int> (markers_.size ());
}

QVariant
MarkerTrack::data (const QModelIndex &index, int role) const
{
  if (!index.isValid ())
    return {};

  auto marker = markers_.at (index.row ());

  switch (role)
    {
    case MarkerPtrRole:
      return QVariant::fromValue (marker);
    default:
      return {};
    }
}

// ========================================================================

bool
MarkerTrack::initialize ()
{
  return true;
}

void
MarkerTrack::init_loaded ()
{
  for (auto &marker : markers_)
    {
      marker->init_loaded ();
    }
}

MarkerTrack::MarkerPtr
MarkerTrack::get_start_marker () const
{
  auto it =
    std::find_if (markers_.begin (), markers_.end (), [] (const auto &marker) {
      return marker->marker_type_ == Marker::Type::Start;
    });
  z_return_val_if_fail (it != markers_.end (), nullptr);
  return *it;
}

MarkerTrack::MarkerPtr
MarkerTrack::get_end_marker () const
{
  auto it =
    std::find_if (markers_.begin (), markers_.end (), [] (const auto &marker) {
      return marker->marker_type_ == Marker::Type::End;
    });
  z_return_val_if_fail (it != markers_.end (), nullptr);
  return *it;
}

MarkerTrack::MarkerPtr
MarkerTrack::insert_marker (MarkerTrack::MarkerPtr marker, int pos)
{
  marker->set_track_name_hash (get_name_hash ());
  markers_.insert (markers_.begin () + pos, marker);
  marker->setParent (this);

  for (size_t i = pos; i < markers_.size (); ++i)
    {
      auto m = markers_[i];
      m->set_marker_track_index (i);
    }

  z_return_val_if_fail (validate (), nullptr);

  // EVENTS_PUSH (EventType::ET_ARRANGER_OBJECT_CREATED, marker.get ());

  return marker;
}

void
MarkerTrack::clear_objects ()
{
  for (auto &marker : markers_ | std::views::reverse)
    {
      if (marker->is_start () || marker->is_end ())
        continue;
      remove_marker (*marker, true, true);
    }
}

void
MarkerTrack::set_playback_caches ()
{
  marker_snapshots_.clear ();
  marker_snapshots_.reserve (markers_.size ());
  for (const auto &marker : markers_)
    {
      marker_snapshots_.push_back (marker->clone_unique ());
    }
}

void
MarkerTrack::init_after_cloning (const MarkerTrack &other)
{
  markers_.reserve (other.markers_.size ());
  for (auto &marker : markers_)
    {
      auto * clone = marker->clone_raw_ptr ();
      clone->setParent (this);
      markers_.push_back (clone);
    }
  Track::copy_members_from (other);
}

bool
MarkerTrack::validate () const
{
  if (!Track::validate_base ())
    {
      return false;
    }

  for (size_t i = 0; i < markers_.size (); ++i)
    {
      auto m = markers_[i];
      z_return_val_if_fail (m->marker_track_index_ == (int) i, false);
    }
  return true;
}

void
MarkerTrack::append_ports (std::vector<Port *> &ports, bool include_plugins) const
{
}

MarkerTrack::MarkerPtr
MarkerTrack::remove_marker (Marker &marker, bool free_marker, bool fire_events)
{
  /* deselect */
  TL_SELECTIONS->remove_object (marker);

  auto it =
    std::find_if (markers_.begin (), markers_.end (), [&] (const auto &m) {
      return m == &marker;
    });
  z_return_val_if_fail (it != markers_.end (), nullptr);
  auto ret = *it;
  it = markers_.erase (it);
  ret->setParent (nullptr);
  if (free_marker)
    {
      ret->deleteLater ();
    }

  for (
    size_t i = std::distance (markers_.begin (), it); i < markers_.size (); ++i)
    {
      auto * m = markers_[i];
      m->set_marker_track_index (i);
    }

  /* EVENTS_PUSH (
    EventType::ET_ARRANGER_OBJECT_REMOVED, ArrangerObject::Type::Marker); */

  return free_marker ? nullptr : ret;
}
