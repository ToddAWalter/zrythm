/*
 * Copyright (C) 2020 Alexandros Theodotou <alex at zrythm dot org>
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

#ifndef __ACTION_PORT_ACTION_H__
#define __ACTION_PORT_ACTION_H__

#include "actions/undoable_action.h"
#include "audio/port_identifier.h"

/**
 * @addtogroup actions
 *
 * @{
 */

typedef enum PortActionType
{
  /** Set control port value. */
  PORT_ACTION_SET_CONTROL_VAL,
} PortActionType;

static const cyaml_strval_t
port_action_type_strings[] =
{
  { "Set control val",
    PORT_ACTION_SET_CONTROL_VAL },
};

typedef struct PortAction
{
  UndoableAction  parent_instance;

  PortActionType  type;

  PortIdentifier  port_id;

  /**
   * Real (not normalized) value before/after the
   * change.
   *
   * To be swapped on undo/redo.
   */
  float           val;
} PortAction;

static const cyaml_schema_field_t
  port_action_fields_schema[] =
{
  YAML_FIELD_MAPPING_EMBEDDED (
    PortAction, parent_instance,
    undoable_action_fields_schema),
  YAML_FIELD_ENUM (
    PortAction, type,
    port_action_type_strings),
  YAML_FIELD_MAPPING_EMBEDDED (
    PortAction, port_id,
    port_identifier_fields_schema),
  YAML_FIELD_FLOAT (
    PortAction, val),

  CYAML_FIELD_END
};

static const cyaml_schema_value_t
  port_action_schema =
{
  YAML_VALUE_PTR (
    PortAction,
    port_action_fields_schema),
};

void
port_action_init_loaded (
  PortAction * self);

/**
 * Create a new action.
 */
UndoableAction *
port_action_new_reset_control (
  PortIdentifier * port_id);

/**
 * Create a new action.
 */
UndoableAction *
port_action_new (
  PortActionType   type,
  PortIdentifier * port_id,
  float            val,
  bool             is_normalized);

int
port_action_do (
  PortAction * self);

int
port_action_undo (
  PortAction * self);

char *
port_action_stringize (
  PortAction * self);

void
port_action_free (
  PortAction * self);

/**
 * @}
 */

#endif
