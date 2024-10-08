// SPDX-FileCopyrightText: (C) 2006-2023 Fons Adriaensen <fons@linuxaudio.org>
// SPDX-License-Identifier: GPL-3.0-or-later
// ----------------------------------------------------------------------------
//
//  Copyright (C) 2006-2023 Fons Adriaensen <fons@linuxaudio.org>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 3 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// ----------------------------------------------------------------------------

#ifndef __VRESAMPLER_H
#define __VRESAMPLER_H

#include "zita-resampler/resampler-table.h"

class VResampler
{
public:
  VResampler (void);
  ~VResampler (void);

  int setup (double ratio, unsigned int nchan, unsigned int hlen);

  int setup (double ratio, unsigned int nchan, unsigned int hlen, double frel);

  void   clear (void);
  int    reset (void);
  int    nchan (void) const { return _nchan; }
  int    inpsize (void) const;
  double inpdist (void) const;
  int    process (void);

  void set_phase (double p);
  void set_rrfilt (double t);
  void set_rratio (double r);

  unsigned int inp_count;
  unsigned int out_count;
  float *      inp_data;
  float *      out_data;
  void *       inp_list;
  void *       out_list;

private:
  enum
  {
    NPHASE = 120
  };

  Resampler_table * _table;
  unsigned int      _nchan;
  unsigned int      _inmax;
  unsigned int      _index;
  unsigned int      _nread;
  unsigned int      _nzero;
  double            _ratio;
  double            _phase;
  double            _pstep;
  double            _qstep;
  double            _wstep;
  float *           _buff;
  float *           _c1;
  float *           _c2;
  void *            _dummy[8];
};

#endif
