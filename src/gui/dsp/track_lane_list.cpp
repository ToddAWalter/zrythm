// SPDX-FileCopyrightText: © 2024-2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "track_lane_list.h"

TrackLaneList::TrackLaneList (QObject * parent)
    : QAbstractListModel (parent) { }

TrackLaneList::~TrackLaneList ()
{
  clear ();
}

QHash<int, QByteArray>
TrackLaneList::roleNames () const
{
  QHash<int, QByteArray> roles;
  roles[TrackLanePtrRole] = "trackLane";
  return roles;
}

int
TrackLaneList::rowCount (const QModelIndex &parent) const
{
  if (parent.isValid ())
    return 0;
  return static_cast<int> (lanes_.size ());
}

QVariant
TrackLaneList::data (const QModelIndex &index, int role) const
{
  if (!index.isValid () || index.row () >= static_cast<int> (lanes_.size ()))
    return {};

  auto lane_var = lanes_.at (index.row ());

  switch (role)
    {
    case TrackLanePtrRole:
      return QVariant::fromStdVariant (lane_var);
    case Qt::DisplayRole:
      return QString::fromStdString (
        std::visit ([&] (auto &&lane) { return lane->get_name (); }, lane_var));
    default:
      return {};
    }

  return {};
}

void
TrackLaneList::copy_members_from (
  const TrackLaneList &other,
  ObjectCloneType      clone_type)
{
  clear ();
  lanes_.reserve (other.size ());
// TODO
#if 0
  for (const auto lane_var : other)
    {
      std::visit (
        [&] (auto &&lane) {
          auto * new_lane = lane->clone_raw_ptr ();
          push_back (new_lane);
        },
        lane_var);
    }
#endif
}
