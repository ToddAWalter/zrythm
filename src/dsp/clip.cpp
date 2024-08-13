// SPDX-FileCopyrightText: © 2019-2022, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "dsp/audio_region.h"
#include "dsp/audio_track.h"
#include "dsp/clip.h"
#include "dsp/engine.h"
#include "dsp/tempo_track.h"
#include "dsp/tracklist.h"
#include "gui/widgets/main_window.h"
#include "io/audio_file.h"
#include "project.h"
#include "utils/audio.h"
#include "utils/debug.h"
#include "utils/dsp.h"
#include "utils/exceptions.h"
#include "utils/file.h"
#include "utils/flags.h"
#include "utils/gtk.h"
#include "utils/hash.h"
#include "utils/io.h"
#include "utils/logger.h"
#include "utils/objects.h"
#include "zrythm_app.h"

#include <glib/gi18n.h>

#include "gtk_wrapper.h"
#include <fmt/printf.h>
#include <giomm.h>
#include <glibmm.h>

void
AudioClip::update_channel_caches (size_t start_from)
{
  auto num_channels = get_num_channels ();
  z_return_if_fail_cmp (num_channels, >, 0);
  z_return_if_fail_cmp (num_frames_, >, 0);

  /* copy the frames to the channel caches */
  ch_frames_.setSize (num_channels, num_frames_, true, false, false);
  const auto frames_read_ptr =
    frames_.getReadPointer (0, start_from * num_channels);
  auto frames_to_write = num_frames_ - start_from;
  for (decltype (num_channels) i = 0; i < num_channels; ++i)
    {
      auto ch_frames_write_ptr = ch_frames_.getWritePointer (i, start_from);
      for (decltype (frames_to_write) j = 0; j < frames_to_write; ++j)
        {
          ch_frames_write_ptr[j] = frames_read_ptr[j * num_channels + i];
        }
    }
}

void
AudioClip::init_from_file (const std::string &full_path, bool set_bpm)
{
  samplerate_ = (int) AUDIO_ENGINE->sample_rate_;
  z_return_if_fail (samplerate_ > 0);

  /* read metadata */
  AudioFile file (full_path);
  try
    {
      file.read_metadata ();
    }
  catch (ZrythmException &e)
    {
      throw ZrythmException (
        fmt::sprintf ("Failed to read metadata from file '%s'", full_path));
    }
  num_frames_ = file.metadata_.num_frames;
  channels_ = file.metadata_.channels;
  bit_depth_ = audio_bit_depth_int_to_enum (file.metadata_.bit_depth);
  bpm_ = file.metadata_.bpm;

  try
    {
      /* read frames into project's samplerate */
      file.read_full (ch_frames_, samplerate_);
    }
  catch (ZrythmException &e)
    {
      throw ZrythmException (
        fmt::sprintf ("Failed to read frames from file '%s'", full_path));
    }
  num_frames_ = ch_frames_.getNumSamples ();
  channels_ = ch_frames_.getNumChannels ();

  name_ = juce::File (full_path).getFileNameWithoutExtension ().toStdString ();
  if (set_bpm)
    {
      z_return_if_fail (PROJECT && P_TEMPO_TRACK);
      bpm_ = P_TEMPO_TRACK->get_current_bpm ();
    }
  use_flac_ = use_flac (bit_depth_);

  /* interleave into frames_ */
  frames_ = ch_frames_;
  AudioFile::interleave_buffer (frames_);
}

void
AudioClip::init_loaded ()
{
  std::string filepath =
    get_path_in_pool_from_name (name_, use_flac_, F_NOT_BACKUP);

  bpm_t bpm = bpm_;
  try
    {
      init_from_file (filepath, false);
    }
  catch (ZrythmException &e)
    {
      throw ZrythmException (
        fmt::sprintf ("Failed to initialize audio file: %s", e.what ()));
    }
  bpm_ = bpm;
}

AudioClip::AudioClip (const std::string &full_path)
{
  init_from_file (full_path, false);

  pool_id_ = -1;
  bpm_ = P_TEMPO_TRACK->get_current_bpm ();
}

