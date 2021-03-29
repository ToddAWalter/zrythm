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

#include "actions/midi_mapping_action.h"
#include "audio/channel.h"
#include "audio/router.h"
#include "gui/backend/event.h"
#include "gui/backend/event_manager.h"
#include "gui/widgets/main_window.h"
#include "plugins/plugin.h"
#include "project.h"
#include "utils/flags.h"
#include "utils/objects.h"
#include "zrythm_app.h"

#include <glib/gi18n.h>

void
midi_mapping_action_init_loaded (
  MidiMappingAction * self)
{
}

/**
 * Creates a new action.
 */
UndoableAction *
midi_mapping_action_new_enable (
  int  idx,
  bool enable)
{
  MidiMappingAction * self =
    object_new (MidiMappingAction);
  UndoableAction * ua = (UndoableAction *) self;
  ua->type = UA_MIDI_MAPPING;

  self->type =
    enable ?
      MIDI_MAPPING_ACTION_ENABLE :
      MIDI_MAPPING_ACTION_DISABLE;
  self->idx = idx;

  return ua;
}

/**
 * Creates a new action.
 */
UndoableAction *
midi_mapping_action_new_bind (
  midi_byte_t *  buf,
  ExtPort *      device_port,
  Port *         dest_port)
{
  MidiMappingAction * self =
    object_new (MidiMappingAction);
  UndoableAction * ua = (UndoableAction *) self;
  ua->type = UA_MIDI_MAPPING;

  self->type = MIDI_MAPPING_ACTION_BIND;
  memcpy (self->buf, buf, 3 * sizeof (midi_byte_t));
  if (device_port)
    {
      self->dev_port = ext_port_clone (device_port);
    }
  self->dest_port_id = dest_port->id;

  return ua;
}

/**
 * Creates a new action.
 */
UndoableAction *
midi_mapping_action_new_unbind (
  int            idx)
{
  MidiMappingAction * self =
    object_new (MidiMappingAction);
  UndoableAction * ua = (UndoableAction *) self;
  ua->type = UA_MIDI_MAPPING;

  self->type = MIDI_MAPPING_ACTION_UNBIND;
  self->idx = idx;

  return ua;
}

static void
bind_or_unbind (
  MidiMappingAction * self,
  bool                bind)
{
  if (bind)
    {
      Port * port =
        port_find_from_identifier (
          &self->dest_port_id);
      self->idx = MIDI_MAPPINGS->num_mappings;
      midi_mappings_bind (
        MIDI_MAPPINGS, self->buf, self->dev_port,
        port, F_NO_PUBLISH_EVENTS);
    }
  else
    {
      MidiMapping * mapping =
        &MIDI_MAPPINGS->mappings[self->idx];
      memcpy (
        self->buf, mapping->key,
        3 * sizeof (midi_byte_t));
      if (self->dev_port)
        {
          ext_port_free (self->dev_port);
        }
      if (mapping->device_port)
        {
          self->dev_port =
            ext_port_clone (mapping->device_port);
        }
      self->dest_port_id = mapping->dest_id;
      midi_mappings_unbind (
        MIDI_MAPPINGS, self->idx,
        F_NO_PUBLISH_EVENTS);
    }
}

int
midi_mapping_action_do (
  MidiMappingAction * self)
{
  switch (self->type)
    {
    case MIDI_MAPPING_ACTION_ENABLE:
      midi_mapping_set_enabled (
        &MIDI_MAPPINGS->mappings[self->idx], true);
      break;
    case MIDI_MAPPING_ACTION_DISABLE:
      midi_mapping_set_enabled (
        &MIDI_MAPPINGS->mappings[self->idx], false);
      break;
    case MIDI_MAPPING_ACTION_BIND:
      bind_or_unbind (self, true);
      break;
    case MIDI_MAPPING_ACTION_UNBIND:
      bind_or_unbind (self, false);
      break;
    default:
      break;
    }

  EVENTS_PUSH (ET_MIDI_BINDINGS_CHANGED, NULL);

  return 0;
}

/**
 * Edits the plugin.
 */
int
midi_mapping_action_undo (
  MidiMappingAction * self)
{
  switch (self->type)
    {
    case MIDI_MAPPING_ACTION_ENABLE:
      midi_mapping_set_enabled (
        &MIDI_MAPPINGS->mappings[self->idx], false);
      break;
    case MIDI_MAPPING_ACTION_DISABLE:
      midi_mapping_set_enabled (
        &MIDI_MAPPINGS->mappings[self->idx], true);
      break;
    case MIDI_MAPPING_ACTION_BIND:
      bind_or_unbind (self, false);
      break;
    case MIDI_MAPPING_ACTION_UNBIND:
      bind_or_unbind (self, true);
      break;
    default:
      break;
    }

  EVENTS_PUSH (ET_MIDI_BINDINGS_CHANGED, NULL);

  return 0;
}

char *
midi_mapping_action_stringize (
  MidiMappingAction * self)
{
  switch (self->type)
    {
    case MIDI_MAPPING_ACTION_ENABLE:
      return g_strdup (_("MIDI mapping enable"));
      break;
    case MIDI_MAPPING_ACTION_DISABLE:
      return g_strdup (_("MIDI mapping disable"));
      break;
    case MIDI_MAPPING_ACTION_BIND:
      return g_strdup (_("MIDI mapping bind"));
      break;
    case MIDI_MAPPING_ACTION_UNBIND:
      return g_strdup (_("MIDI mapping unbind"));
      break;
    default:
      g_warn_if_reached ();
      break;
    }

  g_return_val_if_reached (NULL);
}

void
midi_mapping_action_free (
  MidiMappingAction * self)
{
  object_free_w_func_and_null (
    ext_port_free, self->dev_port);

  object_zero_and_free (self);
}
