/*
 * Copyright (C) 2019-2020 Alexandros Theodotou <alex@zrythm.org>
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

#ifndef __UTILS_LOG_H__
#define __UTILS_LOG_H__

#include <stdbool.h>

#include <gtk/gtk.h>

typedef struct _LogViewerWidget LogViewerWidget;
typedef struct MPMCQueue MPMCQueue;
typedef struct ObjectPool ObjectPool;

/**
 * @addtogroup utils
 *
 * @{
 */

#define LOG (zlog)

typedef struct Log
{
  FILE * logfile;

#if 0
  /* Buffers to fill in */
  GtkTextBuffer * messages_buf;
  GtkTextBuffer * warnings_buf;
  GtkTextBuffer * critical_buf;
#endif

  /** Current log file path. */
  char *          log_filepath;

  /** Message queue, for when messages are sent
   * from a non-gtk thread. */
  MPMCQueue *     mqueue;

  /** Object pool for the queue. */
  ObjectPool *    obj_pool;

  /** Used by the writer func. */
  char *          log_domains;

  bool            initialized;

  /**
   * Whether to use structured log when writing
   * to the console.
   *
   * Used during tests.
   */
  bool            use_structured_for_console;

  /**
   * Minimum log level for the console.
   *
   * Used during tests.
   */
  GLogLevelFlags  min_log_level_for_test_console;

  /** Currently opened log viewer. */
  LogViewerWidget * viewer;

  /** ID of the source function. */
  guint           writer_source_id;

  /**
   * Last timestamp a bug report popup was shown.
   *
   * This is used to avoid showing too many error popups
   * at once.
   */
  gint64          last_popup_time;
} Log;

/** Global variable, available to all files. */
extern Log * zlog;

/**
 * Initializes logging to a file.
 *
 * This must be called from the GTK thread.
 *
 * @param secs Number of timeout seconds.
 */
NONNULL
void
log_init_writer_idle (
  Log *        self,
  unsigned int secs);

/**
 * Idle callback.
 */
NONNULL
int
log_idle_cb (
  Log * self);

/**
 * Returns the last \ref n lines as a newly
 * allocated string.
 *
 * @note This must only be called from the GTK
 *   thread.
 *
 * @param n Number of lines.
 */
NONNULL
char *
log_get_last_n_lines (
  Log * self,
  int   n);

/**
 * Initializes logging to a file.
 *
 * This can be called from any thread.
 *
 * @param filepath If non-NULL, the given file
 *   will be used, otherwise the default file
 *   will be created.
 */
void
log_init_with_file (
  Log *        self,
  const char * filepath);

/**
 * Creates the logger and sets the writer func.
 *
 * This can be called from any thread.
 */
Log *
log_new (void);

/**
 * Stops logging and frees any allocated memory.
 */
NONNULL
void
log_free (
  Log * self);

/**
 * @}
 */

#endif