AudioClip::AudioClip (
  const float *          arr,
  const unsigned_frame_t nframes,
  const channels_t       channels,
  BitDepth               bit_depth,
  const std::string     &name)
{
  frames_.setSize (1, nframes * channels, true, false, false);
  num_frames_ = nframes;
  channels_ = channels;
  samplerate_ = (int) AUDIO_ENGINE->sample_rate_;
  z_return_if_fail (samplerate_ > 0);
  name_ = name;
  bit_depth_ = bit_depth;
  use_flac_ = use_flac (bit_depth);
  pool_id_ = -1;
  dsp_copy (
    frames_.getWritePointer (0), arr, (size_t) nframes * (size_t) channels);
  bpm_ = P_TEMPO_TRACK->get_current_bpm ();
  update_channel_caches (0);
}

AudioClip::AudioClip (
  const channels_t       channels,
  const unsigned_frame_t nframes,
  const std::string     &name)
{
  channels_ = channels;
  frames_.setSize (nframes * channels, 1, true, false, false);
  num_frames_ = nframes;
  name_ = name;
  pool_id_ = -1;
  bpm_ = P_TEMPO_TRACK->get_current_bpm ();
  samplerate_ = (int) AUDIO_ENGINE->sample_rate_;
  bit_depth_ = BitDepth::BIT_DEPTH_32;
  use_flac_ = false;
  z_return_if_fail (samplerate_ > 0);
  dsp_fill (
    frames_.getWritePointer (0), DENORMAL_PREVENTION_VAL (AUDIO_ENGINE),
    (size_t) nframes * (size_t) channels_);
  update_channel_caches (0);
}

std::string
AudioClip::get_path_in_pool_from_name (
  const std::string &name,
  bool               use_flac,
  bool               is_backup)
{
  std::string prj_pool_dir = PROJECT->get_path (ProjectPath::POOL, is_backup);
  if (!file_path_exists (prj_pool_dir))
    {
      z_error ("{} does not exist", prj_pool_dir);
      return "";
    }
  std::string basename =
    juce::File (name).getFileNameWithoutExtension ().toStdString ()
    + (use_flac ? ".FLAC" : ".wav");
  return Glib::build_filename (prj_pool_dir, basename);
}

std::string
AudioClip::get_path_in_pool (bool is_backup) const
{
  return get_path_in_pool_from_name (name_, use_flac_, is_backup);
}

void
AudioClip::write_to_pool (bool parts, bool is_backup)
{
  AudioClip * pool_clip = AUDIO_POOL->get_clip (pool_id_);
  z_return_if_fail (pool_clip == this);

  AUDIO_POOL->print ();
  z_debug ("attempting to write clip {} ({}) to pool...", name_, pool_id_);

  /* generate a copy of the given filename in the project dir */
  std::string path_in_main_project = get_path_in_pool (F_NOT_BACKUP);
  std::string new_path = get_path_in_pool (is_backup);
  z_return_if_fail (!path_in_main_project.empty ());
  z_return_if_fail (!new_path.empty ());

  /* whether a new write is needed */
  bool need_new_write = true;

  /* skip if file with same hash already exists */
  if (file_path_exists (new_path) && !parts)
    {
      bool same_hash =
        !file_hash_.empty ()
        && file_hash_
             == hash_get_from_file (new_path.c_str (), HASH_ALGORITHM_XXH3_64);

      if (same_hash)
        {
          z_debug ("skipping writing to existing clip {} in pool", new_path);
          need_new_write = false;
        }
    }

  /* if writing to backup and same file exists in main project dir, copy (first
   * try reflink) */
  if (need_new_write && !file_hash_.empty () && is_backup)
    {
      bool exists_in_main_project = false;
      if (file_path_exists (path_in_main_project))
        {
          exists_in_main_project =
            file_hash_
            == hash_get_from_file (path_in_main_project, HASH_ALGORITHM_XXH3_64);
        }

      if (exists_in_main_project)
        {
          /* try reflink */
          z_debug (
            "reflinking clip from main project ('{}' to '{}')",
            path_in_main_project, new_path);

          try
            {
              file_reflink (path_in_main_project, new_path);
            }
          catch (const ZrythmException &e1)
            {
              z_debug ("failed to reflink ({}), copying instead", e1.what ());

              /* copy */
              auto src_file = Gio::File::create_for_path (path_in_main_project);
              auto dest_file = Gio::File::create_for_path (new_path);
              z_debug (
                "copying clip from main project ('{}' to '{}')",
                path_in_main_project, new_path);
              try
                {
                  src_file->copy (dest_file);
                }
              catch (const Gio::Error &e2)
                {
                  throw ZrythmException (fmt::format (
                    "Failed to copy '{}' to '{}': {}", path_in_main_project,
                    new_path, e2.what ()));
                }
            } /* endif reflink fail */
        }
    }

  if (need_new_write)
    {
      z_debug (
        "writing clip %s to pool (parts %d, is backup  %d): '%s'", name_, parts,
        is_backup, new_path);
      write_to_file (new_path, parts);
      if (!parts)
        {
          /* store file hash */
          file_hash_ = hash_get_from_file (new_path, HASH_ALGORITHM_XXH3_64);
        }
    }

  AUDIO_POOL->print ();
}

