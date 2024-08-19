// SPDX-FileCopyrightText: © 2020, 2022, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "zrythm-test-config.h"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "dsp/curve.h"

#include "tests/helpers/zrythm_helper.h"

TEST_SUITE_BEGIN ("dsp/curve");

TEST_CASE ("curve algorithms")
{
  CurveOptions opts;

  double epsilon = 0.0001;
  double val;

  /* ---- EXPONENT ---- */

  opts.algo_ = CurveOptions::Algorithm::Exponent;

  /* -- curviness -1 -- */
  opts.curviness_ = -0.95;
  val = opts.get_normalized_y (0.0, false);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);
  val = opts.get_normalized_y (0.5, 0);
  REQUIRE_FLOAT_NEAR (val, 1 - 0.93465, epsilon);
  val = opts.get_normalized_y (1.0, 0);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.0, 1);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.5, 1);
  REQUIRE_FLOAT_NEAR (val, 1 - 0.93465, epsilon);
  val = opts.get_normalized_y (1.0, 1);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);

  /* -- curviness -0.5 -- */
  opts.curviness_ = -0.5;
  val = opts.get_normalized_y (0.0, 0);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);
  val = opts.get_normalized_y (0.5, 0);
  REQUIRE_FLOAT_NEAR (val, 1 - 0.69496, epsilon);
  val = opts.get_normalized_y (1.0, 0);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.0, 1);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.5, 1);
  REQUIRE_FLOAT_NEAR (val, 1 - 0.69496, epsilon);
  val = opts.get_normalized_y (1.0, 1);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);

  /* -- curviness 0 -- */
  opts.curviness_ = 0;
  val = opts.get_normalized_y (0.0, 0);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);
  val = opts.get_normalized_y (0.5, 0);
  REQUIRE_FLOAT_NEAR (val, 0.5, epsilon);
  val = opts.get_normalized_y (1.0, 0);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.0, 1);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.5, 1);
  REQUIRE_FLOAT_NEAR (val, 0.5, epsilon);
  val = opts.get_normalized_y (1.0, 1);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);

  /* -- curviness 0.5 -- */
  opts.curviness_ = 0.5;
  val = opts.get_normalized_y (0.0, 0);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);
  val = opts.get_normalized_y (0.5, 0);
  REQUIRE_FLOAT_NEAR (val, 0.69496, epsilon);
  val = opts.get_normalized_y (1.0, 0);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.0, 1);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.5, 1);
  REQUIRE_FLOAT_NEAR (val, 0.69496, epsilon);
  val = opts.get_normalized_y (1.0, 1);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);

  /* -- curviness 1 -- */
  opts.curviness_ = 0.95;
  val = opts.get_normalized_y (0.0, 0);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);
  val = opts.get_normalized_y (0.5, 0);
  REQUIRE_FLOAT_NEAR (val, 0.93465, epsilon);
  val = opts.get_normalized_y (1.0, 0);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.0, 1);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.5, 1);
  REQUIRE_FLOAT_NEAR (val, 0.93465, epsilon);
  val = opts.get_normalized_y (1.0, 1);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);

  /* ---- SUPERELLIPSE ---- */

  opts.algo_ = CurveOptions::Algorithm::SuperEllipse;

  /* -- curviness - 0.7 -- */
  opts.curviness_ = -0.7;
  val = opts.get_normalized_y (0.0, 0);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);
  val = opts.get_normalized_y (0.5, 0);
  REQUIRE_FLOAT_NEAR (val, 1 - 0.9593, epsilon);
  val = opts.get_normalized_y (1.0, 0);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.0, 1);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.5, 1);
  REQUIRE_FLOAT_NEAR (val, 1 - 0.9593, epsilon);
  val = opts.get_normalized_y (1.0, 1);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);

  /* -- curviness 0 -- */
  opts.curviness_ = 0;
  val = opts.get_normalized_y (0.0, 0);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);
  val = opts.get_normalized_y (0.5, 0);
  REQUIRE_FLOAT_NEAR (val, 0.5, epsilon);
  val = opts.get_normalized_y (1.0, 0);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.0, 1);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.5, 1);
  REQUIRE_FLOAT_NEAR (val, 0.5, epsilon);
  val = opts.get_normalized_y (1.0, 1);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);

  /* -- curviness 0.7 -- */
  opts.curviness_ = 0.7;
  val = opts.get_normalized_y (0.0, 0);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);
  val = opts.get_normalized_y (0.5, 0);
  REQUIRE_FLOAT_NEAR (val, 0.9593, epsilon);
  val = opts.get_normalized_y (1.0, 0);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.0, 1);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.5, 1);
  REQUIRE_FLOAT_NEAR (val, 0.9593, epsilon);
  val = opts.get_normalized_y (1.0, 1);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);

  /* ---- VITAL ---- */

  opts.algo_ = CurveOptions::Algorithm::Vital;

  /* -- curviness -1 -- */
  opts.curviness_ = -1;
  val = opts.get_normalized_y (0.0, 0);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);
  val = opts.get_normalized_y (0.5, 0);
  REQUIRE_FLOAT_NEAR (val, 1 - 0.9933, epsilon);
  val = opts.get_normalized_y (1.0, 0);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.0, 1);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.5, 1);
  REQUIRE_FLOAT_NEAR (val, 1 - 0.9933, epsilon);
  val = opts.get_normalized_y (1.0, 1);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);

  /* -- curviness -0.5 -- */
  opts.curviness_ = -0.5;
  val = opts.get_normalized_y (0.0, 0);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);
  val = opts.get_normalized_y (0.5, 0);
  REQUIRE_FLOAT_NEAR (val, 1 - 0.9241, epsilon);
  val = opts.get_normalized_y (1.0, 0);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.0, 1);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.5, 1);
  REQUIRE_FLOAT_NEAR (val, 1 - 0.9241, epsilon);
  val = opts.get_normalized_y (1.0, 1);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);

  /* -- curviness 0 -- */
  opts.curviness_ = 0;
  val = opts.get_normalized_y (0.0, 0);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);
  val = opts.get_normalized_y (0.5, 0);
  REQUIRE_FLOAT_NEAR (val, 0.5, epsilon);
  val = opts.get_normalized_y (1.0, 0);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.0, 1);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.5, 1);
  REQUIRE_FLOAT_NEAR (val, 0.5, epsilon);
  val = opts.get_normalized_y (1.0, 1);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);

  /* -- curviness 0.5 -- */
  opts.curviness_ = 0.5;
  val = opts.get_normalized_y (0.0, 0);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);
  val = opts.get_normalized_y (0.5, 0);
  REQUIRE_FLOAT_NEAR (val, 0.9241, epsilon);
  val = opts.get_normalized_y (1.0, 0);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.0, 1);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.5, 1);
  REQUIRE_FLOAT_NEAR (val, 0.9241, epsilon);
  val = opts.get_normalized_y (1.0, 1);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);

  /* -- curviness 1 -- */
  opts.curviness_ = 1;
  val = opts.get_normalized_y (0.0, 0);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);
  val = opts.get_normalized_y (0.5, 0);
  REQUIRE_FLOAT_NEAR (val, 0.9933, epsilon);
  val = opts.get_normalized_y (1.0, 0);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.0, 1);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.5, 1);
  REQUIRE_FLOAT_NEAR (val, 0.9933, epsilon);
  val = opts.get_normalized_y (1.0, 1);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);

  /* ---- PULSE ---- */

  opts.algo_ = CurveOptions::Algorithm::Pulse;

  /* -- curviness -1 -- */
  opts.curviness_ = -1;
  val = opts.get_normalized_y (0.0, false);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.5, false);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (1.0, false);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.0, true);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);
  val = opts.get_normalized_y (0.5, true);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);
  val = opts.get_normalized_y (1.0, true);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);

  /* -- curviness -0.5 -- */
  opts.curviness_ = -0.5;
  val = opts.get_normalized_y (0.0, 0);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);
  val = opts.get_normalized_y (0.5, 0);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (1.0, 0);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.0, 1);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.5, 1);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);
  val = opts.get_normalized_y (1.0, 1);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);

  /* -- curviness 0 -- */
  opts.curviness_ = 0;
  val = opts.get_normalized_y (0.0, 0);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);
  val = opts.get_normalized_y (0.5, 0);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (1.0, 0);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.0, 1);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.5, 1);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);
  val = opts.get_normalized_y (1.0, 1);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);

  /* -- curviness 0.5 -- */
  opts.curviness_ = 0.5;
  val = opts.get_normalized_y (0.0, 0);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);
  val = opts.get_normalized_y (0.5, 0);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);
  val = opts.get_normalized_y (1.0, 0);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.0, 1);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.5, 1);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (1.0, 1);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);

  /* -- curviness 1 -- */
  opts.curviness_ = 1;
  val = opts.get_normalized_y (0.0, 0);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);
  val = opts.get_normalized_y (0.5, 0);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);
  val = opts.get_normalized_y (1.0, 0);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.0, 1);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.5, 1);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (1.0, 1);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);

  /* ---- LOGARITHMIC ---- */

  opts.algo_ = CurveOptions::Algorithm::Logarithmic;

  /* -- curviness -1 -- */
  opts.curviness_ = -0.95;
  val = opts.get_normalized_y (0.0, 0);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);
  val = opts.get_normalized_y (0.5, 0);
  REQUIRE_FLOAT_NEAR (val, 1 - 0.968689501, epsilon);

  val = opts.get_normalized_y (1.0, 0);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.0, 1);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.5, 1);
  REQUIRE_FLOAT_NEAR (val, 1 - 0.968689501, epsilon);
  val = opts.get_normalized_y (1.0, 1);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);

  /* -- curviness -0.5 -- */
  opts.curviness_ = -0.5;
  val = opts.get_normalized_y (0.0, 0);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);
  val = opts.get_normalized_y (0.5, 0);
  REQUIRE_FLOAT_NEAR (val, 1 - 0.893168449, epsilon);
  val = opts.get_normalized_y (1.0, 0);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.0, 1);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.5, 1);
  REQUIRE_FLOAT_NEAR (val, 1 - 0.893168449, epsilon);
  val = opts.get_normalized_y (1.0, 1);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);

  /* -- curviness 0 -- */
  opts.curviness_ = 0;
  val = opts.get_normalized_y (0.0, 0);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);
  val = opts.get_normalized_y (0.5, 0);
  REQUIRE_FLOAT_NEAR (val, 0.511909664, epsilon);
  val = opts.get_normalized_y (1.0, 0);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.0, 1);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.5, 1);
  REQUIRE_FLOAT_NEAR (val, 0.511909664, epsilon);
  val = opts.get_normalized_y (1.0, 1);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);

  /* -- curviness 0.5 -- */
  opts.curviness_ = 0.5;
  val = opts.get_normalized_y (0.0, 0);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);
  val = opts.get_normalized_y (0.5, 0);
  REQUIRE_FLOAT_NEAR (val, 0.893168449, epsilon);
  val = opts.get_normalized_y (1.0, 0);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.0, 1);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.5, 1);
  REQUIRE_FLOAT_NEAR (val, 0.893168449, epsilon);
  val = opts.get_normalized_y (1.0, 1);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);

  /* -- curviness 1 -- */
  opts.curviness_ = 0.95;
  val = opts.get_normalized_y (0.0, 0);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);
  val = opts.get_normalized_y (0.5, 0);
  REQUIRE_FLOAT_NEAR (val, 0.968689501, epsilon);
  val = opts.get_normalized_y (1.0, 0);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.0, 1);
  REQUIRE_FLOAT_NEAR (val, 1.0, epsilon);
  val = opts.get_normalized_y (0.5, 1);
  REQUIRE_FLOAT_NEAR (val, 0.968689501, epsilon);
  val = opts.get_normalized_y (1.0, 1);
  REQUIRE_FLOAT_NEAR (val, 0.0, epsilon);
}

TEST_SUITE_END;