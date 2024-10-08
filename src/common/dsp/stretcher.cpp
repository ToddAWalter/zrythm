// SPDX-FileCopyrightText: © 2019-2022, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense
/*
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 * ---
 *
 * Copyright (C) 2018 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * ---
 */

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "common/dsp/stretcher.h"
#include "common/utils/logger.h"
#include "common/utils/math.h"

#include "juce_wrapper.h"

/**
 * Create a new Stretcher using the rubberband backend.
 *
 * @param samplerate The new samplerate.
 * @param time_ratio The ratio to multiply time by (eg if the
 *   BPM is doubled, this will be 0.5).
 * @param pitch_ratio The ratio to pitch by. This will normally
 *   be 1.0 when time-stretching).
 * @param realtime Whether to perform realtime stretching
 *   (lower quality but fast enough to be used real-time).
 */
Stretcher *
stretcher_new_rubberband (
  unsigned int samplerate,
  unsigned int channels,
  double       time_ratio,
  double       pitch_ratio,
  bool         realtime)
{
  z_return_val_if_fail (samplerate > 0, nullptr);

  Stretcher * self = new Stretcher ();

  RubberBandOptions opts = 0;

  self->backend = StretcherBackend::STRETCHER_BACKEND_RUBBERBAND;
  self->samplerate = samplerate;
  self->channels = channels;
  self->is_realtime = realtime;
  if (realtime)
    {
      opts =
        RubberBandOptionProcessRealTime | RubberBandOptionTransientsCrisp
        | RubberBandOptionDetectorCompound | RubberBandOptionPhaseLaminar
        | RubberBandOptionThreadingAlways | RubberBandOptionWindowStandard
        | RubberBandOptionSmoothingOff | RubberBandOptionFormantShifted
        | RubberBandOptionPitchHighSpeed | RubberBandOptionChannelsApart;
      self->block_size = 16000;
      self->rubberband_state =
        rubberband_new (samplerate, channels, opts, time_ratio, pitch_ratio);

      /* feed it samples so it is ready to use */
#if 0
      unsigned int samples_required =
        rubberband_get_samples_required (
          self->rubberband_state);
      float in_samples_l[samples_required];
      float in_samples_r[samples_required];
      for (unsigned int i; i < samples_required; i++)
        {
          in_samples_l[i] = 0.f;
          in_samples_r[i] = 0.f;
        }
      const float * in_samples[channels];
      in_samples[0] = in_samples_l;
      if (channels == 2)
        in_samples[1] = in_samples_r;
      rubberband_process (
        self->rubberband_state, in_samples,
        samples_required, false);
      g_usleep (1000);
      int avail =
        rubberband_available (
          self->rubberband_state);
      float * out_samples[2] = {
        in_samples_l, in_samples_r };
      size_t retrieved_out_samples =
        rubberband_retrieve (
          self->rubberband_state, out_samples,
          (unsigned int) avail);
      z_info (
        "%s: required: {}, available %d, "
        "retrieved %zu",
        __func__, samples_required, avail,
        retrieved_out_samples);
      samples_required =
        rubberband_get_samples_required (
          self->rubberband_state);
      rubberband_process (
        self->rubberband_state, in_samples,
        samples_required, false);
      g_usleep (1000);
      avail =
        rubberband_available (
          self->rubberband_state);
      retrieved_out_samples =
        rubberband_retrieve (
          self->rubberband_state, out_samples,
          (unsigned int) avail);
      z_info (
        "%s: required: {}, available %d, "
        "retrieved %zu",
        __func__, samples_required, avail,
        retrieved_out_samples);
#endif
    }
  else
    {
      opts =
        RubberBandOptionProcessOffline | RubberBandOptionStretchElastic
        | RubberBandOptionTransientsCrisp | RubberBandOptionDetectorCompound
        | RubberBandOptionPhaseLaminar | RubberBandOptionThreadingNever
        | RubberBandOptionWindowStandard | RubberBandOptionSmoothingOff
        | RubberBandOptionFormantShifted | RubberBandOptionPitchHighQuality
        | RubberBandOptionChannelsApart
      /* use finer engine if rubberband v3 */
#if RUBBERBAND_API_MAJOR_VERSION > 2 \
  || (RUBBERBAND_API_MAJOR_VERSION == 2 && RUBBERBAND_API_MINOR_VERSION >= 7)
        | RubberBandOptionEngineFiner
#endif
        ;
      self->block_size = 6000;
      self->rubberband_state =
        rubberband_new (samplerate, channels, opts, time_ratio, pitch_ratio);
      rubberband_set_max_process_size (self->rubberband_state, self->block_size);
    }
  rubberband_set_default_debug_level (0);

  z_debug (
    "created rubberband stretcher: time ratio: {:f}, latency: {}", time_ratio,
    stretcher_get_latency (self));

  return self;
}

