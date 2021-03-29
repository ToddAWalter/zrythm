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

#include "zrythm-config.h"

#include <math.h>

#include "utils/dsp.h"
#include "utils/math.h"
#include "zrythm.h"

#include <gtk/gtk.h>

#ifdef HAVE_LSP_DSP
#include <lsp-plug.in/dsp/dsp.h>
#endif

/**
 * Fill the buffer with the given value.
 */
void
dsp_fill (
  float * buf,
  float   val,
  size_t  size)
{
#ifdef HAVE_LSP_DSP
  if (ZRYTHM_USE_OPTIMIZED_DSP)
    {
      lsp_dsp_fill (buf, val, size);
    }
  else
    {
#endif
      for (size_t i = 0; i < size; i++)
        {
          buf[i] = val;
        }
#ifdef HAVE_LSP_DSP
    }
#endif
}

/**
 * Clamp the buffer to min/max.
 */
void
dsp_limit1 (
  float * buf,
  float   minf,
  float   maxf,
  size_t  size)
{
#ifdef HAVE_LSP_DSP
#if 0
  if (ZRYTHM_USE_OPTIMIZED_DSP)
    {
      lsp_dsp_limit1 (buf, minf, maxf, size);
    }
  else
#endif
    {
#endif
      for (size_t i = 0; i < size; i++)
        {
          buf[i] = CLAMP (buf[i], minf, maxf);
        }
#ifdef HAVE_LSP_DSP
    }
#endif
}

/**
 * Gets the absolute max of the buffer.
 *
 * @return Whether the peak changed.
 */
bool
dsp_abs_max (
  float * buf,
  float * cur_peak,
  size_t  size)
{
  float new_peak = *cur_peak;

#ifdef HAVE_LSP_DSP
  if (ZRYTHM_USE_OPTIMIZED_DSP)
    {
      new_peak =
        lsp_dsp_abs_max (buf, size);
    }
  else
    {
#endif
      for (size_t i = 0; i < size; i++)
        {
          float val = fabsf (buf[i]);
          if (val > new_peak)
            {
              new_peak = val;
            }
        }
#ifdef HAVE_LSP_DSP
    }
#endif

  bool changed =
    !math_floats_equal (new_peak, *cur_peak);
  *cur_peak = new_peak;

  return changed;
}

/**
 * Gets the minimum of the buffer.
 */
float
dsp_min (
  float * buf,
  size_t  size)
{
  float min = 1000.f;
#ifdef HAVE_LSP_DSP
  if (ZRYTHM_USE_OPTIMIZED_DSP)
    {
      min = lsp_dsp_min (buf, size);
    }
  else
    {
#endif
      for (size_t i = 0; i < size; i++)
        {
          if (buf[i] < min)
            {
              min = buf[i];
            }
        }
#ifdef HAVE_LSP_DSP
    }
#endif

  return min;
}

/**
 * Gets the maximum of the buffer.
 */
float
dsp_max (
  float * buf,
  size_t  size)
{
  float max = - 1000.f;
#ifdef HAVE_LSP_DSP
  if (ZRYTHM_USE_OPTIMIZED_DSP)
    {
      max = lsp_dsp_max (buf, size);
    }
  else
    {
#endif
      for (size_t i = 0; i < size; i++)
        {
          if (buf[i] > max)
            {
              max = buf[i];
            }
        }
#ifdef HAVE_LSP_DSP
    }
#endif

  return max;
}

/**
 * Compute dest[i] = src[i].
 */
void
dsp_copy (
  float *       dest,
  const float * src,
  size_t        size)
{
#ifdef HAVE_LSP_DSP
  if (ZRYTHM_USE_OPTIMIZED_DSP)
    {
      lsp_dsp_copy (dest, src, size);
    }
  else
    {
#endif
      for (size_t i = 0; i < size; i++)
        {
          dest[i] = src[i];
        }
#ifdef HAVE_LSP_DSP
    }
#endif
}

/**
 * Calculate dst[i] = dst[i] + src[i].
 */
void
dsp_add2 (
  float *       dest,
  const float * src,
  size_t        size)
{
#ifdef HAVE_LSP_DSP
  if (ZRYTHM_USE_OPTIMIZED_DSP)
    {
      lsp_dsp_add2 (dest, src, size);
    }
  else
    {
#endif
      for (size_t i = 0; i < size; i++)
        {
          dest[i] = dest[i] + src[i];
        }
#ifdef HAVE_LSP_DSP
    }
#endif
}

/**
 * Scale: dst[i] = dst[i] * k.
 */
void
dsp_mul_k2 (
  float * dest,
  float   k,
  size_t  size)
{
#ifdef HAVE_LSP_DSP
  if (ZRYTHM_USE_OPTIMIZED_DSP)
    {
      lsp_dsp_mul_k2 (dest, k, size);
    }
  else
    {
#endif
      for (size_t i = 0; i < size; i++)
        {
          dest[i] *= k;
        }
#ifdef HAVE_LSP_DSP
    }
#endif
}

/**
 * Calculate dest[i] = dest[i] * k1 + src[i] * k2.
 */
void
dsp_mix2 (
  float *       dest,
  const float * src,
  float         k1,
  float         k2,
  size_t        size)
{
#ifdef HAVE_LSP_DSP
  if (ZRYTHM_USE_OPTIMIZED_DSP)
    {
      lsp_dsp_mix2 (dest, src, k1, k2, size);
    }
  else
    {
#endif
      for (size_t i = 0; i < size; i++)
        {
          dest[i] = dest[i] * k1 + src[i] * k2;
        }
#ifdef HAVE_LSP_DSP
    }
#endif
}

/**
 * Calculate
 * dst[i] = dst[i] + src1[i] * k1 + src2[i] * k2.
 */
void
dsp_mix_add2 (
  float *       dest,
  const float * src1,
  const float * src2,
  float         k1,
  float         k2,
  size_t        size)
{
#ifdef HAVE_LSP_DSP
  if (ZRYTHM_USE_OPTIMIZED_DSP)
    {
      lsp_dsp_mix_add2 (
        dest, src1, src2, k1, k2, size);
    }
  else
    {
#endif
      for (size_t i = 0; i < size; i++)
        {
          dest[i] =
            dest[i] + src1[i] * k1 + src2[i] * k2;
        }
#ifdef HAVE_LSP_DSP
    }
#endif
}

/**
 * Makes the two signals mono.
 *
 * @param equal_power True for equal power, false
 *   for equal amplitude.
 *
 * @note Equal amplitude is more suitable for mono
 * compatibility checking. For reference:
 * equal power sum =
 * (L+R) * 0.7079 (-3dB)
 * equal amplitude sum =
 * (L+R) /2 (-6.02dB)
 */
void
dsp_make_mono (
  float * l,
  float * r,
  size_t  size,
  bool    equal_power)
{
  float multiple = equal_power ? 0.7079f : 0.5f;
  dsp_mix2 (l, r, multiple, multiple, size);
  dsp_copy (r, l, size);
}
