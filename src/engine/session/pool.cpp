// SPDX-FileCopyrightText: © 2019-2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "engine/session/clip.h"
#include "engine/session/pool.h"
#include "gui/backend/backend/project.h"
#include "structure/tracks/tempo_track.h"
#include "utils/io.h"
#include "utils/string.h"

using namespace zrythm;

AudioPool::AudioPool (
  ProjectPoolPathGetter path_getter,
  SampleRateGetter      sr_getter)
    : sample_rate_getter_ (std::move (sr_getter)),
      project_pool_path_getter_ (std::move (path_getter))
{
}

void
AudioPool::init_loaded ()
{
  for (auto &clip : clips_)
    {
      if (clip)
        {
          clip->init_loaded (get_clip_path_from_name (
            clip->get_name (), clip->get_use_flac (), false));
        }
    }
}

void
init_from (
  AudioPool             &obj,
  const AudioPool       &other,
  utils::ObjectCloneType clone_type)
{
  std::ranges::for_each (other.clips_, [&] (const auto &other_clip) {
    auto new_clip = utils::clone_unique (*other_clip, clone_type);
    obj.register_clip (std::move (new_clip));
  });
}

bool
AudioPool::name_exists (const utils::Utf8String &name) const
{
  return std::ranges::contains (clips_, name, &AudioClip::get_name);
}

fs::path
AudioPool::get_clip_path_from_name (
  const utils::Utf8String &name,
  bool                     use_flac,
  bool                     is_backup) const
{
  auto prj_pool_dir = project_pool_path_getter_ (is_backup);
  if (!utils::io::path_exists (prj_pool_dir))
    {
      z_error ("{} does not exist", prj_pool_dir);
      return {};
    }
  const auto basename =
    utils::Utf8String::from_path (utils::io::file_strip_ext (name))
    + (use_flac ? u8".FLAC" : u8".wav");
  return prj_pool_dir / basename;
}

fs::path
AudioPool::get_clip_path (const AudioClip &clip, bool is_backup) const
{
  return get_clip_path_from_name (
    clip.get_name (), clip.get_use_flac (), is_backup);
}

void
AudioPool::write_clip (AudioClip &clip, bool parts, bool backup)
{
  AudioClip * pool_clip = get_clip (clip.get_uuid ());
  z_return_if_fail (pool_clip == &clip);

  print ();
  z_debug (
    "attempting to write clip {} ({}) to pool...", clip.get_name (),
    clip.get_uuid ());

  /* generate a copy of the given filename in the project dir */
  auto path_in_main_project = get_clip_path (clip, false);
  auto new_path = get_clip_path (clip, backup);
  z_return_if_fail (!path_in_main_project.empty ());
  z_return_if_fail (!new_path.empty ());

  /* whether a new write is needed */
  bool need_new_write = true;

  /* skip if file with same hash already exists */
  if (utils::io::path_exists (new_path) && !parts)
    {
      bool same_hash =
        clip.get_file_hash () != 0
        && clip.get_file_hash () == utils::hash::get_file_hash (new_path);

      if (same_hash)
        {
          z_debug ("skipping writing to existing clip {} in pool", new_path);
          need_new_write = false;
        }
    }

  /* if writing to backup and same file exists in main project dir, copy (first
   * try reflink) */
  if (need_new_write && clip.get_file_hash () != 0 && backup)
    {
      bool exists_in_main_project = false;
      if (utils::io::path_exists (path_in_main_project))
        {
          exists_in_main_project =
            clip.get_file_hash ()
            == utils::hash::get_file_hash (path_in_main_project);
        }

      if (exists_in_main_project)
        {
          /* try reflink and fall back to normal copying */
          z_debug (
            "reflinking clip from main project ('{}' to '{}')",
            path_in_main_project, new_path);

          if (!utils::io::reflink_file (path_in_main_project, new_path))
            {
              z_debug ("failed to reflink, copying instead");
              z_debug (
                "copying clip from main project ('{}' to '{}')",
                path_in_main_project, new_path);
              utils::io::copy_file (new_path, path_in_main_project);
            }
        }
    }

  if (need_new_write)
    {
      z_debug (
        "writing clip {} to pool (parts {}, is backup  {}): '{}'",
        clip.get_name (), parts, backup, new_path);
      clip.write_to_file (new_path, parts);
      if (!parts)
        {
          /* store file hash */
          clip.set_file_hash (utils::hash::get_file_hash (new_path));
        }
    }

  print ();
}

