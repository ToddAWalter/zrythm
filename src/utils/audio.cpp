// SPDX-FileCopyrightText: © 2019-2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "utils/audio.h"
#include "utils/audio_file.h"
#include "utils/exceptions.h"
#include "utils/gtest_wrapper.h"
#include "utils/logger.h"
#include "utils/math.h"
#include <sndfile.h>

#if defined(__FreeBSD__)
#  include <sys/sysctl.h>
#endif

namespace zrythm::utils::audio
{

/**
 * Returns the number of frames in the given audio
 * file.
 */
unsigned_frame_t
get_num_frames (const fs::path &filepath)
{
  SF_INFO sfinfo;
  memset (&sfinfo, 0, sizeof (sfinfo));
  sfinfo.format = sfinfo.format | SF_FORMAT_PCM_16;
  SNDFILE * sndfile = sf_open (filepath.string ().c_str (), SFM_READ, &sfinfo);
  if (!sndfile)
    {
      const char * err_str = sf_strerror (sndfile);
      z_error ("sndfile null: {}", err_str);
      return 0;
    }
  z_return_val_if_fail (sfinfo.frames > 0, 0);
  unsigned_frame_t frames = static_cast<unsigned_frame_t> (sfinfo.frames);

  int ret = sf_close (sndfile);
  z_return_val_if_fail (ret == 0, 0);

  return frames;
}

/**
 * Returns whether the frame buffers are equal.
 */
bool
frames_equal (
  const float * src1,
  const float * src2,
  size_t        num_frames,
  float         epsilon)
{
  for (size_t i = 0; i < num_frames; i++)
    {
      if (!math_floats_equal_epsilon (src1[i], src2[i], epsilon))
        {
          z_debug (
            "[{}] {:f} != {:f}", i, static_cast<double> (src1[i]),
            static_cast<double> (src2[i]));
          return false;
        }
    }
  return true;
}

/**
 * Returns whether the file contents are equal.
 *
 * @param num_frames Maximum number of frames to check. Passing
 *   0 will check all frames.
 */
bool
audio_files_equal (
  const char * f1,
  const char * f2,
  size_t       num_frames,
  float        epsilon)
{
  try
    {
      AudioFile c1{ f1 };
      AudioFile c2{ f2 };
      auto      metadata1 = c1.read_metadata ();
      auto      metadata2 = c2.read_metadata ();
      if (metadata1.channels != metadata2.channels)
        return false;

      juce::AudioSampleBuffer buf1;
      juce::AudioSampleBuffer buf2;
      c1.read_full (buf1, std::nullopt);
      c2.read_full (buf2, std::nullopt);

      if (num_frames == 0)
        {
          if (buf1.getNumSamples () == buf2.getNumSamples ())
            {
              num_frames = buf1.getNumSamples ();
            }
          else
            {
              return false;
            }
        }
      z_return_val_if_fail (num_frames > 0, false);

      for (int i = 0; i < metadata1.channels; ++i)
        {
          if (
            !frames_equal (
              buf1.getReadPointer (i), buf2.getReadPointer (i), num_frames,
              epsilon))
            return false;
        }

      return true;
    }
  catch (const ZrythmException &e)
    {
      e.handle ("An error occurred while comparing files");
      return false;
    }
}

/**
 * Returns whether the frame buffer is empty (zero).
 */
bool
frames_empty (const float * src, size_t num_frames)
{
  for (size_t i = 0; i < num_frames; i++)
    {
      if (!math_floats_equal (src[i], 0.f))
        {
          z_debug ("[{}] {:f} != 0", i, static_cast<double> (src[i]));
          return false;
        }
    }
  return true;
}

bool
audio_file_is_silent (const fs::path &filepath)
{
  SF_INFO sfinfo;
  memset (&sfinfo, 0, sizeof (sfinfo));
  sfinfo.format = sfinfo.format | SF_FORMAT_PCM_16;
  SNDFILE * sndfile = sf_open (filepath.string ().c_str (), SFM_READ, &sfinfo);
  z_return_val_if_fail (sndfile && sfinfo.frames > 0, true);

  long               buf_size = sfinfo.frames * sfinfo.channels;
  std::vector<float> data (buf_size);
  sf_count_t frames_read = sf_readf_float (sndfile, data.data (), sfinfo.frames);
  EXPECT_EQ (frames_read, sfinfo.frames);
  z_return_val_if_fail (frames_read == sfinfo.frames, true);
  z_debug ("read {} frames for {}", frames_read, filepath);

  bool is_empty = frames_empty (data.data (), static_cast<size_t> (buf_size));
  int  ret = sf_close (sndfile);
  z_return_val_if_fail (ret == 0, true);

  return is_empty;
}

float
detect_bpm (
  const float *       src,
  size_t              num_frames,
  unsigned int        samplerate,
  std::vector<float> &candidates)
{
  // unimplemented
  z_return_val_if_reached (140.f);
#if 0
  ZVampPlugin * plugin =
    vamp_get_plugin (Z_VAMP_PLUGIN_FIXED_TEMPO_ESTIMATOR, static_cast<float>(samplerate));
  size_t step_sz = vamp_plugin_get_preferred_step_size (plugin);
  size_t block_sz = vamp_plugin_get_preferred_block_size (plugin);

  /* only works with 1 channel */
  vamp_plugin_initialize (plugin, 1, step_sz, block_sz);

  ZVampOutputList * outputs = vamp_plugin_get_output_descriptors (plugin);
  vamp_plugin_output_list_print (outputs);
  vamp_plugin_output_list_free (outputs);

  long  cur_timestamp = 0;
  float bpm = 0.f;
  while ((cur_timestamp + static_cast<long>(block_sz)) < static_cast<long>(num_frames))
    {
      const float * frames[] = {
        &src[cur_timestamp],
      };
      ZVampFeatureSet * feature_set = vamp_plugin_process (
        plugin, (const float * const *) frames, cur_timestamp, samplerate);
      const ZVampFeatureList * fl =
        vamp_feature_set_get_list_for_output (feature_set, 0);
      if (fl)
        {
          vamp_feature_list_print (fl);
          const ZVampFeature * feature =
            (ZVampFeature *) g_ptr_array_index (fl->list, 0);
          bpm = feature->values[0];

          for (size_t i = 0; i < feature->num_values; i++)
            {
              candidates.push_back (feature->values[i]);
            }
        }
      cur_timestamp += static_cast<long>(step_sz);
      vamp_feature_set_free (feature_set);
    }

  z_info ("getting remaining features");
  ZVampFeatureSet * feature_set =
    vamp_plugin_get_remaining_features (plugin, samplerate);
  const ZVampFeatureList * fl =
    vamp_feature_set_get_list_for_output (feature_set, 0);
  if (fl)
    {
      vamp_feature_list_print (fl);
      const ZVampFeature * feature =
        (ZVampFeature *) g_ptr_array_index (fl->list, 0);
      bpm = feature->values[0];

      for (size_t i = 0; i < feature->num_values; i++)
        {
          candidates.push_back (feature->values[i]);
        }
    }
  vamp_feature_set_free (feature_set);

  return bpm;
#endif
}

/**
 * Returns the number of CPU cores.
 */
int
get_num_cores ()
{
  static int num_cores = 0;

  if (num_cores > 0)
    return num_cores;

  num_cores = std::thread::hardware_concurrency ();

  z_info ("Number of CPU cores found: {}", num_cores);

  return num_cores;
}

}; // namespace zrythm::utils::audio
