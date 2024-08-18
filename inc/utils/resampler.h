// SPDX-FileCopyrightText: © 2023-2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#ifndef __UTILS_RESAMPLER_H__
#define __UTILS_RESAMPLER_H__

#include "zrythm-config.h"

#include "utils/types.h"

/**
 * @addtogroup utils
 *
 * @{
 */

/**
 * Audio resampler.
 *
 * This currently only supports 32-bit interleaved floats for
 * input and output.
 */
class Resampler
{
public:
  enum class Quality
  {
    Quick,
    Low,
    Medium,
    High,
    VeryHigh,
  };

  /**
   * Creates a new instance of a Resampler with the given
   * settings.
   *
   * Resampler.num_out_frames will be set to the number of output
   * frames required for resampling with the given settings.
   *
   * @throw ZrythmException on error.
   */
  Resampler (
    const float *      in_frames,
    const size_t       num_in_frames,
    const double       input_rate,
    const double       output_rate,
    const unsigned int num_channels,
    const Quality      quality);

  ~Resampler ();

  /**
   * To be called periodically until @ref is_done() returns
   * true.
   *
   * @throw ZrythmException on error.
   */
  void process ();

  /**
   * Indicates whether resampling is finished.
   */
  bool is_done () const;

  /**
   * @brief Gets the interleaved output frames.
   *
   * @ref is_done() must be true before calling this.
   */
  std::vector<float> get_out_frames () const;

private:
  class Impl;
  std::unique_ptr<Impl> pimpl_;
};

/**
 * @}
 */

#endif
