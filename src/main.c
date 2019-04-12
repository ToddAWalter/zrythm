/*
 * Copyright (C) 2018-2019 Alexandros Theodotou
 *
 * This file is part of Zrythm
 *
 * Zrythm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zrythm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifdef _WIN32
#else
#include <execinfo.h>
#endif
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "ext/audio_decoder/ad.h"
#include "plugins/lv2/suil.h"
#include "zrythm.h"

#include <gtk/gtk.h>

/** SIGSEGV handler. */
static void
handler (int sig) {
#ifdef _WIN32
    /* TODO */
#else
  void *array[20];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 20);

  // print out all the frames to stderr
  fprintf(stderr, "Error: signal %d:\n", sig);
  backtrace_symbols_fd(array, size, STDERR_FILENO);
#endif
  exit(1);
}

/**
 * main
 */
int
main (int    argc,
      char **argv)
{
  /* install segfault handler */
  g_message ("Installing signal handler...");
  signal(SIGSEGV, handler);

  /* init suil */
  g_message ("Initing suil...");
  suil_init(&argc, &argv, SUIL_ARG_NONE);

  /* init audio decoder */
  g_message ("Initing audio decoder...");
  ad_init ();

  /* init glibtop */
  /*glibtop_init ();*/

  /* init random */
  g_message ("Initing random...");
#ifdef _WIN32
  srand (time (NULL));
#else
  srandom (time (NULL));
#endif

  // sends activate signal
  g_message ("Initing Zrythm app...");
  zrythm_app = zrythm_app_new ();
  g_message ("running Zrythm...");
  return g_application_run (
    G_APPLICATION (zrythm_app),
    argc,
    argv);
}
