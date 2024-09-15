// SPDX-FileCopyrightText: © 2021, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "zrythm-test-config.h"

#include "dsp/fader.h"
#include "dsp/midi_event.h"
#include "dsp/router.h"
#include "utils/math.h"

#include "tests/helpers/plugin_manager.h"
#include "tests/helpers/zrythm_helper.h"

#include "ext/whereami/whereami.h"

TEST (ZrythmApp, version)
{
  char * exe_path = NULL;
  int    dirname_length, length;
  length = wai_getExecutablePath (nullptr, 0, &dirname_length);
  if (length > 0)
    {
      exe_path = (char *) malloc ((size_t) length + 1);
      wai_getExecutablePath (exe_path, length, &dirname_length);
      exe_path[length] = '\0';
    }
  ASSERT_NONNULL (exe_path);

  const char * arg1 = "--version";
  int          argc = 2;
  char *       argv[] = { exe_path, (char *) arg1 };

  auto app = zrythm_app_new (argc, (const char **) argv);
  int  ret = g_application_run (G_APPLICATION (app.get ()), argc, argv);
  ASSERT_EQ (ret, 0);
}