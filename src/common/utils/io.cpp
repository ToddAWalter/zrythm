// SPDX-FileCopyrightText: © 2018-2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense
/*
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 * ---
 *
 *  Copyright 2000 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-FileCopyrightText: 2000 Red Hat, Inc.
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * ---
 */

#include "zrythm-config.h"

#include <filesystem>

#include "common/utils/exceptions.h"
#include "common/utils/logger.h"

#ifdef _WIN32
#  include <windows.h>
#endif

#include <sys/stat.h>
#include <sys/types.h>

#include "common/utils/datetime.h"
#include "common/utils/file.h"
#include "common/utils/io.h"
#include "common/utils/string.h"
#include "gui/cpp/backend/zrythm.h"
#include "gui/cpp/gtk_widgets/gtk_wrapper.h"

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include "juce/juce.h"
#include <fmt/format.h>
#include <giomm.h>
#include <glibmm.h>

#if defined(__APPLE__) && ZRYTHM_IS_INSTALLER_VER
#  include "CoreFoundation/CoreFoundation.h"
#  include <libgen.h>
#endif

std::string
io_get_dir (const std::string &filename)
{
  return Glib::path_get_dirname (filename);
}

void
io_mkdir (const std::string &dir)
{
  // this is called during logger instantiation so check if logger exists
  if (Logger::getInstanceWithoutCreating ())
    {
      z_debug ("Creating directory: {}", dir);
    }
  std::error_code ec;
  if (!std::filesystem::create_directories (dir, ec))
    {
      if (ec.value () == 0) // Directory already exists
        {
          return;
        }
      throw ZrythmException (fmt::format (
        "Failed to make directory {} with parents: {}", dir, ec.message ()));
    }
}

std::string
io_create_tmp_dir (const std::string template_name)
{
  std::string temp_dir_template = Glib::build_filename (
    std::filesystem::temp_directory_path ().string (), template_name);
  std::string temp_dir =
    std::filesystem::path (g_mkdtemp (temp_dir_template.data ())).string ();
  if (temp_dir.empty ())
    throw ZrythmException (fmt::format (
      "Failed to create temporary directory: {}", temp_dir_template));

  return temp_dir;
}

std::string
io_file_get_ext (const std::string &filename)
{
  const char * dot = strrchr (filename.c_str (), '.');
  if (!dot || dot == filename)
    return "";

  return dot + 1;
}

bool
io_touch_file (const std::string &file_path)
{
  juce::File file (file_path);
  if (file.exists ())
    {
      // Update the file's modification time to the current time
      return file.setLastModificationTime (juce::Time::getCurrentTime ());
    }
  else
    {
      // Create a new empty file
      return file.replaceWithData (nullptr, 0);
    }
}

std::string
io_file_strip_ext (const std::string &filename)
{
  /* if last char is a dot, return the string without the dot */
  size_t len = strlen (filename.c_str ());
  if (filename[len - 1] == '.')
    {
      return filename.substr (0, len - 1);
    }

  const auto dot = io_file_get_ext (filename);

  /* if no dot, return filename */
  if (dot.empty ())
    {
      return filename;
    }

  long size = (dot.c_str () - filename.c_str ()) - 1;
  return filename.substr (0, size);
}

std::string
io_path_get_basename_without_ext (const std::string &filename)
{
  auto basename = Glib::path_get_basename (filename);
  return io_file_strip_ext (basename);
}

#if 0
char *
io_path_get_parent_dir (const char * path)
{
#  ifdef _WIN32
#    define PATH_SEP "\\\\"
#    define ROOT_REGEX "[A-Z]:" PATH_SEP
#  else
#    define PATH_SEP "/"
#    define ROOT_REGEX "/"
#  endif
  char   regex[] = "(" ROOT_REGEX ".*)" PATH_SEP "[^" PATH_SEP "]+";
  char * parent = string_get_regex_group (path, regex, 1);
#  if 0
  z_info ("[%s]\npath: {}\nregex: {}\nparent: {}",
    __func__, path, regex, parent);
#  endif

  if (!parent)
    {
      strcpy (regex, "(" ROOT_REGEX ")[^" PATH_SEP "]*");
      parent = string_get_regex_group (path, regex, 1);
#  if 0
      z_info ("path: {}\nregex: {}\nparent: {}",
        path, regex, parent);
#  endif
    }

#  undef PATH_SEP
#  undef ROOT_REGEX

  return parent;
}
#endif

char *
io_file_get_creation_datetime (const char * filename)
{
  /* TODO */
  return NULL;
}

