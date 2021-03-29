/*
 * Copyright (C) 2018-2020 Alexandros Theodotou <alex at zrythm dot org>
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
 */

#ifndef __UTILS_IO_H__
#define __UTILS_IO_H__

#include "zrythm-config.h"

#include <stdbool.h>
#include <stdio.h>

/**
 * @addtogroup utils
 *
 * @{
 */

/**
 * Gets directory part of filename. MUST be freed.
 *
 * @param filename Filename containing directory.
 */
NONNULL
char *
io_get_dir (const char * filename);

/**
 * Makes directory if doesn't exist.
 */
NONNULL
void
io_mkdir (const char * dir);

/**
 * Creates the file if doesn't exist
 */
NONNULL
FILE *
io_touch_file (const char * filename);

NONNULL
char *
io_path_get_parent_dir (const char * path);

/**
 * Strips extensions from given filename.
 */
NONNULL
char *
io_file_strip_ext (const char * filename);

/**
 * Returns file extension or NULL.
 */
NONNULL
const char *
io_file_get_ext (const char * file);

#define io_path_get_basename(filename) \
  g_path_get_basename (filename)

/**
 * Strips path from given filename.
 *
 * MUST be freed.
 */
NONNULL
char *
io_path_get_basename_without_ext (
  const char * filename);

NONNULL
char *
io_file_get_creation_datetime (
  const char * filename);

NONNULL
char *
io_file_get_last_modified_datetime (
  const char * filename);

/**
 * Removes the given file.
 */
NONNULL
int
io_remove (
  const char * path);

/**
 * Removes a dir, optionally forcing deletion.
 *
 * For safety reasons, this only accepts an
 * absolute path with length greater than 20 if
 * forced.
 */
NONNULL
int
io_rmdir (
  const char * path,
  bool         force);

/**
 * Returns a list of the files in the given
 * directory.
 *
 * @return a NULL terminated array of strings that
 *   must be free'd with g_strfreev(), or NULL if
 *   no files were found.
 */
#define io_get_files_in_dir(dir,allow_empty) \
  io_get_files_in_dir_ending_in ( \
    dir, 0, NULL, allow_empty)

/**
 * Returns a list of the files in the given
 * directory.
 *
 * @param dir The directory to look for.
 * @param allow_empty Whether to allow returning
 *   an empty array that has only NULL, otherwise
 *   return NULL if empty.
 *
 * @return a NULL terminated array of strings that
 *   must be free'd with g_strfreev() or NULL.
 */
char **
io_get_files_in_dir_ending_in (
  const char * _dir,
  const int    recursive,
  const char * end_string,
  bool         allow_empty);

/**
 * @note This will not work if \ref destdir_str has
 *   a file with the same filename as a directory
 *   in \ref srcdir_str.
 */
void
io_copy_dir (
  const char * destdir_str,
  const char * srcdir_str,
  bool         follow_symlinks,
  bool         recursive);

/**
 * Returns a newly allocated path that is either
 * a copy of the original path if the path does
 * not exist, or the original path appended with
 * (n), where n is a number.
 *
 * Example: "myfile" -> "myfile (1)"
 */
NONNULL
char *
io_get_next_available_filepath (
  const char * filepath);

/**
 * Opens the given directory using the default
 * program.
 */
NONNULL
void
io_open_directory (
  const char * path);

/**
 * Returns a clone of the given string after
 * removing forbidden characters.
 */
NONNULL
void
io_escape_dir_name (
  char *       dest,
  const char * dir);

/**
 * Writes \ref content to \ref file.
 *
 * If an error occurred, a string containing the
 * error info is returned.
 */
NONNULL
char *
io_write_file (
  const char * file,
  const char * content,
  size_t       content_size);

#ifdef _WOE32
char *
io_get_registry_string_val (
  const char * path);
#endif

#if defined (__APPLE__) && defined (INSTALLER_VER)
/**
 * Gets the bundle path on MacOS.
 *
 * @return Non-zero on fail.
 */
NONNULL
int
io_get_bundle_path (
  char * bundle_path);
#endif

/**
 * @}
 */

#endif
