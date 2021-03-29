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
 * @file
 *
 * Hardware processor for the routing graph.
 */

#ifndef __AUDIO_HARDWARE_PROCESSOR_H__
#define __AUDIO_HARDWARE_PROCESSOR_H__

#include <stdbool.h>

#include <gtk/gtk.h>

/**
 * @addtogroup audio
 *
 * @{
 */

#define HW_PROCESSOR_SCHEMA_VERSION 1

#define HW_IN_PROCESSOR \
  (AUDIO_ENGINE->hw_in_processor)
#define HW_OUT_PROCESSOR \
  (AUDIO_ENGINE->hw_out_processor)

/**
 * Hardware processor.
 */
typedef struct HardwareProcessor
{
  int             schema_version;

  /**
   * Whether this is the processor at the start of
   * the graph (input) or at the end (output).
   */
  bool            is_input;

  /**
   * Ports selected by the user in the preferences
   * to enable.
   *
   * To be cached at startup (need restart for changes
   * to take effect).
   *
   * This is only for inputs.
   */
  char **         selected_midi_ports;
  int             num_selected_midi_ports;
  char **         selected_audio_ports;
  int             num_selected_audio_ports;

  /**
   * All known external ports.
   */
  ExtPort **      ext_audio_ports;
  int             num_ext_audio_ports;
  size_t          ext_audio_ports_size;
  ExtPort **      ext_midi_ports;
  int             num_ext_midi_ports;
  size_t          ext_midi_ports_size;

  /**
   * Ports to be used by Zrythm, corresponding to the
   * external ports.
   */
  Port **         audio_ports;
  int             num_audio_ports;
  Port **         midi_ports;
  int             num_midi_ports;

  /** Whether set up already. */
  bool            setup;

  /** Whether currently active. */
  bool            activated;

} HardwareProcessor;

static const cyaml_schema_field_t
hardware_processor_fields_schema[] =
{
  YAML_FIELD_INT (
    HardwareProcessor, schema_version),
  YAML_FIELD_INT (
    HardwareProcessor, is_input),
  YAML_FIELD_DYN_PTR_ARRAY_VAR_COUNT_OPT (
    HardwareProcessor, ext_audio_ports,
    ext_port_schema),
  YAML_FIELD_DYN_PTR_ARRAY_VAR_COUNT_OPT (
    HardwareProcessor, ext_midi_ports,
    ext_port_schema),
  YAML_FIELD_DYN_PTR_ARRAY_VAR_COUNT_OPT (
    HardwareProcessor, audio_ports,
    port_schema),
  YAML_FIELD_DYN_PTR_ARRAY_VAR_COUNT_OPT (
    HardwareProcessor, midi_ports,
    port_schema),

  CYAML_FIELD_END
};

static const cyaml_schema_value_t
hardware_processor_schema =
{
  YAML_VALUE_PTR (
    HardwareProcessor,
    hardware_processor_fields_schema),
};

void
hardware_processor_init_loaded (
  HardwareProcessor * self);

/**
 * Returns a new empty instance.
 */
HardwareProcessor *
hardware_processor_new (
  bool      input);

/**
 * Rescans the hardware ports and appends any missing
 * ones.
 */
bool
hardware_processor_rescan_ext_ports (
  HardwareProcessor * self);

/**
 * Finds an ext port from its ID (type + full name).
 *
 * @see ext_port_get_id()
 */
ExtPort *
hardware_processor_find_ext_port (
  HardwareProcessor * self,
  const char *        id);

/**
 * Finds a port from its ID (type + full name).
 *
 * @see ext_port_get_id()
 */
Port *
hardware_processor_find_port (
  HardwareProcessor * self,
  const char *        id);

/**
 * Sets up the ports but does not start them.
 *
 * @return Non-zero on fail.
 */
int
hardware_processor_setup (
  HardwareProcessor * self);

/**
 * Starts or stops the ports.
 *
 * @param activate True to activate, false to
 *   deactivate
 */
void
hardware_processor_activate (
  HardwareProcessor * self,
  bool                activate);

/**
 * Processes the data.
 */
REALTIME
void
hardware_processor_process (
  HardwareProcessor * self,
  nframes_t           nframes);

/**
 * @}
 */

#endif
