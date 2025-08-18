// SPDX-FileCopyrightText: © 2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "structure/tracks/track_all.h"
#include "structure/tracks/track_routing.h"

namespace zrythm::structure::tracks
{
QVariant
TrackRouting::getOutputTrack (const Track * source) const
{
  auto output = get_output_track (source->get_uuid ());
  return output ? QVariant::fromStdVariant (output.value ().get_object ())
                : QVariant{};
}

void
TrackRouting::setOutputTrack (const Track * source, const Track * destination)
{
  z_info ("routing track '{}' to '{}'", source->name (), destination->name ());
  add_or_replace_route (source->get_uuid (), destination->get_uuid ());
}

std::optional<TrackUuidReference>
TrackRouting::get_output_track (const TrackUuid &source) const
{
  auto it = track_routes_.find (source);
  if (it == track_routes_.end ())
    {
      return std::nullopt;
    }

  return TrackUuidReference{ it->second, track_registry_ };
}

} // namespace zrythm::structure::tracks
