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
 * Region identifier.
 *
 * This is in its own file to avoid recursive
 * inclusion.
 */

#ifndef __AUDIO_REGION_IDENTIFIER_H__
#define __AUDIO_REGION_IDENTIFIER_H__

#include <stdbool.h>

#include "utils/general.h"
#include "utils/yaml.h"

/**
 * @addtogroup audio
 *
 * @{
 */

#define REGION_IDENTIFIER_SCHEMA_VERSION 1

/**
 * Type of Region.
 *
 * Bitfield instead of plain enum so multiple
 * values can be passed to some functions (eg to
 * collect all Regions of the given types in a
 * Track).
 */
typedef enum RegionType
{
  REGION_TYPE_MIDI = 1 << 0,
  REGION_TYPE_AUDIO = 1 << 1,
  REGION_TYPE_AUTOMATION = 1 << 2,
  REGION_TYPE_CHORD = 1 << 3,
} RegionType;

static const cyaml_bitdef_t
region_type_bitvals[] =
{
  { .name = "midi", .offset =  0, .bits =  1 },
  { .name = "audio", .offset =  1, .bits =  1 },
  { .name = "automation", .offset = 2, .bits = 1 },
  { .name = "chord", .offset = 3, .bits = 1 },
};

/**
 * Index/identifier for a Region, so we can
 * get Region objects quickly with it without
 * searching by name.
 */
typedef struct RegionIdentifier
{
  int        schema_version;

  RegionType type;

  /** Link group index, if any, or -1. */
  int        link_group;

  int        track_pos;
  int        lane_pos;

  /** Automation track index in the automation
   * tracklist, if automation region. */
  int        at_idx;

  /** Index inside lane or automation track. */
  int        idx;
} RegionIdentifier;

static const cyaml_schema_field_t
region_identifier_fields_schema[] =
{
  YAML_FIELD_INT (
    RegionIdentifier, schema_version),
  YAML_FIELD_BITFIELD (
    RegionIdentifier, type, region_type_bitvals),
  YAML_FIELD_INT (
    RegionIdentifier, link_group),
  YAML_FIELD_INT (
    RegionIdentifier, track_pos),
  YAML_FIELD_INT (
    RegionIdentifier, lane_pos),
  YAML_FIELD_INT (
    RegionIdentifier, at_idx),
  YAML_FIELD_INT (
    RegionIdentifier, idx),

  CYAML_FIELD_END
};

static const cyaml_schema_value_t
region_identifier_schema = {
  YAML_VALUE_PTR (
    RegionIdentifier,
    region_identifier_fields_schema),
};

static const cyaml_schema_value_t
region_identifier_schema_default = {
  YAML_VALUE_DEFAULT (
    RegionIdentifier,
    region_identifier_fields_schema),
};

void
region_identifier_init (
  RegionIdentifier * self);

static inline int
region_identifier_is_equal (
  const RegionIdentifier * a,
  const RegionIdentifier * b)
{
  return
    a->idx == b->idx &&
    a->track_pos == b->track_pos &&
    a->lane_pos == b->lane_pos &&
    a->at_idx == b->at_idx &&
    a->link_group == b->link_group &&
    a->type == b->type;
}

static inline void
region_identifier_copy (
  RegionIdentifier * dest,
  const RegionIdentifier * src)
{
  dest->schema_version = src->schema_version;
  dest->idx = src->idx;
  dest->track_pos = src->track_pos;
  dest->lane_pos = src->lane_pos;
  dest->at_idx = src->at_idx;
  dest->type = src->type;
  dest->link_group = src->link_group;
}

bool
region_identifier_validate (
  RegionIdentifier * self);

static inline const char *
region_identifier_get_region_type_name (
  RegionType type)
{
  g_return_val_if_fail (
    type >= REGION_TYPE_MIDI &&
      type <= REGION_TYPE_CHORD, NULL);

  return
    region_type_bitvals[
      utils_get_uint_from_bitfield_val (type)].name;
}

static inline void
region_identifier_print (
  const RegionIdentifier * self)
{
  g_message (
    "Region identifier: "
    "type: %s, track pos %d, lane pos %d, "
    "at index %d, index %d, link_group: %d",
    region_identifier_get_region_type_name (
      self->type),
    self->track_pos, self->lane_pos, self->at_idx,
    self->idx, self->link_group);
}

/**
 * @}
 */

#endif