/**
 * Perform stretching.
 *
 * @param in_samples_l The left samples.
 * @param in_samples_r The right channel samples. If
 *   this is nullptr, the audio is assumed to be mono.
 * @param in_samples_size The number of input samples
 *   per channel.
 */
signed_frame_t
stretcher_stretch (
  Stretcher *   self,
  const float * in_samples_l,
  const float * in_samples_r,
  size_t        in_samples_size,
  float *       out_samples_l,
  float *       out_samples_r,
  size_t        out_samples_wanted)
{
  z_info ("{}: in samples size: {}", __func__, in_samples_size);
  z_return_val_if_fail (in_samples_l, -1);

  /*rubberband_reset (self->rubberband_state);*/

  /* create the de-interleaved array */
  unsigned int channels = in_samples_r ? 2 : 1;
  z_return_val_if_fail (self->channels == channels, -1);
  std::array<const float *, 2> in_samples{ nullptr, nullptr };
  in_samples[0] = in_samples_l;
  if (channels == 2)
    in_samples[1] = in_samples_r;
  std::array<float *, 2> out_samples = { out_samples_l, out_samples_r };

  if (self->is_realtime)
    {
      rubberband_set_max_process_size (self->rubberband_state, in_samples_size);
    }
  else
    {
      /* tell rubberband how many input samples it
       * will receive */
      rubberband_set_expected_input_duration (
        self->rubberband_state, in_samples_size);

      rubberband_study (
        self->rubberband_state, in_samples.data (), in_samples_size, 1);
    }
  unsigned int samples_required =
    rubberband_get_samples_required (self->rubberband_state);
  z_info (
    "%s: samples required: {}, latency: {}", __func__, samples_required,
    rubberband_get_latency (self->rubberband_state));
  rubberband_process (
    self->rubberband_state, in_samples.data (), in_samples_size, false);

  /* get the output data */
  int avail = rubberband_available (self->rubberband_state);

  /* if the wanted amount of samples are not ready,
   * fill with silence */
  if (avail < (int) out_samples_wanted)
    {
      z_info ("{}: not enough samples available", __func__);
      return static_cast<signed_frame_t> (out_samples_wanted);
    }

  z_info (
    "%s: samples wanted %zu (avail {})", __func__, out_samples_wanted, avail);
  size_t retrieved_out_samples = rubberband_retrieve (
    self->rubberband_state, out_samples.data (), out_samples_wanted);
  z_warn_if_fail (retrieved_out_samples == out_samples_wanted);

  z_info ("{}: out samples size: {}", __func__, retrieved_out_samples);

  return static_cast<signed_frame_t> (retrieved_out_samples);
}

void
stretcher_set_time_ratio (Stretcher * self, double ratio)
{
  rubberband_set_time_ratio (self->rubberband_state, ratio);
}

/**
 * Get latency in number of samples.
 */
unsigned int
stretcher_get_latency (Stretcher * self)
{
  return rubberband_get_latency (self->rubberband_state);
}

/**
 * Perform stretching.
 *
 * @note This must only be used offline.
 *
 * @param in_samples_size The number of input samples
 *   per channel.
 *
 * @return The number of output samples generated per
 *   channel.
 */