/**
 * Returns the number of seconds since the epoch, or
 * -1 if failed.
 */
gint64
io_file_get_last_modified_datetime (const char * filename)
{
  struct stat result
  {
  };
  if (stat (filename, &result) == 0)
    {
      return result.st_mtime;
    }
  z_info ("Failed to get last modified for {}", filename);
  return -1;
}

std::string
io_file_get_last_modified_datetime_as_str (const char * filename)
{
  gint64 secs = io_file_get_last_modified_datetime (filename);
  if (secs == -1)
    return NULL;

  return datetime_epoch_to_str (secs, "%Y-%m-%d %H:%M:%S");
}

bool
io_remove (const std::string &path)
{
  if (gZrythm)
    {
      z_debug ("Removing {}...", path);
    }

  juce::File file (path);
  if (!file.exists ())
    return false;

  if (file.isDirectory ())
    throw ZrythmException (fmt::format ("Cannot remove directory {}", path));

  bool success = file.deleteFile ();
  if (success)
    return true;
  else
    {
      throw ZrythmException (fmt::format ("Failed to remove {}", path));
    };
}

bool
io_rmdir (const std::string &path, bool force)
{
  juce::File dir (path);

  if (!dir.exists () || !dir.isDirectory ())
    {
      return false;
    }
  z_info ("Removing {}{}", path, force ? " recursively" : "");

  if (force)
    {
      z_return_val_if_fail (
        Glib::path_is_absolute (path) && strlen (path.c_str ()) > 20, false);
      return dir.deleteRecursively ();
    }

  std::error_code ec;
  std::filesystem::remove (path, ec);
  return !ec;
}

/**
 * Appends files to the given array from the given dir if they end in the given
 * string.
 *
 * @param opt_end_string If empty, appends all files.
 */
static void
append_files_from_dir_ending_in (
  StringArray                      &files,
  bool                              recursive,
  const std::string                &_dir,
  const std::optional<std::string> &opt_end_string)
{
  juce::File directory (_dir);

  if (!directory.isDirectory ())
    {
      throw ZrythmException (
        fmt::format ("'{}' is not a directory (or doesn't exist)", _dir));
    }

  for (
    const auto &file : juce::RangedDirectoryIterator (
      directory, recursive,
      opt_end_string ? std::string ("*") + *opt_end_string : "*",
      juce::File::findFiles))
    {
      files.add (file.getFile ().getFullPathName ().toStdString ());
    }
}

StringArray
io_get_files_in_dir (const std::string &_dir)
{
  return io_get_files_in_dir_ending_in (_dir, false, std::nullopt);
}

void
io_copy_dir (
  const std::string_view destdir_str,
  const std::string_view srcdir_str,
  bool                   follow_symlinks,
  bool                   recursive)
{

  z_debug (
    "attempting to copy dir '{}' to '{}' (recursive: {})", srcdir_str,
    destdir_str, recursive);

  GError * err = NULL;
  GDir *   srcdir = g_dir_open (srcdir_str.data (), 0, &err);
  if (!srcdir)
    {
      throw ZrythmException (fmt::format (
        "Failed opening directory '{}': {}", srcdir_str, err->message));
    }

  io_mkdir (destdir_str.data ());

  const gchar * filename;
  while ((filename = g_dir_read_name (srcdir)))
    {
      auto src_full_path = Glib::build_filename (srcdir_str.data (), filename);
      auto dest_full_path = Glib::build_filename (destdir_str.data (), filename);

      bool is_dir = Glib::file_test (src_full_path, Glib::FileTest::IS_DIR);

      /* recurse if necessary */
      if (recursive && is_dir)
        {
          io_copy_dir (
            dest_full_path, src_full_path, follow_symlinks, recursive);
        }
      /* otherwise if not dir, copy file */
      else if (!is_dir)
        {
          GFile * src_file = g_file_new_for_path (src_full_path.c_str ());
          GFile * dest_file = g_file_new_for_path (dest_full_path.c_str ());
          z_return_if_fail (src_file && dest_file);
          int flags = (int) G_FILE_COPY_OVERWRITE;
          if (!follow_symlinks)
            {
              flags |= (int) G_FILE_COPY_NOFOLLOW_SYMLINKS;
            }
          bool success = g_file_copy (
            src_file, dest_file, (GFileCopyFlags) flags, nullptr, nullptr,
            nullptr, &err);
          if (!success)
            {
              throw ZrythmException (fmt::format (
                "Failed copying file {} to {}", src_full_path, dest_full_path,
                err->message));
            }

          g_object_unref (src_file);
          g_object_unref (dest_file);
        }
    }
  g_dir_close (srcdir);
}

