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
 * YAML utils.
 */

#ifndef __UTILS_YAML_H__
#define __UTILS_YAML_H__

#include <gtk/gtk.h>

#include <cyaml/cyaml.h>

/**
 * @addtogroup utils
 *
 * @{
 */

/**
 * Mapping embedded inside the struct.
 */
#define YAML_FIELD_MAPPING_EMBEDDED( \
  owner,member,schema) \
  CYAML_FIELD_MAPPING ( \
    #member, CYAML_FLAG_DEFAULT, owner, member, \
    schema)

/**
 * Mapping pointer to a struct.
 */
#define YAML_FIELD_MAPPING_PTR( \
  owner,member,schema) \
  CYAML_FIELD_MAPPING_PTR ( \
    #member, CYAML_FLAG_POINTER, owner, member, \
    schema)

/**
 * Mapping pointer to a struct.
 */
#define YAML_FIELD_MAPPING_PTR_OPTIONAL( \
  owner,member,schema) \
  CYAML_FIELD_MAPPING_PTR ( \
    #member, \
    CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, \
    owner, member, schema)

/**
 * Fixed-width array of pointers with variable count.
 *
 * @code@
 * MyStruct * my_structs[MAX_STRUCTS];
 * int        num_my_structs;
 * @endcode@
 */
#define YAML_FIELD_FIXED_SIZE_PTR_ARRAY_VAR_COUNT( \
  owner,member,schema) \
  CYAML_FIELD_SEQUENCE_COUNT ( \
    #member, CYAML_FLAG_DEFAULT, \
    owner, member, num_##member, \
    &schema, 0, CYAML_UNLIMITED)

/**
 * Fixed-width array of pointers with fixed count.
 *
 * @code@
 * MyStruct * my_structs[MAX_STRUCTS_CONST];
 * @endcode@
 */
#define YAML_FIELD_FIXED_SIZE_PTR_ARRAY( \
  owner,member,schema,size) \
  CYAML_FIELD_SEQUENCE_FIXED ( \
    #member, CYAML_FLAG_DEFAULT, \
    owner, member, \
    &schema, size)

/**
 * Dynamic-width (reallocated) array of pointers
 * with variable count.
 *
 * @code@
 * AutomationTrack ** ats;
 * int                num_ats;
 * int                ats_size;
 * @endcode@
 */
#define YAML_FIELD_DYN_PTR_ARRAY_VAR_COUNT( \
  owner,member,schema) \
  CYAML_FIELD_SEQUENCE_COUNT ( \
    #member, CYAML_FLAG_POINTER, \
    owner, member, num_##member, \
    &schema, 0, CYAML_UNLIMITED)

/**
 * Dynamic-width (reallocated) array of structs
 * with variable count.
 *
 * @code@
 * RegionIdentifier * ids;
 * int                num_ids;
 * int                ids_size;
 * @endcode@
 *
 * @note \ref schema must be declared as
 *   CYAML_VALUE_MAPPING with the flag
 *   CYAML_FLAG_DEFAULT.
 */
#define YAML_FIELD_DYN_ARRAY_VAR_COUNT( \
  owner,member,schema) \
  CYAML_FIELD_SEQUENCE_COUNT ( \
    #member, \
    CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, \
    owner, member, num_##member, \
    &schema, 0, CYAML_UNLIMITED)

/**
 * Dynamic-width (reallocated) array of pointers
 * with variable count, nullable.
 *
 * @code@
 * AutomationTrack ** ats;
 * int                num_ats;
 * int                ats_size;
 * @endcode@
 */
#define YAML_FIELD_DYN_PTR_ARRAY_VAR_COUNT_OPT( \
  owner,member,schema) \
  YAML_FIELD_DYN_ARRAY_VAR_COUNT (\
    owner,member,schema)

/**
 * Dynamic-width (reallocated) array of primitives
 * with variable count.
 *
 * @code@
 * int * ids;
 * int   num_ids;
 * int   ids_size;
 * @endcode@
 *
 * @note \ref schema must be declared as
 *   CYAML_VALUE_MAPPING with the flag
 *   CYAML_FLAG_DEFAULT.
 */
#define YAML_FIELD_DYN_ARRAY_VAR_COUNT_PRIMITIVES( \
  owner,member,schema) \
  YAML_FIELD_DYN_ARRAY_VAR_COUNT ( \
    owner, member, schema)

/**
 * Fixed sequence of pointers.
 */
#define YAML_FIELD_SEQUENCE_FIXED( \
  owner,member,schema,size) \
  CYAML_FIELD_SEQUENCE_FIXED ( \
    #member, CYAML_FLAG_DEFAULT, \
    owner, member, \
    &schema, size)

