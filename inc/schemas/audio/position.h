/*
 * Copyright (C) 2018-2021 Alexandros Theodotou <alex at zrythm dot org>
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
 * Position schema.
 */

#ifndef __SCHEMAS_AUDIO_POSITION_H__
#define __SCHEMAS_AUDIO_POSITION_H__

#include <stdbool.h>

#include "utils/yaml.h"

typedef struct Position_v1
{
  int       schema_version;
  double    ticks;
  long      frames;
} Position_v1;

static const cyaml_schema_field_t
  position_fields_schema_v1[] =
{
  YAML_FIELD_INT (Position_v1, schema_version),
  YAML_FIELD_FLOAT (
    Position_v1, ticks),
  YAML_FIELD_INT (
    Position_v1, frames),
  CYAML_FIELD_END
};

static const cyaml_schema_value_t
  position_schema_v1 =
{
  YAML_VALUE_PTR (
    Position_v1, position_fields_schema_v1),
};

#endif
