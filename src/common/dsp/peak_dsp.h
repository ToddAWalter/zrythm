// SPDX-FileCopyrightText: © 2020-2021 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense
/*
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 * ---
 *
 * Copyright (C) 2013 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * ---
 */

#ifndef __AUDIO_PEAK_DSP__
#define __AUDIO_PEAK_DSP__

class PeakDsp
{
public:
  /**
   * Process.
   *
   * @param p Frame array.
   * @param n Number of samples.
   */
  ATTR_HOT void process (float * p, int n);

  float read_f ();

  void read (float * irms, float * ipeak);

  void reset ();

  /**
   * Init with the samplerate.
   */
  void init (float samplerate);

public:
  float rms;  // max rms value since last read()
  float peak; // max peak value since last read()
  int   cnt;  // digital peak hold counter
  int   fpp;  // frames per period
  float fall; // peak fallback
  bool  flag; // flag set by read(), resets _rms

  int   hold;  // peak hold timeoute
  float fsamp; // sample-rate
};

#endif