void
AudioPool::ensure_unique_clip_name (AudioClip &clip)
{
  constexpr bool is_backup = false;
  auto orig_name_without_ext = utils::io::file_strip_ext (clip.get_name ());
  auto orig_path_in_pool = get_clip_path (clip, is_backup);
  auto new_name = utils::Utf8String::from_path (orig_name_without_ext);
  z_return_if_fail (!new_name.empty ());

  bool changed = false;
  while (name_exists (new_name))
    {
      const auto prev_new_name = new_name;
      const auto regex =
        utils::Utf8String::from_utf8_encoded_string (R"(^.*\((\d+)\)$)");
      int cur_val = new_name.get_regex_group_as_int (regex, 1, 0);

      // Extract base name (without existing suffix)
      std::string base_name;
      if (cur_val > 0)
        {
          size_t suffix_pos =
            new_name.str ().rfind (fmt::format (" ({})", cur_val));
          base_name = new_name.str ().substr (0, suffix_pos);
        }
      else
        {
          base_name = new_name.str ();
        }

      new_name = utils::Utf8String::from_utf8_encoded_string (
        fmt::format ("{} ({})", base_name, cur_val + 1));
      changed = true;
    }

  auto new_path_in_pool =
    get_clip_path_from_name (new_name, clip.get_use_flac (), is_backup);
  if (changed)
    {
      z_return_if_fail (new_path_in_pool != orig_path_in_pool);
    }

  clip.set_name (new_name);
}

void
AudioPool::register_clip (std::shared_ptr<AudioClip> clip)
{
  assert (!clip->get_name ().empty ());

  z_debug ("adding clip <{}> to pool...", clip->get_name ());

  ensure_unique_clip_name (*clip);

  clips_.insert (clip->get_uuid (), clip);
  print ();
}

AudioClip *
AudioPool::get_clip (const AudioClip::Uuid &clip_id)
{
  auto val = clips_.value (clip_id);
  return val.get ();
}

auto
AudioPool::duplicate_clip (const AudioClip::Uuid &clip_id, bool write_file)
  -> AudioClip::Uuid
{
  auto * const clip = get_clip (clip_id);

  auto new_clip = std::make_shared<AudioClip> (
    clip->get_samples (), clip->get_bit_depth (), sample_rate_getter_ (),
    P_TEMPO_TRACK->get_current_bpm (), clip->get_name ());
  register_clip (new_clip);

  z_debug (
    "duplicating clip {} to {}...", clip->get_name (), new_clip->get_name ());

  /* assert clip names are not the same */
  assert (clip->get_name () != new_clip->get_name ());

  if (write_file)
    {
      write_clip (*new_clip, false, false);
    }

  return new_clip->get_uuid ();
}

void
AudioPool::remove_clip (
  const AudioClip::Uuid &clip_id,
  bool                   free_and_remove_file,
  bool                   backup)
{
  z_debug ("removing clip with ID {}", clip_id);

  auto * clip = get_clip (clip_id);

  if (free_and_remove_file)
    {
      const auto path = get_clip_path (*clip, backup);
      z_debug ("removing clip at {}", path);
      assert (!path.empty ());
      utils::io::remove (path);
    }

  clips_.remove (clip_id);
}

