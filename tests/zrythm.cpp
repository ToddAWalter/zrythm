// SPDX-FileCopyrightText: © 2021-2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "zrythm-test-config.h"

# include "gui/dsp/tempo_track.h"
# include "gui/dsp/track.h"
#include "utils/flags.h"
#include "gui/backend/backend/project.h"
#include "gui/backend/backend/zrythm.h"

#include "tests/helpers/zrythm_helper.h"

TEST_F (ZrythmFixture, Dummy) { }

TEST (ZrythmTest, FetchLatestRelease)
{
  /* TODO below is async now */
#if 0
#  ifdef CHECK_UPDATES
  char * ver = zrythm_fetch_latest_release_ver ();
  ASSERT_NONNULL (ver);
  ASSERT_LT (strlen (ver), 20);
#  endif
#endif
}
