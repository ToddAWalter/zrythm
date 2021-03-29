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

#ifndef __AUDIO_EXPORT_H__
#define __AUDIO_EXPORT_H__

#include "audio/position.h"

/**
 * @addtogroup audio
 *
 * @{
 */

/**
 * Audio format.
 */
typedef enum AudioFormat
{
  AUDIO_FORMAT_FLAC,
  AUDIO_FORMAT_OGG_VORBIS,
  AUDIO_FORMAT_OGG_OPUS,
  AUDIO_FORMAT_WAV,
  AUDIO_FORMAT_MP3,
  AUDIO_FORMAT_MIDI,

  /** Raw audio data. */
  AUDIO_FORMAT_RAW,

  NUM_AUDIO_FORMATS,
} AudioFormat;

/**
 * Bit depth.
 */
typedef enum BitDepth
{
  BIT_DEPTH_16,
  BIT_DEPTH_24,
  BIT_DEPTH_32
} BitDepth;

/**
 * Time range to export.
 */
typedef enum ExportTimeRange
{
  TIME_RANGE_LOOP,
  TIME_RANGE_SONG,
  TIME_RANGE_CUSTOM,
} ExportTimeRange;

/**
 * Export mode.
 *
 * If this is anything other than
 * @ref EXPORT_MODE_FULL, the @ref Track.bounce
 * or @ref ZRegion.bounce_mode should be set.
 */
typedef enum ExportMode
{
  /** Export everything within the range normally. */
  EXPORT_MODE_FULL,

  /** Export selected tracks within the range
   * only. */
  EXPORT_MODE_TRACKS,

  /** Export selected regions within the range
   * only. */
  EXPORT_MODE_REGIONS,
} ExportMode;

/**
 * Export settings to be passed to the exporter
 * to use.
 */
typedef struct ExportSettings
{
  AudioFormat       format;
  char *            artist;
  char *            genre;

  /** Bit depth (16/24/64). */
  BitDepth          depth;
  ExportTimeRange   time_range;

  /** Export mode. */
  ExportMode        mode;

  /** Positions in case of custom time range. */
  Position          custom_start;
  Position          custom_end;

  /**
   * Dither or not.
   */
  bool              dither;

  /**
   * Absolute path for export file.
   */
  char *            file_uri;

  /** whether we are exporting stems. */
  //bool              stems;

  /** Progress done (0.0 to 1.0). */
  double            progress;

  /** Number of files being simultaneously exported,
   * for progress calculation. */
  int               num_files;

  /** Export cancelled. */
  bool              cancelled;

  /** Error occured. */
  bool              has_error;

  /** Error string. */
  char              error_str[1800];
} ExportSettings;

/**
 * Returns an instance of default ExportSettings.
 *
 * It must be free'd with export_settings_free().
 */
ExportSettings *
export_settings_default (void);

/**
 * Sets the defaults for bouncing.
 *
 * @note \ref ExportSettings.mode must already be
 *   set at this point.
 *
 * @param filepath Path to bounce to. If NULL, this
 *   will generate a temporary filepath.
 * @param bounce_name Name used for the file if
 *   \ref filepath is NULL.
 */
void
export_settings_set_bounce_defaults (
  ExportSettings * self,
  const char *     filepath,
  const char *     bounce_name);

void
export_settings_free_members (
  ExportSettings * self);

void
export_settings_free (
  ExportSettings * self);

/**
 * Generic export thread to be used for simple
 * exporting.
 *
 * See bounce_dialog for an example.
 */
void *
exporter_generic_export_thread (
  ExportSettings * info);

/**
 * To be called to create and perform an undoable
 * action for creating an audio track with the
 * bounced material.
 *
 * @param pos Position to place the audio region
 *   at.
 */
void
exporter_create_audio_track_after_bounce (
  ExportSettings * settings,
  const Position * pos);

/**
 * Returns the audio format as string.
 *
 * @param extension Whether to return the extension
 *   for this format, or a human friendly label.
 */
const char *
exporter_stringize_audio_format (
  AudioFormat format,
  bool        extension);

/**
 * Exports an audio file based on the given
 * settings.
 *
 * @return Non-zero if fail.
 */
int
exporter_export (ExportSettings * info);

/**
 * @}
 */

#endif
