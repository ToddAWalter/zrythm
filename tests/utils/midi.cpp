// SPDX-FileCopyrightText: © 2020-2022 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "zrythm-test-config.h"

#include "project.h"
#include "utils/math.h"
#include "utils/midi.h"
#include "zrythm.h"

#include "helpers/project.h"
#include "helpers/zrythm.h"

static void
test_msb_lsb_conversions (void)
{
  /* first byte */
  midi_byte_t lsb;
  /* second byte */
  midi_byte_t msb;
  midi_get_bytes_from_combined (12280, &lsb, &msb);
  g_assert_cmpint (lsb, ==, 120);
  g_assert_cmpint (msb, ==, 95);

  midi_byte_t buf[] = { MIDI_CH1_PITCH_WHEEL_RANGE, 120, 95 };
  g_assert_cmpuint (midi_get_14_bit_value (buf), ==, 12280);
}

int
main (int argc, char * argv[])
{
  g_test_init (&argc, &argv, NULL);

  test_helper_zrythm_init ();

#define TEST_PREFIX "/audio/midi/"

  g_test_add_func (
    TEST_PREFIX "test msb lsb conversions",
    (GTestFunc) test_msb_lsb_conversions);

  return g_test_run ();
}