void
AudioClip::write_to_file (const std::string &filepath, bool parts)
{
  z_return_if_fail (samplerate_ > 0);
  z_return_if_fail (frames_written_ < SIZE_MAX);
  size_t           before_frames = (size_t) frames_written_;
  unsigned_frame_t ch_offset = parts ? frames_written_ : 0;
  unsigned_frame_t offset = ch_offset * channels_;

  size_t nframes;
  if (parts)
    {
      z_return_if_fail_cmp (num_frames_, >=, frames_written_);
      unsigned_frame_t _nframes = num_frames_ - frames_written_;
      z_return_if_fail_cmp (_nframes, <, SIZE_MAX);
      nframes = _nframes;
    }
  else
    {
      z_return_if_fail_cmp (num_frames_, <, SIZE_MAX);
      nframes = num_frames_;
    }
  try
    {
      audio_write_raw_file (
        frames_.getReadPointer (0) + offset, ch_offset, nframes,
        (uint32_t) samplerate_, use_flac_, bit_depth_, channels_, filepath);
    }
  catch (const ZrythmException &e)
    {
      throw ZrythmException (
        fmt::format ("Failed to write audio file '{}'", filepath));
    }
  update_channel_caches (before_frames);

  if (parts)
    {
      frames_written_ = num_frames_;
      last_write_ = g_get_monotonic_time ();
    }

  /* TODO move this to a unit test for this function */
  if (ZRYTHM_TESTING)
    {
      AudioClip new_clip (filepath);
      if (num_frames_ != new_clip.num_frames_)
        {
          z_warning ("{} != {}", num_frames_, new_clip.num_frames_);
        }
      float epsilon = 0.0001f;
      z_warn_if_fail (audio_frames_equal (
        ch_frames_.getReadPointer (0), new_clip.ch_frames_.getReadPointer (0),
        (size_t) new_clip.num_frames_, epsilon));
      z_warn_if_fail (audio_frames_equal (
        frames_.getReadPointer (0), new_clip.frames_.getReadPointer (0),
        (size_t) new_clip.num_frames_ * new_clip.channels_, epsilon));
    }
}

bool
AudioClip::is_in_use (bool check_undo_stack) const
{
  for (auto track : TRACKLIST->tracks_ | type_is<AudioTrack>{})
    {
      for (auto &lane : track->lanes_)
        {
          for (auto region : lane->regions_ | type_is<AudioRegion>{})
            {
              if (region->pool_id_ == pool_id_)
                return true;
            }
        }
    }

  if (check_undo_stack)
    {
      if (UNDO_MANAGER->contains_clip (*this))
        {
          return true;
        }
    }

  return false;
}

typedef struct AppLaunchData
{
  GFile *         file;
  GtkAppChooser * app_chooser;
} AppLaunchData;

static void
app_launch_data_free (AppLaunchData * data, GClosure * closure)
{
  free (data);
}

