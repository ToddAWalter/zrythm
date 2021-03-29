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
 * Audio clip.
 */

#ifndef __AUDIO_CLIP_H__
#define __AUDIO_CLIP_H__

#include <stdbool.h>

#include "utils/types.h"
#include "utils/yaml.h"

/**
 * @addtogroup audio
 *
 * @{
 */

#define AUDIO_CLIP_SCHEMA_VERSION 1

/**
 * Audio clips for the pool.
 *
 * These should be loaded in the project's sample
 * rate.
 */
typedef struct AudioClip
{
  int           schema_version;

  /** Name of the clip. */
  char *        name;

  /** The audio frames, interleaved. */
  sample_t *    frames;

  /** Number of frames per channel. */
  long          num_frames;

  /**
   * Per-channel frames for convenience.
   */
  sample_t *    ch_frames[16];

  /** Number of channels. */
  channels_t    channels;

  /**
   * BPM of the clip, or BPM of the project when
   * the clip was first loaded.
   */
  bpm_t         bpm;

  /**
   * Samplerate of the clip, or samplerate when
   * the clip was imported into the project.
   */
  int           samplerate;

  /** ID in the audio pool. */
  int           pool_id;

  /**
   * Frames already written to the file, per channel.
   *
   * Used when writing in chunks/parts.
   */
  long          frames_written;

  /**
   * Time the last write took place.
   *
   * This is used so that we can write every x
   * ms instead of all the time.
   *
   * @see AudioClip.frames_written.
   */
  gint64        last_write;
} AudioClip;

static const cyaml_schema_field_t
audio_clip_fields_schema[] =
{
  YAML_FIELD_INT (
    AudioClip, schema_version),
  YAML_FIELD_STRING_PTR (
    AudioClip, name),
  YAML_FIELD_FLOAT (
    AudioClip, bpm),
  YAML_FIELD_INT (
    AudioClip, samplerate),
  YAML_FIELD_INT (
    AudioClip, pool_id),

  CYAML_FIELD_END
};

static const cyaml_schema_value_t
audio_clip_schema = {
  YAML_VALUE_PTR (
    AudioClip, audio_clip_fields_schema),
};

/**
 * Inits after loading a Project.
 */
NONNULL
void
audio_clip_init_loaded (
  AudioClip * self);

/**
 * Creates an audio clip from a file.
 *
 * The name used is the basename of the file.
 */
NONNULL
AudioClip *
audio_clip_new_from_file (
  const char * full_path);

/**
 * Creates an audio clip by copying the given float
 * array.
 *
 * @param arr Interleaved array.
 * @param name A name for this clip.
 */
AudioClip *
audio_clip_new_from_float_array (
  const float *    arr,
  const long       nframes,
  const channels_t channels,
  const char *     name);

/**
 * Create an audio clip while recording.
 *
 * The frames will keep getting reallocated until
 * the recording is finished.
 *
 * @param nframes Number of frames to allocate. This
 *   should be the current cycle's frames when
 *   called during recording.
 */
AudioClip *
audio_clip_new_recording (
  const channels_t channels,
  const long       nframes,
  const char *     name);

/**
 * Updates the channel caches.
 *
 * See @ref AudioClip.ch_frames.
 *
 * @param start_from Frames to start from (per
 *   channel. The previous frames will be kept.
 */
NONNULL
void
audio_clip_update_channel_caches (
  AudioClip * self,
  size_t      start_from);

/**
 * Writes the given audio clip data to a file.
 *
 * @param parts If true, only write new data. @see
 *   AudioClip.frames_written.
 *
 * @return Non-zero if fail.
 */
NONNULL
int
audio_clip_write_to_file (
  AudioClip *  self,
  const char * filepath,
  bool         parts);

/**
 * Writes the clip to the pool as a wav file.
 *
 * @param parts If true, only write new data. @see
 *   AudioClip.frames_written.
 */
NONNULL
void
audio_clip_write_to_pool (
  AudioClip * self,
  bool        parts);

NONNULL
char *
audio_clip_get_path_in_pool_from_name (
  const char * name);

NONNULL
char *
audio_clip_get_path_in_pool (
  AudioClip * self);

/**
 * Returns whether the clip is used inside the
 * project (in actual project regions only, not
 * in undo stack).
 */
NONNULL
bool
audio_clip_is_in_use (
  AudioClip * self);

/**
 * To be called by audio_pool_remove_clip().
 *
 * Removes the file associated with the clip and
 * frees the instance.
 */
NONNULL
void
audio_clip_remove_and_free (
  AudioClip * self);

/**
 * Frees the audio clip.
 */
NONNULL
void
audio_clip_free (
  AudioClip * self);

/**
 * @}
 */

#endif
