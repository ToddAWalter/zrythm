/*
 * Copyright (C) 2019 Alexandros Theodotou <alex at zrythm dot org>
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

#ifndef __PLUGINS_LV2_URID_H__
#define __PLUGINS_LV2_URID_H__

#include <lv2/urid/urid.h>

/**
 * @addtogroup lv2
 *
 * @{
 */

/**
 * Cached URIDs for quick access (instead of having
 * to use symap).
 */
typedef struct Lv2URIDs
{
  LV2_URID atom_Float;
  LV2_URID atom_Int;
  LV2_URID atom_Object;
  LV2_URID atom_Path;
  LV2_URID atom_String;
  LV2_URID atom_eventTransfer;
  LV2_URID bufsz_maxBlockLength;
  LV2_URID bufsz_minBlockLength;
  LV2_URID bufsz_sequenceSize;
  LV2_URID log_Error;
  LV2_URID log_Trace;
  LV2_URID log_Warning;
  LV2_URID midi_MidiEvent;
  LV2_URID param_sampleRate;
  LV2_URID patch_Get;
  LV2_URID patch_Put;
  LV2_URID patch_Set;
  LV2_URID patch_body;
  LV2_URID patch_property;
  LV2_URID patch_value;
  LV2_URID time_Position;
  LV2_URID time_bar;
  LV2_URID time_barBeat;
  LV2_URID time_beatUnit;
  LV2_URID time_beatsPerBar;
  LV2_URID time_beatsPerMinute;
  LV2_URID time_frame;
  LV2_URID time_speed;
  LV2_URID ui_updateRate;
  LV2_URID ui_scaleFactor;

  /* Non-standard URIs */
  LV2_URID z_hostInfo_name;
  LV2_URID z_hostInfo_version;
} Lv2URIDs;

/**
 * URID feature map implementation.
 *
 * LV2_URID is just an int (uint32_t) for the
 * given uri.
 */
LV2_URID
lv2_urid_map_uri (
  LV2_URID_Map_Handle handle,
  const char*         uri);

/**
 * URID feature unmap implementation.
 */
const char *
lv2_urid_unmap_uri (
  LV2_URID_Unmap_Handle handle,
  LV2_URID              urid);

/**
 * @}
 */

#endif
