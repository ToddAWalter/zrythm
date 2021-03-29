/*
 * Copyright (C) 2019-2021 Alexandros Theodotou <alex at zrythm dot org>
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
 * Arranger object schema.
 */

#ifndef __SCHEMAS_GUI_BACKEND_ARRANGER_OBJECT_H__
#define __SCHEMAS_GUI_BACKEND_ARRANGER_OBJECT_H__

#include <stdbool.h>

#include "audio/curve.h"
#include "audio/position.h"
#include "audio/region_identifier.h"
#include "utils/yaml.h"

typedef enum ArrangerObjectType_v1
{
  ARRANGER_OBJECT_TYPE_NONE_v1,
  ARRANGER_OBJECT_TYPE_ALL_v1,
  ARRANGER_OBJECT_TYPE_REGION_v1,
  ARRANGER_OBJECT_TYPE_MIDI_NOTE_v1,
  ARRANGER_OBJECT_TYPE_CHORD_OBJECT_v1,
  ARRANGER_OBJECT_TYPE_SCALE_OBJECT_v1,
  ARRANGER_OBJECT_TYPE_MARKER_v1,
  ARRANGER_OBJECT_TYPE_AUTOMATION_POINT_v1,
  ARRANGER_OBJECT_TYPE_VELOCITY_v1,
} ArrangerObjectType_v1;

static const cyaml_strval_t
arranger_object_type_strings_v1[] =
{
  { __("None"),
    ARRANGER_OBJECT_TYPE_NONE_v1 },
  { __("All"),
    ARRANGER_OBJECT_TYPE_ALL_v1 },
  { __("Region"),
    ARRANGER_OBJECT_TYPE_REGION_v1 },
  { __("Midi Note"),
    ARRANGER_OBJECT_TYPE_MIDI_NOTE_v1 },
  { __("Chord Object"),
    ARRANGER_OBJECT_TYPE_CHORD_OBJECT_v1 },
  { __("Scale Object"),
    ARRANGER_OBJECT_TYPE_SCALE_OBJECT_v1 },
  { __("Marker"),
    ARRANGER_OBJECT_TYPE_MARKER_v1 },
  { __("Automation Point"),
    ARRANGER_OBJECT_TYPE_AUTOMATION_POINT_v1 },
  { __("Velocity"),
    ARRANGER_OBJECT_TYPE_VELOCITY_v1 },
};

typedef enum ArrangerObjectFlags_v1
{
  ARRANGER_OBJECT_FLAG_NON_PROJECT = 1 << 0,
} ArrangerObjectFlags_v1;

static const cyaml_bitdef_t
arranger_object_flags_bitvals_v1[] =
{
  { .name = "non_project", .offset =  0, .bits =  1 },
};

typedef struct ArrangerObject_v1
{
  int                schema_version;
  ArrangerObjectType_v1 type;
  ArrangerObjectFlags_v1  flags;
  Position_v1           pos;
  Position_v1           end_pos;
  Position_v1           clip_start_pos;
  Position_v1           loop_start_pos;
  Position_v1           loop_end_pos;
  Position_v1           fade_in_pos;
  Position_v1           fade_out_pos;
  CurveOptions_v1       fade_in_opts;
  CurveOptions_v1       fade_out_opts;
  GdkRectangle       full_rect;
  int                textw;
  int                texth;
  void *   transient;
  void *   main;
  bool               muted;
  int                magic;
  RegionIdentifier_v1   region_id;
  int                index_in_prev_lane;
  bool               deleted_temporarily;
  bool               use_cache;
  void *          cached_cr[2];
  void *  cached_surface[2];
  GdkRectangle       last_name_rect;
} ArrangerObject_v1;

static const cyaml_schema_field_t
  arranger_object_fields_schema_v1[] =
{
  YAML_FIELD_INT (ArrangerObject_v1, schema_version),
  YAML_FIELD_ENUM (
    ArrangerObject_v1, type,
    arranger_object_type_strings_v1),
  CYAML_FIELD_BITFIELD (
    "flags", CYAML_FLAG_DEFAULT,
    ArrangerObject_v1, flags,
    arranger_object_flags_bitvals_v1,
    CYAML_ARRAY_LEN (
      arranger_object_flags_bitvals_v1)),
  YAML_FIELD_INT (ArrangerObject_v1, muted),
  YAML_FIELD_MAPPING_EMBEDDED (
    ArrangerObject_v1, pos,
    position_fields_schema_v1),
  YAML_FIELD_MAPPING_EMBEDDED (
    ArrangerObject_v1, end_pos,
    position_fields_schema_v1),
  YAML_FIELD_MAPPING_EMBEDDED (
    ArrangerObject_v1, clip_start_pos,
    position_fields_schema_v1),
  YAML_FIELD_MAPPING_EMBEDDED (
    ArrangerObject_v1, loop_start_pos,
    position_fields_schema_v1),
  YAML_FIELD_MAPPING_EMBEDDED (
    ArrangerObject_v1, loop_end_pos,
    position_fields_schema_v1),
  YAML_FIELD_MAPPING_EMBEDDED (
    ArrangerObject_v1, fade_in_pos,
    position_fields_schema_v1),
  YAML_FIELD_MAPPING_EMBEDDED (
    ArrangerObject_v1, fade_out_pos,
    position_fields_schema_v1),
  YAML_FIELD_MAPPING_EMBEDDED (
    ArrangerObject_v1, fade_in_opts,
    curve_options_fields_schema_v1),
  YAML_FIELD_MAPPING_EMBEDDED (
    ArrangerObject_v1, fade_out_opts,
    curve_options_fields_schema_v1),
  YAML_FIELD_MAPPING_EMBEDDED (
    ArrangerObject_v1, region_id,
    region_identifier_fields_schema_v1),

  CYAML_FIELD_END
};

static const cyaml_schema_value_t
  arranger_object_schema_v1 =
{
  YAML_VALUE_PTR (
    ArrangerObject_v1,
    arranger_object_fields_schema_v1),
};

#endif