StringArray
io_get_files_in_dir_as_basenames (const std::string &_dir)
{
  StringArray files = io_get_files_in_dir (_dir);

  StringArray files_as_basenames;
  for (const auto &filename : files)
    {
      files_as_basenames.add (Glib::path_get_basename (filename.toStdString ()));
    }

  return files_as_basenames;
}

StringArray
io_get_files_in_dir_ending_in (
  const std::string                &_dir,
  bool                              recursive,
  const std::optional<std::string> &end_string)
{
  StringArray arr;
  append_files_from_dir_ending_in (arr, recursive, _dir, end_string);
  return arr;
}

std::string
io_get_next_available_filepath (const std::string &filepath)
{
  int  i = 1;
  auto file_without_ext = io_file_strip_ext (filepath);
  auto file_ext = io_file_get_ext (filepath);
  auto new_path = filepath;
  while (file_path_exists (new_path))
    {
      if (Glib::file_test (new_path, Glib::FileTest::IS_DIR))
        {
          new_path = fmt::format ("{} ({})", filepath, i++);
        }
      else
        {
          new_path = fmt::format ("{} ({}).{}", file_without_ext, i++, file_ext);
        }
    }

  return new_path;
}

/**
 * Opens the given directory using the default
 * program.
 */
void
io_open_directory (const char * path)
{
  z_return_if_fail (g_file_test (path, G_FILE_TEST_IS_DIR));

  std::string command;
#ifdef _WIN32
  auto canonical_path = Glib::canonicalize_filename (path, nullptr);
  auto new_path = string_replace (canonical_path, "\\", "\\\\");
  command = fmt::format ("{} \"{}\"", OPEN_DIR_CMD, new_path);
#else
  command = fmt::format ("{} \"{}\"", OPEN_DIR_CMD, path);
#endif
  system (command.c_str ());
  z_info ("executed: {}", command);
}

void
io_write_file_atomic (const std::string &file_path, const std::string &data)
{
  try
    {
      Glib::file_set_contents (file_path, data);
    }
  catch (const std::exception &e)
    {
      throw ZrythmException (fmt::format ("Error writing file: {}", e.what ()));
    }
}

std::string
io_get_legal_file_name (const std::string &file_name)
{
  return juce::File::createLegalFileName (file_name).toStdString ();
}

std::string
io_get_legal_path_name (const std::string &path)
{
  return juce::File::createLegalPathName (path).toStdString ();
}

#ifdef _WIN32
std::string
io_get_registry_string_val (const std::string &key)
{
  auto full_path = fmt::format (
    "HKEY_LOCAL_MACHINE\\Software\\{}\\{}\\Settings\\{}", PROGRAM_NAME,
    PROGRAM_NAME, key);
  auto value = juce::WindowsRegistry::getValue (juce::String (full_path));

  if (!value.isEmpty ())
    {
      z_info ("reg value: {}", value.toStdString ());
      return value.toStdString ();
    }

  z_warning ("reg value not found: {}", full_path);
  return "";
}
#endif

#if defined(__APPLE__) && ZRYTHM_IS_INSTALLER_VER
/**
 * Gets the bundle path on MacOS.
 *
 * @return Non-zero on fail.
 */
int
io_get_bundle_path (char * bundle_path)
{
  CFBundleRef bundle = CFBundleGetMainBundle ();
  CFURLRef    bundleURL = CFBundleCopyBundleURL (bundle);
  Boolean     success = CFURLGetFileSystemRepresentation (
    bundleURL, TRUE, (UInt8 *) bundle_path, PATH_MAX);
  z_return_val_if_fail (success, -1);
  CFRelease (bundleURL);
  z_info ("bundle path: {}", bundle_path);

  return 0;
}
#endif

/**
 * Returns the new path after traversing any symlinks (using
 * readlink()).
 */
char *
io_traverse_path (const char * abs_path)
{
  /* TODO handle on other platforms as well */
#if defined(__linux__) || defined(__FreeBSD__)
  char * traversed_path = realpath (abs_path, nullptr);
  if (traversed_path)
    {
      if (!string_is_equal (traversed_path, abs_path))
        {
          z_debug ("traversed path: {} => {}", abs_path, traversed_path);
        }
      return traversed_path;
    }
  else
    {
      z_warning ("realpath() failed: {}", strerror (errno));
      return g_strdup (abs_path);
    }
#else
  return g_strdup (abs_path);
#endif
}
