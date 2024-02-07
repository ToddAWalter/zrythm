/*
 * SPDX-FileCopyrightText: © 2020 Alexandros Theodotou <alex@zrythm.org>
 *
 * SPDX-License-Identifier: LicenseRef-ZrythmLicense
 */

/**
 * \file
 *
 * System utils.
 */

#ifndef __UTILS_SYSTEM_H__
#define __UTILS_SYSTEM_H__

#include <stdbool.h>

/**
 * @addtogroup utils
 *
 * @{
 */

/**
 * Runs the given command in the background, waits for
 * it to finish and returns its exit code.
 *
 * @param ms_timer A timer in ms to
 *   kill the process, or negative to not
 *   wait.
 */
int
system_run_cmd (const char * cmd, long ms_timer);

/**
 * Runs the command and returns the output, or NULL.
 *
 * This assumes that the process will exit within a
 * few milliseconds from when the first output is
 * printed, unless \ref always_wait is true, in
 * which case the process will only be
 * reaped after the waiting time.
 *
 * @param ms_timer A timer in ms to
 *   kill the process, or negative to not
 *   wait.
 */
char *
system_get_cmd_output (char ** argv, long ms_timer, bool always_wait);

/**
 * Runs the given command in the background, waits
 * for it to finish and returns its exit code.
 *
 * @param args NULL-terminated array of args.
 * @param[out] out_stdout A pointer to save the newly
 *   allocated stdout output (if non-NULL).
 * @param[out] out_stderr A pointer to save the newly
 *   allocated stderr output (if non-NULL).
 * @param ms_timer A timer in ms to
 *   kill the process, or negative to not
 *   wait.
 */
int
system_run_cmd_w_args (
  const char ** args,
  int           ms_to_wait,
  char **       out_stdout,
  char **       out_stderr,
  bool          warn_if_fail);

/**
 * @}
 */

#endif
