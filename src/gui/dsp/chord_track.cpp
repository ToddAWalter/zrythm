// SPDX-FileCopyrightText: © 2018-2022, 2024-2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include <algorithm>

#include "gui/backend/backend/project.h"
#include "gui/backend/backend/settings_manager.h"
#include "gui/backend/backend/zrythm.h"
#include "gui/dsp/chord_region.h"
#include "gui/dsp/chord_track.h"
#include "gui/dsp/track.h"
#include "utils/rt_thread_id.h"

ChordTrack::ChordTrack (
  TrackRegistry          &track_registry,
  PluginRegistry         &plugin_registry,
  PortRegistry           &port_registry,
  ArrangerObjectRegistry &obj_registry,
  bool                    new_identity)
    : Track (
        Track::Type::Chord,
        PortType::Event,
        PortType::Event,
        plugin_registry,
        port_registry,
        obj_registry),
      AutomatableTrack (port_registry, new_identity),
      ProcessableTrack (port_registry, new_identity),
      RecordableTrack (port_registry, new_identity),
      ChannelTrack (track_registry, plugin_registry, port_registry, new_identity),
      object_registry_ (obj_registry)
{
  if (new_identity)
    {
      color_ = Color (QColor ("#1C8FFB"));
      icon_name_ = "gnome-icon-library-library-music-symbolic";
    }
  automation_tracklist_->setParent (this);
  RegionOwner::parent_base_qproperties (*this);
}

// =========================================================================
// QML Interface
// =========================================================================

QHash<int, QByteArray>
ChordTrack::roleNames () const
{
  QHash<int, QByteArray> roles;
  roles[ScaleObjectPtrRole] = "scaleObject";
  return roles;
}

int
ChordTrack::rowCount (const QModelIndex &parent) const
{
  return static_cast<int> (scales_.size ());
}

QVariant
ChordTrack::data (const QModelIndex &index, int role) const
{
  if (!index.isValid ())
    return {};

  auto * scale = get_scale_at (index.row ());

  switch (role)
    {
    case ScaleObjectPtrRole:
      return QVariant::fromValue (scale);
    default:
      return {};
    }
}

// =========================================================================

void
ChordTrack::init_after_cloning (
  const ChordTrack &other,
  ObjectCloneType   clone_type)
{
  Track::copy_members_from (other, clone_type);
  AutomatableTrack::copy_members_from (other, clone_type);
  ProcessableTrack::copy_members_from (other, clone_type);
  RecordableTrack::copy_members_from (other, clone_type);
  ChannelTrack::copy_members_from (other, clone_type);
  RegionOwner::copy_members_from (other, clone_type);
  scales_.reserve (other.scales_.size ());
// TODO
#if 0
  for (const auto &scale : get_scales_view())
    {
      scales_.push_back (scale->clone_raw_ptr ());
      scales_.back ()->setParent (this);
    }
#endif
}

void
ChordTrack::append_ports (std::vector<Port *> &ports, bool include_plugins) const
{
  ChannelTrack::append_member_ports (ports, include_plugins);
  ProcessableTrack::append_member_ports (ports, include_plugins);
  RecordableTrack::append_member_ports (ports, include_plugins);
}

bool
ChordTrack::initialize ()
{
  init_channel ();
  generate_automation_tracks ();
  init_recordable_track ([] () {
    return ZRYTHM_HAVE_UI
           && zrythm::gui::SettingsManager::get_instance ()->get_trackAutoArm ();
  });

  return true;
}

void
ChordTrack::clear_objects ()
{
  beginResetModel ();
  clear_regions ();
  if (is_in_active_project ())
    {
      for (auto scale_id : std::ranges::reverse_view (scales_))
        {
          remove_scale (scale_id.id ());
        }
    }
  else
    {
      scales_.clear ();
    }
  scale_snapshots_.clear ();
  endResetModel ();
}

void
ChordTrack::set_playback_caches ()
{
// FIXME: TODO
#if 0
  region_snapshots_.clear ();
  region_snapshots_.reserve (region_list_->regions_.size ());
  foreach_region ([&] (auto &region) {
    z_return_if_fail (region.track_id_ == get_uuid ());
    region_snapshots_.push_back (region.clone_unique ());
  });

  scale_snapshots_.clear ();
  scale_snapshots_.reserve (scales_.size ());
  for (const auto &scale : get_scales_view ())
    {
      scale_snapshots_.push_back (scale->clone_unique ());
    }
#endif
}

