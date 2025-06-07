// SPDX-FileCopyrightText: © 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "gui/backend/backend/zrythm.h"
#include "structure/arrangement/region.h"
#include "structure/arrangement/region_owned_object.h"
#include "utils/gtest_wrapper.h"

namespace zrythm::structure::arrangement
{
void
init_from (
  RegionOwnedObject       &obj,
  const RegionOwnedObject &other,
  utils::ObjectCloneType   clone_type)
{
  obj.region_id_ = other.region_id_;
}

void
RegionOwnedObject::set_region_and_index (const Region &region)
{
  region_id_ = region.get_uuid ();
  set_track_id (region.get_track_id ());

#if 0
  /* note: this was only done for automation points, not sure why */
  /* set the info to the transient too */
  if ((ZRYTHM_HAVE_UI || ZRYTHM_TESTING) && PROJECT->loaded_ && transient_)
    // && arranger_object_should_orig_be_visible (*this, nullptr))
    {
      auto trans_obj = get_transient<RegionOwnedObject> ();
      trans_obj->region_id_ = region.id_;
      trans_obj->index_ = index_;
      trans_obj->track_id_ = region.track_id_;
    }
#endif
}
}
