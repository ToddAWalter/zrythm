/*
 * Copyright (C) 2018-2021 Alexandros Theodotou <alex at zrythm dot org>
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

/**
 * \file
 *
 * Position struct and API.
 */

#ifndef __AUDIO_POSITION_H__
#define __AUDIO_POSITION_H__

#include <stdbool.h>

#include "utils/yaml.h"

/**
 * @addtogroup audio
 *
 * @{
 */

#define POSITION_SCHEMA_VERSION 1

#define TICKS_PER_QUARTER_NOTE 960
#define TICKS_PER_SIXTEENTH_NOTE 240
#define TICKS_PER_QUARTER_NOTE_DBL 960.0
#define TICKS_PER_SIXTEENTH_NOTE_DBL 240.0
#define position_add_sixteenths(_pos, _s) \
  position_add_ticks ( \
    (_pos), (_s) * TICKS_PER_SIXTEENTH_NOTE)
#define position_add_beats(_pos, _b) \
  g_warn_if_fail (TRANSPORT->ticks_per_beat > 0); \
  position_add_ticks ( \
    (_pos), (_b) * TRANSPORT->ticks_per_beat)
#define position_add_bars(_pos, _b) \
  g_warn_if_fail (TRANSPORT->ticks_per_bar > 0); \
  position_add_ticks ( \
    (_pos), (_b) * TRANSPORT->ticks_per_bar)
#define position_snap_simple(pos, sg) \
  position_snap (NULL, pos, NULL, NULL, sg)

/**
 * Whether the position starts on or after f1 and
 * before f2.
 */
#define position_between_frames_excl2(pos,f1,f2) \
  ((pos)->frames >= f1 && (pos)->frames < f2)

/**
 * Compares 2 positions based on their frames.
 *
 * negative = p1 is earlier
 * 0 = equal
 * positive = p2 is earlier
 */
#define position_compare(p1,p2) \
  ((p1)->frames - (p2)->frames)

/** Checks if _pos is before _cmp. */
#define position_is_before(_pos,_cmp) \
  (position_compare (_pos, _cmp) < 0)

/** Checks if _pos is before or equal to _cmp. */
#define position_is_before_or_equal(_pos,_cmp) \
  (position_compare (_pos, _cmp) <= 0)

/** Checks if _pos is equal to _cmp. */
#define position_is_equal(_pos,_cmp) \
  (position_compare (_pos, _cmp) == 0)

/** Checks if _pos is after _cmp. */
#define position_is_after(_pos,_cmp) \
  (position_compare (_pos, _cmp) > 0)

/** Checks if _pos is after or equal to _cmp. */
#define position_is_after_or_equal(_pos,_cmp) \
  (position_compare (_pos, _cmp) >= 0)

/**
 * Compares 2 positions based on their total ticks.
 *
 * negative = p1 is earlier
 * 0 = equal
 * positive = p2 is earlier
 */
#define position_compare_ticks(p1,p2) \
  ((p1)->ticks - (p2)->ticks)

#define position_is_equal_ticks(p1,p2) \
  (fabs (position_compare_ticks (p1, p2)) <= \
     DBL_EPSILON)

/** Returns if _pos is after or equal to _start and
 * before _end. */
#define position_is_between(_pos,_start,_end) \
  (position_is_after_or_equal (_pos, _start) && \
   position_is_before (_pos, _end))

/** Returns if _pos is after _start and
 * before _end. */
#define position_is_between_excl_start(_pos,_start,_end) \
  (position_is_after (_pos, _start) && \
   position_is_before (_pos, _end))

/** Inits the default position on the stack. */
#define POSITION_INIT_ON_STACK(name) \
  Position name = POSITION_START;

/**
 * Initializes given position.
 *
 * Assumes the given argument is a Pointer *.
 */
#define position_init(__pos) \
  *(__pos) = POSITION_START

typedef struct SnapGrid SnapGrid;
typedef struct Track Track;
typedef struct ZRegion ZRegion;

/**
 * A Position is made up of
 * bars.beats.sixteenths.ticks.
 */
typedef struct Position
{
  int       schema_version;

  /** Precise total number of ticks. */
  double    ticks;

  /** Position in frames (samples). */
  long      frames;
} Position;

static const cyaml_schema_field_t
  position_fields_schema[] =
{
  YAML_FIELD_INT (Position, schema_version),
  YAML_FIELD_FLOAT (Position, ticks),
  YAML_FIELD_INT (Position, frames),

  CYAML_FIELD_END
};

static const cyaml_schema_value_t
  position_schema =
{
  YAML_VALUE_PTR (
    Position, position_fields_schema),
};

/** Start Position to be used in calculations. */
static const Position POSITION_START =
{
  .schema_version = POSITION_SCHEMA_VERSION,
  .ticks = 0.0,
  .frames = 0
};

/**
 * Sets position to given bar.
 */
void
position_set_to_bar (
  Position * self,
  int        bar);

/**
 * Sorts an array of Position's.
 */
void
position_sort_array (
  Position *   array,
  const size_t size);

/**
 * Sets position to target position
 *
 * Assumes each position is Position *.
 */
#define position_set_to_pos(_pos,_target) \
  *(_pos) = *(_target)

/**
 * Adds the frames to the position and updates
 * the rest of the fields, and makes sure the
 * frames are still accurate.
 */
HOT
__attribute__ ((access (read_write, 1)))
void
position_add_frames (
  Position * pos,
  const long frames);

