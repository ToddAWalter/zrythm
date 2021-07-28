/*
 * Copyright (C) 2020 Alexandros Theodotou <alex at zrythm dot org>
 *
 * This file is part of Zrythm
 *
 * Zrythm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
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
 * AUDIO functions.
 *
 * TODO move to a more appropriate directory.
 */

#ifndef __AUDIO_AUDIO_FUNCTION_H__
#define __AUDIO_AUDIO_FUNCTION_H__

#include "utils/yaml.h"

typedef struct ArrangerSelections ArrangerSelections;
typedef struct Plugin Plugin;

/**
 * @addtogroup audio
 *
 * @{
 */

typedef enum AudioFunctionType
{
  AUDIO_FUNCTION_INVERT,
  AUDIO_FUNCTION_NORMALIZE,
  AUDIO_FUNCTION_REVERSE,

  /** Custom plugin. */
  AUDIO_FUNCTION_CUSTOM_PLUGIN,

  /* reserved */
  AUDIO_FUNCTION_INVALID,
} AudioFunctionType;

static const cyaml_strval_t
  audio_function_type_strings[] =
{
  { __("Invert"), AUDIO_FUNCTION_INVERT },
  { __("Normalize"), AUDIO_FUNCTION_NORMALIZE },
  { __("Reverse"), AUDIO_FUNCTION_REVERSE },
  { __("Custom Plugin"), AUDIO_FUNCTION_CUSTOM_PLUGIN },
  { __("Invalid"), AUDIO_FUNCTION_INVALID },
};

static inline const char *
audio_function_type_to_string (
  AudioFunctionType type)
{
  return audio_function_type_strings[type].str;
}

/**
 * Returns the URI of the plugin responsible for
 * handling the type, if any.
 */
static inline const char *
audio_function_get_plugin_uri_for_type (
  AudioFunctionType type)
{
  switch (type)
    {
    default:
      break;
    }

  return NULL;
}

/**
 * Applies the given action to the given selections.
 *
 * @param sel Selections to edit.
 * @param type Function type.
 */
int
audio_function_apply (
  ArrangerSelections * sel,
  AudioFunctionType    type,
  const char *         uri);

/**
 * @}
 */

#endif
