// SPDX-FileCopyrightText: © 2018-2022 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

/**
 * \file
 *
 * Transport schema.
 */

#ifndef __SCHEMAS_AUDIO_TRANSPORT_H__
#define __SCHEMAS_AUDIO_TRANSPORT_H__

#include <cstdint>

#include "gui/backend/backend/cyaml_schemas/dsp/port.h"
#include "gui/backend/backend/cyaml_schemas/dsp/position.h"
#include "utils/types.h"

typedef struct Transport_v1
{
  int         schema_version;
  int         total_bars;
  Position_v1 playhead_pos;
  Position_v1 cue_pos;
  Position_v1 loop_start_pos;
  Position_v1 loop_end_pos;
  Position_v1 punch_in_pos;
  Position_v1 punch_out_pos;
  Position_v1 range_1;
  Position_v1 range_2;
  int         has_range;
  nframes_t   position;
  Port_v1 *   roll;
  Port_v1 *   stop;
  Port_v1 *   backward;
  Port_v1 *   forward;
  Port_v1 *   loop_toggle;
  Port_v1 *   rec_toggle;
} Transport_v1;

static const cyaml_schema_field_t transport_fields_schema_v1[] = {
  YAML_FIELD_INT (Transport_v1, schema_version),
  YAML_FIELD_INT (Transport_v1, total_bars),
  YAML_FIELD_MAPPING_EMBEDDED (
    Transport_v1,
    playhead_pos,
    position_fields_schema_v1),
  YAML_FIELD_MAPPING_EMBEDDED (Transport_v1, cue_pos, position_fields_schema_v1),
  YAML_FIELD_MAPPING_EMBEDDED (
    Transport_v1,
    loop_start_pos,
    position_fields_schema_v1),
  YAML_FIELD_MAPPING_EMBEDDED (
    Transport_v1,
    loop_end_pos,
    position_fields_schema_v1),
  YAML_FIELD_MAPPING_EMBEDDED (
    Transport_v1,
    punch_in_pos,
    position_fields_schema_v1),
  YAML_FIELD_MAPPING_EMBEDDED (
    Transport_v1,
    punch_out_pos,
    position_fields_schema_v1),
  YAML_FIELD_MAPPING_EMBEDDED (Transport_v1, range_1, position_fields_schema_v1),
  YAML_FIELD_MAPPING_EMBEDDED (Transport_v1, range_2, position_fields_schema_v1),
  YAML_FIELD_INT (Transport_v1, has_range),
  YAML_FIELD_INT (Transport_v1, position),
  YAML_FIELD_MAPPING_PTR_OPTIONAL (Transport_v1, roll, port_fields_schema_v1),
  YAML_FIELD_MAPPING_PTR_OPTIONAL (Transport_v1, stop, port_fields_schema_v1),
  YAML_FIELD_MAPPING_PTR_OPTIONAL (Transport_v1, backward, port_fields_schema_v1),
  YAML_FIELD_MAPPING_PTR_OPTIONAL (Transport_v1, forward, port_fields_schema_v1),
  YAML_FIELD_MAPPING_PTR_OPTIONAL (Transport_v1, loop_toggle, port_fields_schema_v1),
  YAML_FIELD_MAPPING_PTR_OPTIONAL (Transport_v1, rec_toggle, port_fields_schema_v1),

  CYAML_FIELD_END
};

static const cyaml_schema_value_t transport_schema_v1 = {
  YAML_VALUE_PTR (Transport_v1, transport_fields_schema_v1),
};

#endif
