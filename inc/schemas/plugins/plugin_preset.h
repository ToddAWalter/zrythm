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
 * Plugin preset schemas.
 */

#ifndef __SCHEMAS_PLUGINS_PLUGIN_PRESET_H__
#define __SCHEMAS_PLUGINS_PLUGIN_PRESET_H__

#include "schemas/plugins/plugin_identifier.h"
#include "utils/yaml.h"

typedef struct PluginPresetIdentifier_v1
{
  int              schema_version;
  int              idx;
  int              bank_idx;
  PluginIdentifier_v1 plugin_id;
} PluginPresetIdentifier_v1;

static const cyaml_schema_field_t
plugin_preset_identifier_fields_schema_v1[] =
{
  YAML_FIELD_INT (
    PluginPresetIdentifier_v1, schema_version),
  YAML_FIELD_INT (
    PluginPresetIdentifier_v1, idx),
  YAML_FIELD_INT (
    PluginPresetIdentifier_v1, bank_idx),
  YAML_FIELD_MAPPING_EMBEDDED (
    PluginPresetIdentifier_v1, plugin_id,
    plugin_identifier_fields_schema_v1),

  CYAML_FIELD_END
};

static const cyaml_schema_value_t
plugin_preset_identifier_schema_v1 = {
  YAML_VALUE_PTR (
    PluginPresetIdentifier_v1,
    plugin_preset_identifier_fields_schema_v1),
};

typedef struct PluginPreset_v1
{
  int              schema_version;
  char *           name;
  char *           uri;
  int              carla_program;
  PluginPresetIdentifier_v1 id;
} PluginPreset_v1;

static const cyaml_schema_field_t
plugin_preset_fields_schema_v1[] =
{
  YAML_FIELD_INT (
    PluginPreset_v1, schema_version),
  YAML_FIELD_STRING_PTR (
    PluginPreset_v1, name),
  YAML_FIELD_STRING_PTR_OPTIONAL (
    PluginPreset_v1, uri),
  YAML_FIELD_INT (
    PluginPreset_v1, carla_program),
  YAML_FIELD_MAPPING_EMBEDDED (
    PluginPreset_v1, id,
    plugin_preset_identifier_fields_schema_v1),

  CYAML_FIELD_END
};

static const cyaml_schema_value_t
plugin_preset_schema_v1 = {
  YAML_VALUE_PTR (
    PluginPreset_v1, plugin_preset_fields_schema_v1),
};

typedef struct PluginBank_v1
{
  int              schema_version;
  PluginPreset_v1 **  presets;
  int              num_presets;
  size_t           presets_size;
  char *           uri;
  char *           name;
  PluginPresetIdentifier_v1 id;
} PluginBank;

static const cyaml_schema_field_t
plugin_bank_fields_schema_v1[] =
{
  YAML_FIELD_STRING_PTR (
    PluginBank_v1, schema_version),
  YAML_FIELD_DYN_PTR_ARRAY_VAR_COUNT (
    PluginBank_v1, presets,
    plugin_preset_schema_v1),
  YAML_FIELD_STRING_PTR (
    PluginBank_v1, name),
  YAML_FIELD_STRING_PTR_OPTIONAL (
    PluginBank_v1, uri),
  YAML_FIELD_MAPPING_EMBEDDED (
    PluginBank_v1, id,
    plugin_preset_identifier_fields_schema_v1),

  CYAML_FIELD_END
};

static const cyaml_schema_value_t
plugin_bank_schema_v1 = {
  YAML_VALUE_PTR (
    PluginBank_v1, plugin_bank_fields_schema_v1),
};

#endif