static void
on_launch_clicked (GtkButton * btn, AppLaunchData * data)
{
  GError * err = NULL;
  GList *  file_list = NULL;
  file_list = g_list_append (file_list, data->file);
  GAppInfo * app_nfo = gtk_app_chooser_get_app_info (data->app_chooser);
  bool       success = g_app_info_launch (app_nfo, file_list, nullptr, &err);
  g_list_free (file_list);
  if (!success)
    {
      z_info ("app launch unsuccessful");
    }
}

std::unique_ptr<AudioClip>
AudioClip::edit_in_ext_program ()
{
  GError * err = NULL;
  char *   tmp_dir = g_dir_make_tmp ("zrythm-audio-clip-tmp-XXXXXX", &err);
  if (!tmp_dir)
    {
      throw ZrythmException (
        fmt::sprintf ("Failed to create tmp dir: %s", err->message));
    }
  std::string abs_path = Glib::build_filename (tmp_dir, "tmp.wav");
  try
    {
      write_to_file (abs_path, false);
    }
  catch (ZrythmException &e)
    {
      throw ZrythmException (
        fmt::sprintf ("Failed to write audio file '%s'", abs_path));
    }

  GFile *     file = g_file_new_for_path (abs_path.c_str ());
  GFileInfo * file_info = g_file_query_info (
    file, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE, G_FILE_QUERY_INFO_NONE,
    nullptr, &err);
  if (!file_info)
    {
      throw ZrythmException (
        fmt::sprintf ("Failed to query file info for %s", abs_path));
    }
  const char * content_type = g_file_info_get_content_type (file_info);

  GtkWidget * dialog = gtk_dialog_new_with_buttons (
    _ ("Edit in external app"), GTK_WINDOW (MAIN_WINDOW),
    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, _ ("_OK"),
    GTK_RESPONSE_ACCEPT, _ ("_Cancel"), GTK_RESPONSE_REJECT, nullptr);

  /* populate content area */
  GtkWidget * content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  GtkWidget * main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
  gtk_widget_set_margin_start (main_box, 4);
  gtk_widget_set_margin_end (main_box, 4);
  gtk_widget_set_margin_top (main_box, 4);
  gtk_widget_set_margin_bottom (main_box, 4);
  gtk_box_append (GTK_BOX (content_area), main_box);

  GtkWidget * lbl = gtk_label_new ("");
  gtk_label_set_selectable (GTK_LABEL (lbl), true);
  char * markup = g_markup_printf_escaped (
    _ ("Edit the file at <u>%s</u>, then press OK"), abs_path.c_str ());
  gtk_label_set_markup (GTK_LABEL (lbl), markup);
  gtk_box_append (GTK_BOX (main_box), lbl);

  GtkWidget * launch_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2);
  gtk_widget_set_halign (launch_box, GTK_ALIGN_CENTER);
  GtkWidget * app_chooser_button = gtk_app_chooser_button_new (content_type);
  gtk_box_append (GTK_BOX (launch_box), app_chooser_button);
  GtkWidget *     btn = gtk_button_new_with_label (_ ("Launch"));
  AppLaunchData * data = object_new (AppLaunchData);
  data->file = file;
  data->app_chooser = GTK_APP_CHOOSER (app_chooser_button);
  g_signal_connect_data (
    G_OBJECT (btn), "clicked", G_CALLBACK (on_launch_clicked), data,
    (GClosureNotify) app_launch_data_free, (GConnectFlags) 0);
  gtk_box_append (GTK_BOX (launch_box), btn);
  gtk_box_append (GTK_BOX (main_box), launch_box);

  int ret = z_gtk_dialog_run (GTK_DIALOG (dialog), true);
  if (ret != GTK_RESPONSE_ACCEPT)
    {
      z_debug ("operation cancelled");
      return nullptr;
    }

  /* ok - reload from file */
  return std::make_unique<AudioClip> (abs_path);
}

void
AudioClip::remove (bool backup)
{
  std::string path = get_path_in_pool (backup);
  z_debug ("removing clip at {}", path);
  z_return_if_fail (path.length () > 0);
  io_remove (path);
}