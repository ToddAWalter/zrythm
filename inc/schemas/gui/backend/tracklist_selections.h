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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * \file
 *
 * Tracklist selections schema.
 */

#ifndef __SCHEMAS_ACTIONS_TRACKLIST_SELECTIONS_H__
#define __SCHEMAS_ACTIONS_TRACKLIST_SELECTIONS_H__

#include "schemas/audio/track.h"
#include "utils/yaml.h"

typedef struct TracklistSelections_v1
{
  int                  schema_version;
  Track_v1 *              tracks[600];
  int                  num_tracks;
  bool                 is_project;
} TracklistSelections_v1;

static const cyaml_schema_field_t
  tracklist_selections_fields_schema_v1[] =
{
  YAML_FIELD_INT (
    TracklistSelections_v1, schema_version),
  YAML_FIELD_FIXED_SIZE_PTR_ARRAY_VAR_COUNT (
    TracklistSelections_v1, tracks, track_schema_v1),
  YAML_FIELD_INT (
    TracklistSelections_v1, is_project),

  CYAML_FIELD_END
};

static const cyaml_schema_value_t
tracklist_selections_schema_v1 = {
  YAML_VALUE_PTR (
    TracklistSelections_v1,
    tracklist_selections_fields_schema_v1),
};

#endif
