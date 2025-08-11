// SPDX-FileCopyrightText: © 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "structure/tracks/track_all.h"
#include "structure/tracks/tracklist.h"

#include "track_filter_proxy_model.h"

using namespace zrythm::gui;

TrackFilterProxyModel::TrackFilterProxyModel (QObject * parent)
    : QSortFilterProxyModel (parent)
{
}

void
TrackFilterProxyModel::addVisibilityFilter (bool visible)
{
  use_visible_filter_ = true;
  visible_filter_ = visible;
  invalidateFilter ();
}

void
TrackFilterProxyModel::addPinnedFilter (bool pinned)
{
  use_pinned_filter_ = true;
  pinned_filter_ = pinned;
  invalidateFilter ();
}

void
TrackFilterProxyModel::clearFilters ()
{
  use_visible_filter_ = false;
  use_pinned_filter_ = false;
  invalidateFilter ();
}

bool
TrackFilterProxyModel::filterAcceptsRow (
  int                source_row,
  const QModelIndex &source_parent) const
{
  if (
    auto * tracklist =
      dynamic_cast<structure::tracks::Tracklist *> (sourceModel ()))
    {
      auto tr = tracklist->get_track_at_index (source_row);
      return std::visit (
        [&] (auto &&track) {
          z_return_val_if_fail (track, false);

          if (use_visible_filter_)
            {
              if (
                tracklist->should_be_visible (track->get_uuid ())
                != visible_filter_)
                {
                  return false;
                }
            }

          if (use_pinned_filter_)
            {
              if (
                tracklist->is_track_pinned (track->get_uuid ())
                != pinned_filter_)
                {
                  return false;
                }
            }

          return true;
        },
        tr);
    }
  z_warning ("invalid source model");
  return false;
}