/** Deprecated - added for compatibility. */
#define position_to_frames(x) ((x)->frames)
#define position_to_ticks(x) ((x)->ticks)

/**
 * Converts seconds to position and puts the result
 * in the given Position.
 */
void
position_from_seconds (
  Position * position,
  double     secs);

HOT
NONNULL
void
position_from_frames (
  Position * pos,
  long       frames);

/**
 * Sets position to the given total tick count.
 */
HOT
NONNULL
void
position_from_ticks (
  Position * pos,
  double     ticks);

HOT
NONNULL
void
position_add_ticks (
  Position * self,
  double     ticks);

/**
 * Returns the Position in milliseconds.
 */
long
position_to_ms (
  const Position * pos);

long
position_ms_to_frames (
  const long ms);

void
position_add_ms (
  Position * pos,
  long       ms);

void
position_add_minutes (
  Position * pos,
  int        mins);

void
position_add_seconds (
  Position * pos,
  long       seconds);

/**
 * Snaps position using given options.
 *
 * @param start_pos The previous position (ie, the
 *   position the drag started at. This is only used
 *   when the "keep offset" setting is on.
 * @param pos Position to edit.
 * @param track Track, used when moving things in
 *   the timeline. If keep offset is on and this is
 *   passed, the objects in the track will be taken
 *   into account. If keep offset is on and this is
 *   NULL, all applicable objects will be taken into
 *   account. Not used if keep offset is off.
 * @param region Region, used when moving
 *   things in the editor. Same behavior as @ref
 *   track.
 * @param sg SnapGrid options.
 */
void
position_snap (
  Position *       start_pos,
  Position *       pos,
  Track    *       track,
  ZRegion   *      region,
  const SnapGrid * sg);

/**
 * Sets the end position to be 1 snap point away
 * from the start pos.
 *
 * FIXME rename to something more meaningful.
 *
 * @param start_pos Start Position.
 * @param end_pos End Position.
 * @param snap SnapGrid.
 */
void
position_set_min_size (
  const Position * start_pos,
  Position * end_pos,
  SnapGrid * snap);

/**
 * Updates ticks.
 */
HOT
NONNULL
void
position_update_ticks_from_frames (
  Position * position);

/**
 * Updates frames.
 */
HOT
NONNULL
void
position_update_frames_from_ticks (
  Position * position);

/**
 * Calculates the midway point between the two
 * Positions and sets it on pos.
 *
 * @param pos Position to set to.
 */
void
position_get_midway_pos (
  Position * start_pos,
  Position * end_pos,
  Position * pos);

/**
 * Returns the difference in ticks between the two
 * Position's, snapped based on the given SnapGrid
 * (if any).
 *
 * @param end_pos End position.
 * @param start_pos Start Position.
 * @param sg SnapGrid to snap with, or NULL to not
 *   snap.
 */
double
position_get_ticks_diff (
  const Position * end_pos,
  const Position * start_pos,
  const SnapGrid * sg);

/**
 * Creates a string in the form of "0.0.0.0" from
 * the given position.
 *
 * Must be free'd by caller.
 */
NONNULL
char *
position_to_string_alloc (
  const Position * pos);

/**
 * Creates a string in the form of "0.0.0.0" from
 * the given position.
 */
NONNULL
void
position_to_string (
  const Position * pos,
  char *           buf);

/**
 * Prints the Position in the "0.0.0.0" form.
 */
NONNULL
void
position_print (
  const Position * pos);

NONNULL
void
position_print_range (
  const Position * pos,
  const Position * pos2);

/**
 * Returns the total number of beats.
 *
 * @param include_current Whether to count the
 *   current beat if it is at the beat start.
 */
NONNULL
int
position_get_total_bars (
  const Position * pos,
  bool  include_current);

/**
 * Returns the total number of beats.
 *
 * @param include_current Whether to count the
 *   current beat if it is at the beat start.
 */
NONNULL
int
position_get_total_beats (
  const Position * pos,
  bool  include_current);

/**
 * Returns the total number of sixteenths not
 * including the current one.
 */
NONNULL
int
position_get_total_sixteenths (
  const Position * pos,
  bool             include_current);

/**
 * Changes the sign of the position.
 *
 * For example, 4.2.1.21 would become -4.2.1.21.
 */
NONNULL
void
position_change_sign (
  Position * pos);

/**
 * Gets the bars of the position.
 *
 * Ie, if the position is equivalent to 4.1.2.42,
 * this will return 4.
 *
 * @param start_at_one Start at 1 or -1 instead of
 *   0.
 */
NONNULL
int
position_get_bars (
  const Position * pos,
  bool  start_at_one);

/**
 * Gets the beats of the position.
 *
 * Ie, if the position is equivalent to 4.1.2.42,
 * this will return 1.
 *
 * @param start_at_one Start at 1 or -1 instead of
 *   0.
 */
NONNULL
int
position_get_beats (
  const Position * pos,
  bool  start_at_one);

/**
 * Gets the sixteenths of the position.
 *
 * Ie, if the position is equivalent to 4.1.2.42,
 * this will return 2.
 *
 * @param start_at_one Start at 1 or -1 instead of
 *   0.
 */
NONNULL
int
position_get_sixteenths (
  const Position * pos,
  bool  start_at_one);

/**
 * Gets the ticks of the position.
 *
 * Ie, if the position is equivalent to 4.1.2.42,
 * this will return 42.
 */
NONNULL
double
position_get_ticks (
  const Position * pos);

NONNULL
bool
position_validate (
  const Position * pos);

/**
 * @}
 */

#endif
