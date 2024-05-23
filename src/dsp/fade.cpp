// SPDX-FileCopyrightText: © 2020 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "dsp/curve.h"
#include "dsp/fade.h"

/**
 * Gets the normalized Y for a normalized X.
 *
 * @param x Normalized x.
 * @param fade_in 1 for in, 0 for out.
 */
double
fade_get_y_normalized (double x, CurveOptions * opts, int fade_in)
{
  return curve_get_normalized_y (x, opts, !fade_in);
}
