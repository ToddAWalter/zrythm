/*
 * Copyright (C) 2019-2021 Alexandros Theodotou <alex at zrythm dot org>
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

#include "audio/engine.h"
#include "audio/snap_grid.h"
#include "audio/transport.h"
#include "project.h"
#include "settings/settings.h"
#include "utils/algorithms.h"
#include "utils/arrays.h"
#include "utils/mem.h"
#include "utils/objects.h"

#include <gtk/gtk.h>

int
snap_grid_get_ticks_from_length_and_type (
  NoteLength length,
  NoteType   type)
{
  switch (length)
    {
    case NOTE_LENGTH_2_1:
      switch (type)
        {
        case NOTE_TYPE_NORMAL:
          return 8 * TICKS_PER_QUARTER_NOTE;
          break;
        case NOTE_TYPE_DOTTED:
          return 12 * TICKS_PER_QUARTER_NOTE;
          break;
        case NOTE_TYPE_TRIPLET:
          return (16 * TICKS_PER_QUARTER_NOTE) / 3;
          break;
        }
      break;
    case NOTE_LENGTH_1_1:
      switch (type)
        {
        case NOTE_TYPE_NORMAL:
          return 4 * TICKS_PER_QUARTER_NOTE;
          break;
        case NOTE_TYPE_DOTTED:
          return 6 * TICKS_PER_QUARTER_NOTE;
          break;
        case NOTE_TYPE_TRIPLET:
          return (8 * TICKS_PER_QUARTER_NOTE) / 3;
          break;
        }
      break;
    case NOTE_LENGTH_1_2:
      switch (type)
        {
        case NOTE_TYPE_NORMAL:
          return 2 * TICKS_PER_QUARTER_NOTE;
          break;
        case NOTE_TYPE_DOTTED:
          return 3 * TICKS_PER_QUARTER_NOTE;
          break;
        case NOTE_TYPE_TRIPLET:
          return (4 * TICKS_PER_QUARTER_NOTE) / 3;
          break;
        }
      break;
    case NOTE_LENGTH_1_4:
      switch (type)
        {
        case NOTE_TYPE_NORMAL:
          return TICKS_PER_QUARTER_NOTE;
          break;
        case NOTE_TYPE_DOTTED:
          return (3 * TICKS_PER_QUARTER_NOTE) / 2;
          break;
        case NOTE_TYPE_TRIPLET:
          return (2 * TICKS_PER_QUARTER_NOTE) / 3;
          break;
        }
      break;
    case NOTE_LENGTH_1_8:
      switch (type)
        {
        case NOTE_TYPE_NORMAL:
          return TICKS_PER_QUARTER_NOTE / 2;
          break;
        case NOTE_TYPE_DOTTED:
          return (3 * TICKS_PER_QUARTER_NOTE) / 4;
          break;
        case NOTE_TYPE_TRIPLET:
          return TICKS_PER_QUARTER_NOTE / 3;
          break;
        }
      break;
    case NOTE_LENGTH_1_16:
      switch (type)
        {
        case NOTE_TYPE_NORMAL:
          return TICKS_PER_QUARTER_NOTE / 4;
          break;
        case NOTE_TYPE_DOTTED:
          return (3 * TICKS_PER_QUARTER_NOTE) / 8;
          break;
        case NOTE_TYPE_TRIPLET:
          return TICKS_PER_QUARTER_NOTE / 6;
          break;
        }
      break;
    case NOTE_LENGTH_1_32:
      switch (type)
        {
        case NOTE_TYPE_NORMAL:
          return TICKS_PER_QUARTER_NOTE / 8;
          break;
        case NOTE_TYPE_DOTTED:
          return (3 * TICKS_PER_QUARTER_NOTE) / 16;
          break;
        case NOTE_TYPE_TRIPLET:
          return TICKS_PER_QUARTER_NOTE / 12;
          break;
        }
      break;
    case NOTE_LENGTH_1_64:
      switch (type)
        {
        case NOTE_TYPE_NORMAL:
          return TICKS_PER_QUARTER_NOTE / 16;
          break;
        case NOTE_TYPE_DOTTED:
          return (3 * TICKS_PER_QUARTER_NOTE) / 32;
          break;
        case NOTE_TYPE_TRIPLET:
          return TICKS_PER_QUARTER_NOTE / 24;
          break;
        }
      break;
    case NOTE_LENGTH_1_128:
      switch (type)
        {
        case NOTE_TYPE_NORMAL:
          return TICKS_PER_QUARTER_NOTE / 32;
          break;
        case NOTE_TYPE_DOTTED:
          return (3 * TICKS_PER_QUARTER_NOTE) / 64;
          break;
        case NOTE_TYPE_TRIPLET:
          return TICKS_PER_QUARTER_NOTE / 48;
          break;
        }
      break;
    }
  g_warn_if_reached ();
  return -1;
}

/**
 * Gets a snap point's length in ticks.
 */
