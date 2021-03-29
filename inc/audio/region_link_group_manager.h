/*
 * Copyright (C) 2020-2021 Alexandros Theodotou <alex at zrythm dot org>
 *
 * This file is part of Zrythm
 *
 * Zrythm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version approved by
 * Alexandros Theodotou in a public statement of acceptance.
 *
 * Zrythm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Zrythm.  If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * \file
 *
 * Manager of linked region groups.
 */

#ifndef __AUDIO_REGION_LINK_GROUP_MANAGER_H__
#define __AUDIO_REGION_LINK_GROUP_MANAGER_H__

#include "audio/region_link_group.h"
#include "utils/yaml.h"

/**
 * @addtogroup audio
 *
 * @{
 */

#define REGION_LINK_GROUP_MANAGER_SCHEMA_VERSION 1

#define REGION_LINK_GROUP_MANAGER \
  (&PROJECT->region_link_group_manager)

/**
 * Manager of region link groups.
 */
typedef struct RegionLinkGroupManager
{
  int                schema_version;

  /** Region link groups. */
  RegionLinkGroup *  groups;
  int                num_groups;
  size_t             groups_size;
} RegionLinkGroupManager;

static const cyaml_schema_field_t
  region_link_group_manager_fields_schema[] =
{
  YAML_FIELD_INT (
    RegionLinkGroupManager, schema_version),
  YAML_FIELD_DYN_ARRAY_VAR_COUNT (
    RegionLinkGroupManager, groups,
    region_link_group_schema_default),

  CYAML_FIELD_END
};

static const cyaml_schema_value_t
  region_link_group_manager_schema =
{
  YAML_VALUE_PTR (
    RegionLinkGroupManager,
    region_link_group_manager_fields_schema),
};

void
region_link_group_manager_init_loaded (
  RegionLinkGroupManager * self);

void
region_link_group_manager_init (
  RegionLinkGroupManager * self);

/**
 * Adds a group and returns its index.
 */
int
region_link_group_manager_add_group (
  RegionLinkGroupManager * self);

RegionLinkGroup *
region_link_group_manager_get_group (
  RegionLinkGroupManager * self,
  int                      group_id);

/**
 * Removes the group.
 */
void
region_link_group_manager_remove_group (
  RegionLinkGroupManager * self,
  int                      group_id);

/**
 * @}
 */

#endif