signed_frame_t
stretcher_stretch_interleaved (
  Stretcher *   self,
  const float * in_samples,
  size_t        in_samples_size,
  float **      _out_samples)
{
  z_return_val_if_fail (in_samples, -1);

  z_info ("input samples: {}", in_samples_size);

  /* create the de-interleaved array */
  unsigned int       channels = self->channels;
  std::vector<float> in_buffers_l (in_samples_size, 0.f);
  std::vector<float> in_buffers_r (in_samples_size, 0.f);
  for (size_t i = 0; i < in_samples_size; i++)
    {
      in_buffers_l[i] = in_samples[i * channels];
      if (channels == 2)
        in_buffers_r[i] = in_samples[i * channels + 1];
    }
  const float * in_buffers[2] = { in_buffers_l.data (), in_buffers_r.data () };

  /* tell rubberband how many input samples it will
   * receive */
  rubberband_set_expected_input_duration (
    self->rubberband_state, in_samples_size);

  /* study first */
  size_t samples_to_read = in_samples_size;
  while (samples_to_read > 0)
    {
      /* samples to read now */
      unsigned int read_now =
        (unsigned int) MIN ((size_t) self->block_size, samples_to_read);

      /* read */
      rubberband_study (
        self->rubberband_state, in_buffers, read_now,
        read_now == samples_to_read);

      /* remaining samples to read */
      samples_to_read -= read_now;
    }
  z_warn_if_fail (samples_to_read == 0);

  /* create the out sample arrays */
  // float * out_samples[channels];
  size_t out_samples_size = (size_t) math_round_double_to_signed_64 (
    rubberband_get_time_ratio (self->rubberband_state) * in_samples_size);
  juce::AudioSampleBuffer out_samples (channels, out_samples_size);

  /* process */
  size_t processed = 0;
  size_t total_out_frames = 0;
  while (processed < in_samples_size)
    {
      size_t in_chunk_size =
        rubberband_get_samples_required (self->rubberband_state);
      size_t samples_left = in_samples_size - processed;

      if (samples_left < in_chunk_size)
        {
          in_chunk_size = samples_left;
        }

      /* move the in buffers */
      const float * tmp_in_arrays[2] = {
        in_buffers[0] + processed, in_buffers[1] + processed
      };

      /* process */
      rubberband_process (
        self->rubberband_state, tmp_in_arrays, in_chunk_size,
        samples_left == in_chunk_size);

      processed += in_chunk_size;

      /*z_info ("processed %lu, in samples %lu",*/
      /*processed, in_samples_size);*/

      size_t avail = (size_t) rubberband_available (self->rubberband_state);

      /* retrieve the output data in temporary arrays */
      std::vector<float> tmp_out_l (avail);
      std::vector<float> tmp_out_r (avail);
      float * tmp_out_arrays[2] = { tmp_out_l.data (), tmp_out_r.data () };
      size_t  out_chunk_size =
        rubberband_retrieve (self->rubberband_state, tmp_out_arrays, avail);

      /* save the result */
      for (size_t i = 0; i < channels; i++)
        {
          for (size_t j = 0; j < out_chunk_size; j++)
            {
              out_samples.setSample (
                i, j + total_out_frames, tmp_out_arrays[i][j]);
            }
        }

      total_out_frames += out_chunk_size;
    }

  z_info (
    "retrieved %zu samples (expected %zu)", total_out_frames, out_samples_size);
  z_warn_if_fail (
    /* allow 1 sample earlier */
    total_out_frames <= out_samples_size
    && total_out_frames >= out_samples_size - 1);

  /* store the output data in the given arrays */
  *_out_samples = static_cast<float *> (g_realloc (
    *_out_samples, (size_t) channels * total_out_frames * sizeof (float)));
  for (unsigned int ch = 0; ch < channels; ch++)
    {
      for (size_t i = 0; i < total_out_frames; i++)
        {
          (*_out_samples)[i * (size_t) channels + ch] =
            out_samples.getSample (ch, i);
        }
    }

  return static_cast<signed_frame_t> (total_out_frames);
}

/**
 * Frees the resampler.
 */
void
stretcher_free (Stretcher * self)
{
  if (self->rubberband_state)
    rubberband_delete (self->rubberband_state);

  delete self;
}
