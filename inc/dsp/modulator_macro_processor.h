// SPDX-FileCopyrightText: © 2021-2022 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

/**
 * \file
 *
 * Modulator macro button processor.
 */

#ifndef __AUDIO_MODULATOR_MACRO_PROCESSOR_H__
#define __AUDIO_MODULATOR_MACRO_PROCESSOR_H__

#include "dsp/port.h"
#include "utils/yaml.h"

typedef struct Track Track;

/**
 * @addtogroup dsp
 *
 * @{
 */

#define MODULATOR_MACRO_PROCESSOR_SCHEMA_VERSION 1

#define modulator_macro_processor_is_in_active_project(self) \
  (self->track && track_is_in_active_project (self->track))

/**
 * Modulator macro button processor.
 *
 * Has 1 control input, many CV inputs and 1 CV
 * output.
 *
 * Can only belong to modulator track.
 */
typedef struct ModulatorMacroProcessor
{
  int schema_version;

  /**
   * Name to be shown in the modulators tab.
   *
   * @note This is only cosmetic and should not be
   * used anywhere during processing.
   */
  char * name;

  /** CV input port for connecting CV signals to. */
  Port * cv_in;

  /**
   * CV output after macro is applied.
   *
   * This can be routed to other parameters to apply
   * the macro.
   */
  Port * cv_out;

  /** Control port controlling the amount. */
  Port * macro;

  /** Pointer to owner track, if any. */
  Track * track;

} ModulatorMacroProcessor;

static inline const char *
modulator_macro_processor_get_name (ModulatorMacroProcessor * self)
{
  return self->name;
}

COLD void
modulator_macro_processor_init_loaded (
  ModulatorMacroProcessor * self,
  Track *                   track);

void
modulator_macro_processor_set_name (
  ModulatorMacroProcessor * self,
  const char *              name);

Track *
modulator_macro_processor_get_track (ModulatorMacroProcessor * self);

/**
 * Process.
 */
void
modulator_macro_processor_process (
  ModulatorMacroProcessor *           self,
  const EngineProcessTimeInfo * const time_nfo);

ModulatorMacroProcessor *
modulator_macro_processor_new (Track * track, int idx);

void
modulator_macro_processor_free (ModulatorMacroProcessor * self);

/**
 * @}
 */

#endif
