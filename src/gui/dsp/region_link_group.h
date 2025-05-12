// SPDX-FileCopyrightText: © 2020-2021, 2024-2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#pragma once

#include "gui/dsp/arranger_object.h"
#include "utils/format.h"

class Region;

/**
 * @addtogroup dsp
 *
 * @{
 */

/**
 * A group of linked regions.
 */
class RegionLinkGroup final
{
public:
  RegionLinkGroup (int idx) : group_idx_ (idx) { }
  void add_region (Region &region);

  /**
   * Remove the region from the link group.
   *
   * @param autoremove_last_region_and_group Automatically remove the last
   * region left in the group, and the group itself when empty.
   */
  void remove_region (
    Region &region,
    bool    autoremove_last_region_and_group,
    bool    update_identifier);

  bool contains_region (const Region &region) const;

  /**
   * Updates all other regions in the link group.
   *
   * @param region The region where the change happened.
   */
  void update (const Region &region);

  bool validate () const;

private:
  static constexpr auto kIdsKey = "ids"sv;
  friend void to_json (nlohmann::json &j, const RegionLinkGroup &group)
  {
    j[kIdsKey] = group.ids_;
  }
  friend void from_json (const nlohmann::json &j, RegionLinkGroup &group)
  {
    j.at (kIdsKey).get_to (group.ids_);
  }

public:
  /** Group index. */
  int group_idx_;

  /** Identifiers for regions in this link group. */
  std::vector<ArrangerObject::Uuid> ids_;
};

DEFINE_OBJECT_FORMATTER (
  RegionLinkGroup,
  RegionLinkGroup,
  [] (const RegionLinkGroup &group) {
    return fmt::format (
      "RegionLinkGroup {{ group_idx: {}, ids: [{}] }}", group.group_idx_,
      fmt::join (group.ids_, ", "));
  });

/**
 * @}
 */
