// SPDX-FileCopyrightText: © 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "common/dsp/lane_owned_object.h"
#include "common/dsp/laned_track.h"

template <typename RegionT>
LaneOwnedObjectImpl<RegionT>::TrackLaneT *
LaneOwnedObjectImpl<RegionT>::get_lane () const
{
  if (owner_lane_ != nullptr)
    {
      return owner_lane_;
    }

  auto * region = dynamic_cast<const RegionT *> (this);
  auto track = dynamic_cast<const LanedTrackImpl<TrackLaneT> *> (get_track ());
  z_return_val_if_fail (track, nullptr);
  z_return_val_if_fail (
    region->id_.lane_pos_ < (int) track->lanes_.size (), nullptr);

  auto lane_var = track->lanes_.at (region->id_.lane_pos_);
  return std::visit (
    [&] (auto &&lane) -> TrackLaneT * {
      z_return_val_if_fail (lane, nullptr);
      return dynamic_cast<TrackLaneT *> (lane);
    },
    lane_var);
}

template <typename RegionT>
void
LaneOwnedObjectImpl<RegionT>::set_lane (TrackLaneT &lane)
{
  z_return_if_fail (lane.track_ != nullptr);

  if (lane.is_auditioner ())
    is_auditioner_ = true;

  owner_lane_ = &lane;

  if (is_region ())
    {
      auto region = dynamic_cast<Region *> (this);
      region->id_.lane_pos_ = lane.pos_;
      region->id_.track_name_hash_ = lane.track_->get_name_hash ();
    }
  track_name_hash_ = lane.track_->get_name_hash ();
}

template class LaneOwnedObjectImpl<MidiRegion>;
template class LaneOwnedObjectImpl<AudioRegion>;