int
snap_grid_get_snap_ticks (
  SnapGrid * self)
{
  return
    snap_grid_get_ticks_from_length_and_type (
      self->snap_note_length, self->snap_note_type);
}

/**
 * Gets a the default length in ticks.
 */
int
snap_grid_get_default_ticks (
  SnapGrid * self)
{
  if (self->length_type == NOTE_LENGTH_LINK)
    {
      return snap_grid_get_snap_ticks (self);
    }
  else if (self->length_type ==
             NOTE_LENGTH_LAST_OBJECT)
    {
      double last_obj_length = 0.0;
      if (self->type == SNAP_GRID_TYPE_TIMELINE)
        {
          last_obj_length =
            g_settings_get_double (
              S_UI, "timeline-last-object-length");
        }
      else if (self->type == SNAP_GRID_TYPE_EDITOR)
        {
          last_obj_length =
            g_settings_get_double (
              S_UI, "editor-last-object-length");
        }
      return (int) last_obj_length;
    }
  else
    {
      return
        snap_grid_get_ticks_from_length_and_type (
          self->default_note_length,
          self->default_note_type);
    }
}

static void
add_snap_point (
  SnapGrid * self,
  Position * snap_point)
{
  array_double_size_if_full (
    self->snap_points, self->num_snap_points,
    self->snap_points_size, Position);
  position_from_ticks (
    &self->snap_points[self->num_snap_points++],
    snap_point->ticks);
  /*position_set_to_pos (*/
    /*&self->snap_points[self->num_snap_points++],*/
    /*snap_point);*/
}

/**
 * Updates snap points.
 */
void
snap_grid_update_snap_points (
  SnapGrid * self,
  int        max_bars)
{
  POSITION_INIT_ON_STACK (tmp);
  Position end_pos;
  position_set_to_bar (
    &end_pos, max_bars);
  self->num_snap_points = 0;
  add_snap_point (self, &tmp);
  long ticks = snap_grid_get_snap_ticks (self);
  g_warn_if_fail (TRANSPORT->ticks_per_bar > 0);
  while (tmp.ticks < end_pos.ticks)
    {
      tmp.ticks += ticks;
      add_snap_point (self, &tmp);
    }
#if 0
  while (position_is_before (&tmp, &end_pos))
    {
      position_add_ticks (&tmp, ticks);
      add_snap_point (self, &tmp);
    }
#endif
}

void
snap_grid_init (
  SnapGrid *   self,
  SnapGridType type,
  NoteLength   note_length)
{
  self->schema_version = SNAP_GRID_SCHEMA_VERSION;
  self->type = type;
  self->num_snap_points = 0;
  self->snap_note_length = note_length;
  self->snap_note_type = NOTE_TYPE_NORMAL;
  self->default_note_length = note_length;
  self->default_note_type = NOTE_TYPE_NORMAL;
  self->snap_to_grid = true;
  self->length_type = NOTE_LENGTH_LINK;

  self->snap_points = object_new_n (1, Position);
  self->snap_points_size = 1;
}

static const char *
get_note_type_str (
  NoteType type)
{
  return note_type_short_strings[type].str;
}

static const char *
get_note_length_str (
  NoteLength length)
{
  return note_length_strings[length].str;
}

/**
 * Returns the grid intensity as a human-readable
 * string.
 *
 * Must be free'd.
 */
char *
snap_grid_stringize (
  NoteLength note_length,
  NoteType   note_type)
{
  const char * c =
    get_note_type_str (note_type);
  const char * first_part =
    get_note_length_str (note_length);

  return  g_strdup_printf ("%s%s", first_part, c);
}

/**
 * Returns the next or previous SnapGrid Point.
 *
 * @param self Snap grid to search in.
 * @param pos Position to search for.
 * @param return_prev 1 to return the previous
 * element or 0 to return the next.
 */
Position *
snap_grid_get_nearby_snap_point (
  SnapGrid * self,
  const Position * pos,
  const int        return_prev)
{
  Position * ret_pos = NULL;
  algorithms_binary_search_nearby (
    self->snap_points, pos, return_prev, 0,
    self->num_snap_points, Position *,
    position_compare, &, ret_pos, NULL);

  return ret_pos;
}
