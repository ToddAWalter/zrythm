// SPDX-FileCopyrightText: © 2019-2022, 2024-2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "structure/tracks/marker_track.h"

namespace zrythm::structure::tracks
{
MarkerTrack::MarkerTrack (FinalTrackDependencies dependencies)
    : Track (
        Track::Type::Marker,
        PortType::Unknown,
        PortType::Unknown,
        {},
        dependencies.to_base_dependencies ()),
      arrangement::ArrangerObjectOwner<Marker> (
        dependencies.obj_registry_,
        dependencies.file_audio_source_registry_,
        *this)
{
  main_height_ = DEF_HEIGHT / 2;
  icon_name_ = u8"gnome-icon-library-flag-filled-symbolic";
  color_ = Color (QColor ("#7C009B"));
}

auto
MarkerTrack::get_start_marker () const -> Marker *
{
  auto markers = get_children_view ();
  auto it =
    std::ranges::find (markers, Marker::MarkerType::Start, &Marker::markerType);
  z_return_val_if_fail (it != markers.end (), nullptr);
  return *it;
}

auto
MarkerTrack::get_end_marker () const -> Marker *
{
  auto markers = get_children_view ();
  auto it =
    std::ranges::find (markers, Marker::MarkerType::End, &Marker::markerType);
  z_return_val_if_fail (it != markers.end (), nullptr);
  return *it;
}

void
MarkerTrack::set_playback_caches ()
{
  // TODO
#if 0
  marker_snapshots_.clear ();
  marker_snapshots_.reserve (markers_.size ());

  for (const auto &marker : markers_)
    {
      marker_snapshots_.push_back (marker->clone_unique ());
    }
#endif
}

void
init_from (
  MarkerTrack           &obj,
  const MarkerTrack     &other,
  utils::ObjectCloneType clone_type)
{
  init_from (
    static_cast<arrangement::ArrangerObjectOwner<arrangement::Marker> &> (obj),
    static_cast<const arrangement::ArrangerObjectOwner<arrangement::Marker> &> (
      other),
    clone_type);
  init_from (
    static_cast<Track &> (obj), static_cast<const Track &> (other), clone_type);
}
}