void
AudioPool::remove_unused (bool backup)
{
  z_debug ("--- removing unused files from pool ---");

  /* remove clips from the pool that are not in use */
  int removed_clips = 0;
  for (const auto &clip : clips_)
    {
      if (!PROJECT->is_audio_clip_in_use (*clip, true))
        {
          z_debug ("unused clip: {}", clip->get_name ());
          remove_clip (clip->get_uuid (), true, backup);
          ++removed_clips;
        }
    }

  /* remove untracked files from pool directory */
  auto prj_pool_dir = project_pool_path_getter_ (backup);
  auto files =
    utils::io::get_files_in_dir_ending_in (prj_pool_dir, true, std::nullopt);
  for (const auto &path : files)
    {
      bool found = false;
      for (const auto &clip : clips_)
        {
          if (!clip)
            continue;

          if (get_clip_path (*clip, backup) == path)
            {
              found = true;
              break;
            }
        }

      /* if file not found in pool clips, delete */
      if (!found)
        {
          utils::io::remove (path);
        }
    }

  z_info ("removed {} clips", removed_clips);
}

void
AudioPool::reload_clip_frame_bufs ()
{
  for (auto &clip : clips_)
    {
      if (!clip)
        continue;

      const auto in_use = PROJECT->is_audio_clip_in_use (*clip, false);

      if (in_use && clip->get_num_frames () == 0)
        {
          /* load from the file */
          clip->init_loaded (get_clip_path_from_name (
            clip->get_name (), clip->get_use_flac (), false));
        }
      else if (!in_use && clip->get_num_frames () > 0)
        {
          /* unload frames */
          clip->clear_frames ();
        }
    }
}

struct WriteClipData
{
  AudioClip * clip;
  bool        is_backup;

  /** To be set after writing the file. */
  bool        successful = false;
  std::string error;
};

void
AudioPool::write_to_disk (bool is_backup)
{
  /* ensure pool dir exists */
  auto prj_pool_dir = project_pool_path_getter_ (is_backup);
  if (!utils::io::path_exists (prj_pool_dir))
    {
      try
        {
          utils::io::mkdir (prj_pool_dir);
        }
      catch (const ZrythmException &e)
        {
          std::throw_with_nested (
            ZrythmException ("Failed to create pool directory"));
        }
    }

  const int        num_threads = juce::SystemStats::getNumCpus ();
  juce::ThreadPool pool (num_threads);

  std::string           error_message;
  juce::CriticalSection error_mutex;

  for (auto &clip : clips_)
    {
      if (clip)
        {
          pool.addJob ([this, &clip, is_backup, &error_message, &error_mutex] () {
            try
              {
                write_clip (*clip, false, is_backup);
              }
            catch (const ZrythmException &e)
              {
                const juce::ScopedLock lock (error_mutex);
                if (error_message.empty ())
                  {
                    error_message = fmt::format (
                      "Failed to write clip {} to disk: {}", clip->get_name (),
                      e.what ());
                  }
              }
          });
        }
    }

  z_debug ("waiting for tasks to finish...");
  pool.removeAllJobs (false, -1);
  z_debug ("done");

  if (!error_message.empty ())
    {
      throw ZrythmException (error_message);
    }
}

void
AudioPool::print () const
{
  std::stringstream ss;
  ss << "[Audio Pool]\n";
  for (const auto &clip : clips_)
    {
      auto pool_path = get_clip_path (*clip, false);
      ss << fmt::format (
        "[Clip #{}] {} ({}): {}\n", clip->get_uuid (), clip->get_name (),
        clip->get_file_hash (), pool_path);
    }
  z_info ("{}", ss.str ());
}

void
to_json (nlohmann::json &j, const AudioPool &pool)
{
  j[AudioPool::kClipsKey] = pool.clips_.values ();
}
void
from_json (const nlohmann::json &j, AudioPool &pool)
{
  for (const auto &clip_json : j.at (AudioPool::kClipsKey))
    {
      auto clip = std::make_shared<AudioClip> ();
      from_json (clip_json, *clip);
      pool.clips_.insert (clip->get_uuid (), clip);
    }
}