#define YAML_FIELD_INT(owner,member) \
  CYAML_FIELD_INT ( \
    #member, CYAML_FLAG_DEFAULT, \
    owner, member)

#define YAML_FIELD_UINT(owner,member) \
  CYAML_FIELD_UINT ( \
    #member, CYAML_FLAG_DEFAULT, \
    owner, member)

#define YAML_FIELD_FLOAT(owner,member) \
  CYAML_FIELD_FLOAT ( \
    #member, CYAML_FLAG_DEFAULT, \
    owner, member)

#define YAML_FIELD_STRING_PTR(owner,member) \
  CYAML_FIELD_STRING_PTR ( \
    #member, CYAML_FLAG_POINTER, \
    owner, member, 0, CYAML_UNLIMITED)

#define YAML_FIELD_STRING_PTR_OPTIONAL(owner,member) \
  CYAML_FIELD_STRING_PTR ( \
    #member, \
    CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL, \
    owner, member, 0, CYAML_UNLIMITED)

#define YAML_FIELD_ENUM(owner,member,strings) \
  CYAML_FIELD_ENUM ( \
    #member, CYAML_FLAG_DEFAULT, \
    owner, member, strings, \
    CYAML_ARRAY_LEN (strings))

#define YAML_FIELD_BITFIELD(owner,member,bitvals) \
  CYAML_FIELD_BITFIELD ( \
    #member, CYAML_FLAG_DEFAULT, \
    owner, member, bitvals, \
    CYAML_ARRAY_LEN (bitvals))

/**
 * Schema to be used as a pointer.
 */
#define YAML_VALUE_PTR( \
  cc,fields_schema) \
  CYAML_VALUE_MAPPING ( \
    CYAML_FLAG_POINTER, cc, fields_schema)

/**
 * Schema to be used as a pointer that can be
 * NULL.
 */
#define YAML_VALUE_PTR_NULLABLE( \
  cc,fields_schema) \
  CYAML_VALUE_MAPPING ( \
    CYAML_FLAG_POINTER_NULL_STR, cc, fields_schema)

/**
 * Schema to be used for arrays of structs directly
 * (not as pointers).
 *
 * For every other case, use the PTR above.
 */
#define YAML_VALUE_DEFAULT( \
  cc,fields_schema) \
  CYAML_VALUE_MAPPING ( \
    CYAML_FLAG_DEFAULT, cc, fields_schema)

/**
 * Serializes to YAML.
 *
 * @return Newly allocated YAML string, or NULL if
 *   error.
 */
NONNULL
char *
yaml_serialize (
  void *                       data,
  const cyaml_schema_value_t * schema);

NONNULL
void *
yaml_deserialize (
  const char *                 yaml,
  const cyaml_schema_value_t * schema);

NONNULL
void
yaml_print (
  void *                       data,
  const cyaml_schema_value_t * schema);

/**
 * Custom logging function for libcyaml.
 */
void yaml_cyaml_log_func (
  cyaml_log_t  level,
  void *       ctxt,
  const char * format,
  va_list      args);

void
yaml_set_log_level (
  cyaml_log_t level);

void
yaml_get_cyaml_config (
  cyaml_config_t * cyaml_config);

static const cyaml_schema_value_t
int_schema = {
  CYAML_VALUE_INT (
    CYAML_FLAG_DEFAULT, int),
};

static const cyaml_schema_value_t
uint8_t_schema = {
  CYAML_VALUE_UINT (
    CYAML_FLAG_DEFAULT, typeof (uint8_t)),
};

static const cyaml_schema_value_t
float_schema = {
  CYAML_VALUE_FLOAT (
    CYAML_FLAG_DEFAULT,
    typeof (float)),
};

static const cyaml_schema_field_t
gdk_rgba_fields_schema[] =
{
  CYAML_FIELD_FLOAT (
    "red", CYAML_FLAG_DEFAULT,
    GdkRGBA, red),
  CYAML_FIELD_FLOAT (
    "green", CYAML_FLAG_DEFAULT,
    GdkRGBA, green),
  CYAML_FIELD_FLOAT (
    "blue", CYAML_FLAG_DEFAULT,
    GdkRGBA, blue),
  CYAML_FIELD_FLOAT (
    "alpha", CYAML_FLAG_DEFAULT,
    GdkRGBA, alpha),

  CYAML_FIELD_END
};

typedef enum YamlDummyEnum
{
  YAML_DUMMY_ENUM1,
} YamlDummyEnum;

/**
 * @}
 */

#endif