void
ChordTrack::init_loaded (
  PluginRegistry &plugin_registry,
  PortRegistry   &port_registry)
{
  // ChannelTrack must be initialized before AutomatableTrack
  ChannelTrack::init_loaded (plugin_registry, port_registry);
  AutomatableTrack::init_loaded (plugin_registry, port_registry);
  ProcessableTrack::init_loaded (plugin_registry, port_registry);
  RecordableTrack::init_loaded (plugin_registry, port_registry);
  for (auto * scale : get_scales_view ())
    {
      scale->init_loaded ();
    }

  foreach_region ([&] (auto &chord_region) {
    chord_region.track_id_ = get_uuid ();
    chord_region.init_loaded ();
  });
}

ScaleObject *
ChordTrack::get_scale_at (size_t index) const
{
  return std::get<ScaleObject *> (scales_.at (index).get_object ());
}

void
ChordTrack::insert_scale (ArrangerObjectUuidReference scale_ref, int idx)
{
  z_return_if_fail (idx >= 0);
  z_return_if_fail (!get_uuid ().is_null ());
  beginInsertRows ({}, idx, idx);
  auto * scale = std::get<ScaleObject *> (scale_ref.get_object ());
  scale->set_track_id (get_uuid ());
  scales_.insert (scales_.begin () + idx, scale_ref);
  for (const auto &[index, s] : std::views::enumerate (get_scales_view ()))
    {
      s->set_index_in_chord_track (index);
    }
  endInsertRows ();

  // EVENTS_PUSH (EventType::ET_ARRANGER_OBJECT_CREATED, ret.get ());
}

ScaleObject *
ChordTrack::get_scale_at_pos (const Position pos) const
{
  auto view = std::ranges::reverse_view (get_scales_view ());
  auto it = std::ranges::find_if (view, [&pos] (const auto &scale) {
    return *scale->pos_ <= pos;
  });

  return it != view.end () ? (*it) : nullptr;
}

ChordObject *
ChordTrack::get_chord_at_pos (const Position pos) const
{
  auto region = get_region_at_pos (pos, false);
  if (!region)
    {
      return nullptr;
    }

  auto local_frames =
    (signed_frame_t) region->timeline_frames_to_local (pos.frames_, true);

  auto chord_objects_view =
    region->get_object_ptrs_view () | std::views::reverse;
  auto it =
    std::ranges::find_if (chord_objects_view, [local_frames] (const auto &co) {
      return co->pos_->frames_ <= local_frames;
    });

  return it != chord_objects_view.end () ? (*it) : nullptr;
}

void
ChordTrack::remove_scale (const ArrangerObject::Uuid &scale_id)
{
  // Find and remove the scale from the vector
  auto it =
    std::ranges::find (scales_, scale_id, &ArrangerObjectUuidReference::id);
  z_return_if_fail (it != scales_.end ());

  // Deselect the scale
  auto   scale_ref = *it;
  auto * scale = std::get<ScaleObject *> (scale_ref.get_object ());
  scale->unset_selection_status_getter ();
  // FIXME: use of global variable. this should be dependency-injected
  ArrangerObjectFactory::get_instance ()
    ->get_selection_manager_for_object (*scale)
    .remove_from_selection (scale_id);

  scale->index_in_chord_track_ = -1;
  int pos = std::distance (scales_.begin (), it);
  beginRemoveRows ({}, pos, pos);
  scales_.erase (it);

  // Update indices of remaining scales
  for (const auto &[index, s] : std::views::enumerate (get_scales_view ()))
    {
      s->set_index_in_chord_track (static_cast<int> (index));
    }
  endRemoveRows ();

  /* EVENTS_PUSH (
    EventType::ET_ARRANGER_OBJECT_REMOVED, ArrangerObject::Type::ScaleObject); */
}

bool
ChordTrack::validate () const
{
  if (
    !Track::validate_base () || !ChannelTrack::validate_base ()
    || !AutomatableTrack::validate_base ())
    return false;

  bool valid = true;
  foreach_region ([&] (auto &region) {
    bool inner_valid = region.validate (Track::is_in_active_project (), 0);
    if (!inner_valid)
      {
        valid = false;
      }
  });
  z_return_val_if_fail (valid, false);

#if 0
  for (size_t i = 0; i < scales_.size (); i++)
    {
      auto &m = scales_[i];
      z_return_val_if_fail (
        m->index_in_chord_track_ == static_cast<int> (i), false);
    }
#endif

  return true;
}
