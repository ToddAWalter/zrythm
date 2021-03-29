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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "audio/audio_region.h"
#include "audio/automation_point.h"
#include "audio/automation_region.h"
#include "audio/chord_region.h"
#include "audio/chord_track.h"
#include "audio/marker_track.h"
#include "audio/midi_region.h"
#include "audio/stretcher.h"
#include "gui/backend/arranger_object.h"
#include "gui/backend/automation_selections.h"
#include "gui/backend/chord_selections.h"
#include "gui/backend/event.h"
#include "gui/backend/event_manager.h"
#include "gui/backend/timeline_selections.h"
#include "gui/widgets/automation_arranger.h"
#include "gui/widgets/automation_editor_space.h"
#include "gui/widgets/automation_point.h"
#include "gui/widgets/automation_region.h"
#include "gui/widgets/bot_dock_edge.h"
#include "gui/widgets/center_dock.h"
#include "gui/widgets/chord_arranger.h"
#include "gui/widgets/chord_editor_space.h"
#include "gui/widgets/chord_object.h"
#include "gui/widgets/chord_region.h"
#include "gui/widgets/clip_editor.h"
#include "gui/widgets/clip_editor_inner.h"
#include "gui/widgets/main_notebook.h"
#include "gui/widgets/marker.h"
#include "gui/widgets/midi_arranger.h"
#include "gui/widgets/midi_editor_space.h"
#include "gui/widgets/midi_modifier_arranger.h"
#include "gui/widgets/midi_note.h"
#include "gui/widgets/midi_region.h"
#include "gui/widgets/region.h"
#include "gui/widgets/scale_object.h"
#include "gui/widgets/timeline_arranger.h"
#include "gui/widgets/timeline_panel.h"
#include "gui/widgets/track.h"
#include "gui/widgets/velocity.h"
#include "project.h"
#include "utils/cairo.h"
#include "utils/flags.h"
#include "utils/math.h"
#include "utils/objects.h"
#include "zrythm_app.h"

#include <glib/gi18n.h>

#define TYPE(x) \
  ARRANGER_OBJECT_TYPE_##x

#define TYPE_IS(x) \
  (self->type == TYPE (x))

#define POSITION_TYPE(x) \
  ARRANGER_OBJECT_POSITION_TYPE_##x

#define FOREACH_TYPE(func) \
  func (REGION, ZRegion, region) \
  func (SCALE_OBJECT, ScaleObject, scale_object) \
  func (MARKER, Marker, marker) \
  func (MIDI_NOTE, MidiNote, midi_note) \
  func (VELOCITY, Velocity, velocity) \
  func (CHORD_OBJECT, ChordObject, chord_object) \
  func (AUTOMATION_POINT, AutomationPoint, \
        automation_point)

void
arranger_object_init (
  ArrangerObject * self)
{
  self->schema_version =
    ARRANGER_OBJECT_SCHEMA_VERSION;
  self->magic = ARRANGER_OBJECT_MAGIC;

  position_init (&self->pos);
  position_init (&self->end_pos);
  position_init (&self->clip_start_pos);
  position_init (&self->loop_start_pos);
  position_init (&self->loop_end_pos);
  position_init (&self->fade_in_pos);
  position_init (&self->fade_out_pos);

  curve_opts_init (&self->fade_in_opts);
  curve_opts_init (&self->fade_out_opts);

  self->region_id.schema_version =
    REGION_IDENTIFIER_SCHEMA_VERSION;
}

/**
 * Returns the ArrangerSelections corresponding
 * to the given object type.
 */
ArrangerSelections *
arranger_object_get_selections_for_type (
  ArrangerObjectType type)
{
  switch (type)
    {
    case TYPE (REGION):
    case TYPE (SCALE_OBJECT):
    case TYPE (MARKER):
      return (ArrangerSelections *) TL_SELECTIONS;
    case TYPE (MIDI_NOTE):
    case TYPE (VELOCITY):
      return (ArrangerSelections *) MA_SELECTIONS;
    case TYPE (CHORD_OBJECT):
      return (ArrangerSelections *) CHORD_SELECTIONS;
    case TYPE (AUTOMATION_POINT):
      return
        (ArrangerSelections *) AUTOMATION_SELECTIONS;
    default:
      return NULL;
    }
}

/**
 * Selects the object by adding it to its
 * corresponding selections or making it the
 * only selection.
 *
 * @param select 1 to select, 0 to deselect.
 * @param append 1 to append, 0 to make it the only
 *   selection.
 */
void
arranger_object_select (
  ArrangerObject * self,
  const bool       select,
  const bool       append,
  bool             fire_events)
{
  g_return_if_fail (IS_ARRANGER_OBJECT (self));

  if (self->type == ARRANGER_OBJECT_TYPE_VELOCITY)
    {
      self =
        (ArrangerObject *)
        velocity_get_midi_note ((Velocity *) self);
      g_return_if_fail (IS_MIDI_NOTE (self));
    }

  ArrangerSelections * selections =
    arranger_object_get_selections_for_type (
      self->type);

  /* if nothing to do, return */
  bool is_selected =
    arranger_object_is_selected (self);
  if ((is_selected && select && append) ||
      (!is_selected && !select))
    {
      return;
    }

  if (select)
    {
      if (!append)
        {
          arranger_selections_clear (
            selections, F_NO_FREE, fire_events);
        }
      arranger_selections_add_object (
        selections, self);
    }
  else
    {
      arranger_selections_remove_object (
        selections, self);
    }

  if (fire_events)
    {
      EVENTS_PUSH (
        ET_ARRANGER_OBJECT_CHANGED, self);
    }
}

/**
 * Returns the number of loops in the ArrangerObject,
 * optionally including incomplete ones.
 */
int
arranger_object_get_num_loops (
  ArrangerObject * self,
  const int        count_incomplete)
{
  int i = 0;
  long loop_size =
    arranger_object_get_loop_length_in_frames (
      self);
  g_return_val_if_fail (loop_size > 0, 0);
  long full_size =
    arranger_object_get_length_in_frames (self);
  long loop_start =
    self->loop_start_pos.frames -
    self->clip_start_pos.frames;
  long curr_frames = loop_start;

  while (curr_frames < full_size)
    {
      i++;
      curr_frames += loop_size;
    }

  if (!count_incomplete)
    i--;

  return i;
}

static void
set_to_region_object (
  ZRegion * src,
  ZRegion * dest)
{
  g_return_if_fail (src && dest);
}

static void
set_to_midi_note_object (
  MidiNote * src,
  MidiNote * dest)
{
  g_return_if_fail (
    dest && dest->vel && src && src->vel);
  dest->vel->vel = src->vel->vel;
  dest->val = src->val;
}

/**
 * Sets the mute status of the object.
 */
void
arranger_object_set_muted (
  ArrangerObject * self,
  bool             muted,
  bool             fire_events)
{
  self->muted = muted;

  if (fire_events)
    {
      EVENTS_PUSH (
        ET_ARRANGER_OBJECT_CHANGED, self);
    }
}

/**
 * Gets the mute status of the object.
 */
bool
arranger_object_get_muted (
  ArrangerObject * self)
{
  return self->muted;
}

/**
 * Sets the dest object's values to the main
 * src object's values.
 */
void
arranger_object_set_to_object (
  ArrangerObject * dest,
  ArrangerObject * src)
{
  g_return_if_fail (src && dest);

  /* reset positions */
  dest->pos = src->pos;
  if (arranger_object_type_has_length (src->type))
    {
      dest->end_pos = src->end_pos;
    }
  if (arranger_object_type_can_loop (src->type))
    {
      dest->clip_start_pos = src->clip_start_pos;
      dest->loop_start_pos = src->loop_start_pos;
      dest->loop_end_pos = src->loop_end_pos;
    }
  if (arranger_object_can_fade (src))
    {
      dest->fade_in_pos = src->fade_in_pos;
      dest->fade_out_pos = src->fade_out_pos;
    }

  /* reset other members */
  switch (src->type)
    {
    case TYPE (REGION):
      set_to_region_object (
        (ZRegion *) src, (ZRegion *) dest);
      break;
    case TYPE (MIDI_NOTE):
      set_to_midi_note_object (
        (MidiNote *) src, (MidiNote *) dest);
      break;
    case TYPE (CHORD_OBJECT):
      {
        ChordObject * dest_co =
          (ChordObject *) dest;
        ChordObject * src_co =
          (ChordObject *) src;
        dest_co->index = src_co->index;
      }
      break;
    case TYPE (AUTOMATION_POINT):
      {
        AutomationPoint * dest_ap =
          (AutomationPoint *) dest;
        AutomationPoint * src_ap =
          (AutomationPoint *) src;
        dest_ap->fvalue = src_ap->fvalue;
      }
      break;
    default:
      break;
    }
}

/**
 * Returns if the object is in the selections.
 */
bool
arranger_object_is_selected (
  ArrangerObject * self)
{
  ArrangerSelections * selections =
    arranger_object_get_selections_for_type (
      self->type);

  return
    arranger_selections_contains_object (
      selections, self);
}

/**
 * If the object is part of a ZRegion, returns it,
 * otherwise returns NULL.
 */
ZRegion *
arranger_object_get_region (
  ArrangerObject * self)
{
  RegionIdentifier * id = NULL;
  switch (self->type)
    {
    case ARRANGER_OBJECT_TYPE_AUTOMATION_POINT:
    case ARRANGER_OBJECT_TYPE_MIDI_NOTE:
    case ARRANGER_OBJECT_TYPE_CHORD_OBJECT:
      id = &self->region_id;
      break;
    case ARRANGER_OBJECT_TYPE_VELOCITY:
      {
        Velocity * vel =
          (Velocity *) self;
        ArrangerObject * mn_obj =
          (ArrangerObject *)
          vel->midi_note;
        id = &mn_obj->region_id;
      }
      break;
    default:
      return NULL;
      break;
    }

  ZRegion * region = region_find (id);

  return region;
}

/**
 * Returns whether the given object is hit by the
 * given position or range.
 *
 * @param start Start position.
 * @param end End position, or NULL to only check
 *   for intersection with \ref start.
 */
bool
arranger_object_is_hit (
  ArrangerObject * self,
  Position *       start,
  Position *       end)
{
  if (end)
    {
      /* check range */
      if (position_is_after (
            &self->pos, end) ||
          position_is_after (
            start, &self->end_pos))
        {
          return false;
        }
      else
        {
          return true;
        }
    }
  else
    {
      /* check position hit */
      return
        position_is_after_or_equal (
          start, &self->pos) &&
        position_is_before_or_equal (
          start, &self->end_pos);
    }
}

/**
 * Gets a pointer to the Position in the
 * ArrangerObject matching the given arguments.
 */
static Position *
get_position_ptr (
  ArrangerObject *           self,
  ArrangerObjectPositionType pos_type)
{
  switch (pos_type)
    {
    case ARRANGER_OBJECT_POSITION_TYPE_START:
      return &self->pos;
    case ARRANGER_OBJECT_POSITION_TYPE_END:
      return &self->end_pos;
    case ARRANGER_OBJECT_POSITION_TYPE_CLIP_START:
      return &self->clip_start_pos;
    case ARRANGER_OBJECT_POSITION_TYPE_LOOP_START:
      return &self->loop_start_pos;
    case ARRANGER_OBJECT_POSITION_TYPE_LOOP_END:
      return &self->loop_end_pos;
    case ARRANGER_OBJECT_POSITION_TYPE_FADE_IN:
      return &self->fade_in_pos;
    case ARRANGER_OBJECT_POSITION_TYPE_FADE_OUT:
      return &self->fade_out_pos;
    }
  g_return_val_if_reached (NULL);
}

/**
 * Returns if the given Position is valid.
 *
 * @param pos The position to set to.
 * @param pos_type The type of Position to set in the
 *   ArrangerObject.
 * @param validate Validate the Position before
 *   setting it.
 */
int
arranger_object_is_position_valid (
  ArrangerObject *           self,
  const Position *           pos,
  ArrangerObjectPositionType pos_type)
{
  int is_valid = 0;
  switch (pos_type)
    {
    case ARRANGER_OBJECT_POSITION_TYPE_START:
      if (arranger_object_type_has_length (
            self->type))
        {
          Position * end_pos = &self->end_pos;
          is_valid =
            position_is_before (pos, end_pos);

          if (!arranger_object_owned_by_region (
                self))
            {
              is_valid =
                is_valid &&
                position_is_after_or_equal (
                  pos, &POSITION_START);
            }
        }
      else if (arranger_object_type_has_global_pos (
                 self->type))
        {
          is_valid =
            position_is_after_or_equal (
              pos, &POSITION_START);
        }
      else
        {
          is_valid = 1;
        }
      break;
    case ARRANGER_OBJECT_POSITION_TYPE_LOOP_START:
      {
        Position * loop_end_pos =
           &self->loop_end_pos;
        is_valid =
          position_is_before (
            pos, loop_end_pos) &&
          position_is_after_or_equal (
            pos, &POSITION_START);
      }
      break;
    case ARRANGER_OBJECT_POSITION_TYPE_LOOP_END:
      {
        /* TODO */
        is_valid = 1;
      }
      break;
    case ARRANGER_OBJECT_POSITION_TYPE_CLIP_START:
      {
        Position * loop_end_pos =
           &self->loop_end_pos;
        is_valid =
          position_is_before (
            pos, loop_end_pos) &&
          position_is_after_or_equal (
            pos, &POSITION_START);
      }
      break;
    case ARRANGER_OBJECT_POSITION_TYPE_END:
      {
        /* TODO */
        is_valid = 1;
      }
      break;
    default:
      break;
    }

#if 0
  g_debug (
    "%s position with ticks %f",
    is_valid ? "Valid" : "Invalid",
    pos->ticks);
#endif

  return is_valid;
}

/**
 * Copies the identifier from src to dest.
 */
void
arranger_object_copy_identifier (
  ArrangerObject * dest,
  ArrangerObject * src)
{
  g_return_if_fail (
    IS_ARRANGER_OBJECT (dest) &&
    IS_ARRANGER_OBJECT (src) &&
    dest->type == src->type);

  if (arranger_object_owned_by_region (dest))
    {
      region_identifier_copy (
        &dest->region_id, &src->region_id);
    }

  switch (dest->type)
    {
    case TYPE (REGION):
      {
        ZRegion * dest_r = (ZRegion *) dest;
        ZRegion * src_r = (ZRegion *) src;
        region_identifier_copy (
          &dest_r->id, &src_r->id);
      }
      break;
    case TYPE (MIDI_NOTE):
      {
        MidiNote * destmn = (MidiNote *) dest;
        MidiNote * srcmn = (MidiNote *) src;
        destmn->pos = srcmn->pos;
      }
      break;
    case TYPE (AUTOMATION_POINT):
      {
        AutomationPoint * destap =
          (AutomationPoint *) dest;
        AutomationPoint * srcap =
          (AutomationPoint *) src;
        destap->index = srcap->index;
      }
      break;
    case TYPE (CHORD_OBJECT):
      {
        ChordObject * destap =
          (ChordObject *) dest;
        ChordObject * srcap =
          (ChordObject *) src;
        destap->index = srcap->index;
      }
      break;
    case TYPE (MARKER):
      {
        Marker * dest_marker =
          (Marker *) dest;
        Marker * src_marker =
          (Marker *) src;
        dest_marker->track_pos =
          src_marker->track_pos;
        dest_marker->index =
          src_marker->index;
        if (dest_marker->name)
          g_free (dest_marker->name);
        dest_marker->name =
          g_strdup (src_marker->name);
      }
    default:
      break;
    }
}

/**
 * Sets the Position  all of the object's linked
 * objects (see ArrangerObjectInfo)
 *
 * @param pos The position to set to.
 * @param pos_type The type of Position to set in the
 *   ArrangerObject.
 * @param validate Validate the Position before
 *   setting it.
 */
void
arranger_object_set_position (
  ArrangerObject *           self,
  const Position *           pos,
  ArrangerObjectPositionType pos_type,
  const int                  validate)
{
  g_return_if_fail (self && pos);

  /* return if validate is on and position is
   * invalid */
  if (validate &&
      !arranger_object_is_position_valid (
        self, pos, pos_type))
    return;

  Position * pos_ptr;
  pos_ptr = get_position_ptr (self, pos_type);
  g_return_if_fail (pos_ptr);
  position_set_to_pos (pos_ptr, pos);
}

/**
 * Returns the type as a string.
 */
const char *
arranger_object_stringize_type (
  ArrangerObjectType type)
{
  return arranger_object_type_strings[type].str;
}

/**
 * Sets the magic on the arranger object.
 */
void
arranger_object_set_magic (
  ArrangerObject * self)
{
  self->magic = ARRANGER_OBJECT_MAGIC;

  switch (self->type)
    {
    case TYPE (REGION):
      ((ZRegion *) self)->magic = REGION_MAGIC;
      break;
    case TYPE (MIDI_NOTE):
      ((MidiNote *) self)->magic = MIDI_NOTE_MAGIC;
      break;
    default:
      /* nothing needed */
      break;
    }
}

/**
 * Prints debug information about the given
 * object.
 */
void
arranger_object_print (
  ArrangerObject * self)
{
  g_return_if_fail (self);

  const char * type =
    arranger_object_stringize_type (self->type);

  char positions[900];
  char start_pos_str[100];
  position_to_string (
    &self->pos, start_pos_str);
  if (arranger_object_type_has_length (self->type))
    {
      char end_pos_str[100];
      position_to_string (
        &self->end_pos, end_pos_str);

      if (arranger_object_type_can_loop (
            self->type))
        {
          char clip_start_pos_str[100];
          position_to_string (
            &self->clip_start_pos,
            clip_start_pos_str);
          char loop_start_pos_str[100];
          position_to_string (
            &self->loop_start_pos,
            loop_start_pos_str);
          char loop_end_pos_str[100];
          position_to_string (
            &self->loop_end_pos,
            loop_end_pos_str);
          sprintf (
            positions,
            "(%s ~ %s | cs: %s ls: %s le: %s)",
            start_pos_str, end_pos_str,
            clip_start_pos_str,
            loop_start_pos_str, loop_end_pos_str);
        }
      else
        {
          sprintf (
            positions, "(%s ~ %s)",
            start_pos_str, end_pos_str);
        }
    }
  else
    {
      sprintf (positions, "(%s)", start_pos_str);
    }

  char * extra_info = NULL;
  if (self->type == ARRANGER_OBJECT_TYPE_REGION)
    {
      ZRegion * region = (ZRegion *) self;
      extra_info =
        g_strdup_printf (
          " track: %d - idx: %d",
          region->id.track_pos, region->id.idx);
    }

  g_message (
    "%s %s%s",
    type, positions, extra_info ? extra_info : "");

  if (extra_info)
    g_free (extra_info);
}

/**
 * Moves the object by the given amount of
 * ticks.
 */
void
arranger_object_move (
  ArrangerObject *         self,
  const double             ticks)
{
  if (arranger_object_type_has_length (self->type))
    {
      long length_frames =
        arranger_object_get_length_in_frames (
          self);

      /* start pos */
      Position tmp;
      position_set_to_pos (
        &tmp, &self->pos);
      position_add_ticks (
        &tmp, ticks);
      arranger_object_set_position (
        self, &tmp,
        ARRANGER_OBJECT_POSITION_TYPE_START,
        F_NO_VALIDATE);

      /* end pos */
      if (self->type == TYPE (REGION))
        {
          /* audio regions need the exact
           * number of frames to match the clip.
           *
           * this should be used for all
           * objects but currently moving objects
           * before 1.1.1.1 causes bugs hence this
           * (temporary) if statement */
          position_add_frames (
            &tmp, length_frames);
        }
      else
        {
          position_set_to_pos (
            &tmp, &self->end_pos);
          position_add_ticks (
            &tmp, ticks);
        }
      arranger_object_set_position (
        self, &tmp,
        ARRANGER_OBJECT_POSITION_TYPE_END,
        F_NO_VALIDATE);
    }
  else
    {
      Position tmp;
      position_set_to_pos (
        &tmp, &self->pos);
      position_add_ticks (
        &tmp, ticks);
      arranger_object_set_position (
        self, &tmp,
        ARRANGER_OBJECT_POSITION_TYPE_START,
        F_NO_VALIDATE);
    }
}

/**
 * Getter.
 */
void
arranger_object_get_pos (
  const ArrangerObject * self,
  Position *             pos)
{
  position_set_to_pos (
    pos, &self->pos);
}

/**
 * Getter.
 */
void
arranger_object_get_end_pos (
  const ArrangerObject * self,
  Position *             pos)

{
  position_set_to_pos (
    pos, &self->end_pos);
}

/**
 * Getter.
 */
void
arranger_object_get_clip_start_pos (
  const ArrangerObject * self,
  Position *             pos)

{
  position_set_to_pos (
    pos, &self->clip_start_pos);
}

/**
 * Getter.
 */
void
arranger_object_get_loop_start_pos (
  const ArrangerObject * self,
  Position *             pos)

{
  position_set_to_pos (
    pos, &self->loop_start_pos);
}

/**
 * Getter.
 */
void
arranger_object_get_loop_end_pos (
  const ArrangerObject * self,
  Position *             pos)

{
  position_set_to_pos (
    pos, &self->loop_end_pos);
}

static void
init_loaded_midi_note (
  MidiNote * self)
{
  self->magic = MIDI_NOTE_MAGIC;
  self->vel->midi_note = self;
}

static void
init_loaded_scale_object (
  ScaleObject * self)
{
  self->magic = SCALE_OBJECT_MAGIC;
}

static void
init_loaded_chord_object (
  ChordObject * self)
{
  self->magic = CHORD_OBJECT_MAGIC;
}

static void
init_loaded_region (
  ZRegion * self)
{
  self->magic = REGION_MAGIC;

  int i;
  switch (self->id.type)
    {
    case REGION_TYPE_AUDIO:
      {
        AudioClip * clip =
          audio_region_get_clip (self);
        g_return_if_fail (clip);
        self->last_clip_change =
          g_get_monotonic_time ();
      }
      break;
    case REGION_TYPE_MIDI:
      {
        MidiNote * mn;
        for (i = 0; i < self->num_midi_notes; i++)
          {
            mn = self->midi_notes[i];
            arranger_object_init_loaded (
              (ArrangerObject *) mn);
          }
        self->midi_notes_size =
          (size_t) self->num_midi_notes;
      }
      break;
    case REGION_TYPE_CHORD:
      {
        ChordObject * chord;
        for (i = 0; i < self->num_chord_objects;
             i++)
          {
            chord = self->chord_objects[i];
            arranger_object_init_loaded (
              (ArrangerObject *) chord);
          }
        self->chord_objects_size =
          (size_t) self->num_chord_objects;
        }
      break;
    case REGION_TYPE_AUTOMATION:
      {
        AutomationPoint * ap;
        for (i = 0; i < self->num_aps; i++)
          {
            ap = self->aps[i];
            arranger_object_init_loaded (
              (ArrangerObject *) ap);
          }
        self->aps_size =
          (size_t) self->num_aps;
      }
      break;
    }

  region_validate (self, false);
}

/**
 * Initializes the object after loading a Project.
 */
void
arranger_object_init_loaded (
  ArrangerObject * self)
{
  g_return_if_fail (self->type > TYPE (NONE));

  /* init positions */
  self->magic = ARRANGER_OBJECT_MAGIC;

  switch (self->type)
    {
    case TYPE (REGION):
      init_loaded_region (
        (ZRegion *) self);
      break;
    case TYPE (MIDI_NOTE):
      init_loaded_midi_note (
        (MidiNote *) self);
      arranger_object_init_loaded (
        (ArrangerObject *)
        ((MidiNote *) self)->vel);
      break;
    case TYPE (SCALE_OBJECT):
      init_loaded_scale_object (
        (ScaleObject *) self);
      break;
    case TYPE (CHORD_OBJECT):
      init_loaded_chord_object (
        (ChordObject *) self);
      break;
    default:
      /* nothing needed */
      break;
    }
}

/**
 * Returns the length of the ArrangerObject (if
 * it has length) in ticks.
 *
 * (End Position - start Position).
 */
double
arranger_object_get_length_in_ticks (
  ArrangerObject * self)
{
  g_return_val_if_fail (
    arranger_object_type_has_length (self->type),
    0);

  return
    self->end_pos.ticks -
    self->pos.ticks;
}

/**
 * Returns the length of the ArrangerObject (if
 * it has length) in frames.
 *
 * (End Position - start Position).
 */
long
arranger_object_get_length_in_frames (
  ArrangerObject * self)
{
  g_return_val_if_fail (
    arranger_object_type_has_length (self->type),
    0);

  return
    self->end_pos.frames -
    self->pos.frames;
}

/**
 * Returns the length of the loop in ticks.
 */
double
arranger_object_get_loop_length_in_ticks (
  ArrangerObject * self)
{
  g_return_val_if_fail (
    arranger_object_type_has_length (self->type),
    0);

  return
    self->loop_end_pos.ticks -
    self->loop_start_pos.ticks;
}

/**
 * Returns the length of the loop in ticks.
 */
long
arranger_object_get_loop_length_in_frames (
  ArrangerObject * self)
{
  g_return_val_if_fail (
    arranger_object_type_has_length (self->type),
    0);

  return
    self->loop_end_pos.frames -
    self->loop_start_pos.frames;
}

/**
 * Updates the frames of each position in each
 * child recursively.
 */
void
arranger_object_update_frames (
  ArrangerObject * self)
{
  long frames_len_before = 0;
  if (arranger_object_type_has_length (self->type))
    {
      frames_len_before =
        arranger_object_get_length_in_frames (self);
    }

  position_update_frames_from_ticks (&self->pos);
  if (arranger_object_type_has_length (self->type))
    {
      position_update_frames_from_ticks (
        &self->end_pos);
    }
  if (arranger_object_type_can_loop (self->type))
    {
      position_update_frames_from_ticks (
        &self->clip_start_pos);
      position_update_frames_from_ticks (
        &self->loop_start_pos);
      position_update_frames_from_ticks (
        &self->loop_end_pos);
    }
  if (arranger_object_can_fade (self))
    {
      position_update_frames_from_ticks (
        &self->fade_in_pos);
      position_update_frames_from_ticks (
        &self->fade_out_pos);
    }

  ZRegion * r;
  switch (self->type)
    {
    case TYPE (REGION):
      r = (ZRegion *) self;

      /* validate */
      if (r->id.type == REGION_TYPE_AUDIO &&
          !region_get_musical_mode (r))
        {
          long frames_len_after =
            arranger_object_get_length_in_frames (
              self);
          if (frames_len_after !=
                frames_len_before)
            {
              double ticks =
                (frames_len_before -
                frames_len_after) *
                AUDIO_ENGINE->ticks_per_frame;
              arranger_object_resize (
                self, false,
                ARRANGER_OBJECT_RESIZE_STRETCH_BPM_CHANGE,
                ticks, false);
            }
          long tl_frames =
            self->end_pos.frames - 1;
          long local_frames;
          AudioClip * clip;
          local_frames =
            region_timeline_frames_to_local (
              r, tl_frames, F_NORMALIZE);
          clip = audio_region_get_clip (r);
          if (local_frames >= clip->num_frames)
            {
            }
          g_return_if_fail (
            local_frames < clip->num_frames);
        }

      for (int i = 0; i < r->num_midi_notes; i++)
        {
          arranger_object_update_frames (
            (ArrangerObject *) r->midi_notes[i]);
        }
      for (int i = 0; i < r->num_unended_notes; i++)
        {
          arranger_object_update_frames (
            (ArrangerObject *) r->unended_notes[i]);
        }

      for (int i = 0; i < r->num_aps; i++)
        {
          arranger_object_update_frames (
            (ArrangerObject *) r->aps[i]);
        }

      for (int i = 0; i < r->num_chord_objects; i++)
        {
          arranger_object_update_frames (
            (ArrangerObject *) r->chord_objects[i]);
        }
      break;
    default:
      break;
    }
}

static void
add_ticks_to_region_children (
  ZRegion *    self,
  const double ticks)
{
  switch (self->id.type)
    {
    case REGION_TYPE_MIDI:
      for (int i = 0; i < self->num_midi_notes; i++)
        {
          arranger_object_move (
            (ArrangerObject *) self->midi_notes[i],
            ticks);
        }
      break;
    case REGION_TYPE_AUDIO:
      break;
    case REGION_TYPE_AUTOMATION:
      for (int i = 0; i < self->num_aps; i++)
        {
          arranger_object_move (
            (ArrangerObject *) self->aps[i],
            ticks);
        }
      break;
    case REGION_TYPE_CHORD:
      for (int i = 0; i < self->num_chord_objects;
           i++)
        {
          arranger_object_move (
            (ArrangerObject *)
              self->chord_objects[i],
            ticks);
        }
      break;
    }
}

/**
 * Adds the given ticks to each included object.
 */
void
arranger_object_add_ticks_to_children (
  ArrangerObject * self,
  const double     ticks)
{
  if (self->type == TYPE (REGION))
    {
      add_ticks_to_region_children (
        (ZRegion *) self, ticks);
    }
}

/**
 * Resizes the object on the left side or right
 * side by given amount of ticks, for objects that
 * do not have loops (currently none? keep it as
 * reference).
 *
 * @param left 1 to resize left side, 0 to resize
 *   right side.
 * @param ticks Number of ticks to resize.
 * @param during_ui_action Whether this is called
 *   during a UI action (not at the end).
 */
void
arranger_object_resize (
  ArrangerObject *         self,
  const int                left,
  ArrangerObjectResizeType type,
  const double             ticks,
  bool                     during_ui_action)
{
  double before_length =
    arranger_object_get_length_in_ticks (self);
  Position tmp;
  if (left)
    {
      if (type == ARRANGER_OBJECT_RESIZE_FADE)
        {
          tmp = self->fade_in_pos;
          position_add_ticks (&tmp, ticks);
          arranger_object_set_position (
            self, &tmp,
            ARRANGER_OBJECT_POSITION_TYPE_FADE_IN,
            F_NO_VALIDATE);
        }
      else
        {
          tmp = self->pos;
          position_add_ticks (&tmp, ticks);
          arranger_object_set_position (
            self, &tmp,
            ARRANGER_OBJECT_POSITION_TYPE_START,
            F_NO_VALIDATE);

          if (arranger_object_type_can_loop (
                self->type))
            {
              tmp = self->loop_end_pos;
              position_add_ticks (&tmp, - ticks);
              arranger_object_set_position (
                self, &tmp,
                ARRANGER_OBJECT_POSITION_TYPE_LOOP_END,
                F_NO_VALIDATE);
            }
          if (arranger_object_can_fade (self))
            {
              tmp = self->fade_out_pos;
              position_add_ticks (&tmp, - ticks);
              arranger_object_set_position (
                self, &tmp,
                ARRANGER_OBJECT_POSITION_TYPE_FADE_OUT,
                F_NO_VALIDATE);
            }

          /* move containing items */
          arranger_object_add_ticks_to_children (
            self, - ticks);
          if (type == ARRANGER_OBJECT_RESIZE_LOOP &&
              arranger_object_type_can_loop (
                self->type))
            {
              tmp = self->loop_start_pos;
              position_add_ticks (
                &tmp, - ticks);
              arranger_object_set_position (
                self, &tmp,
                ARRANGER_OBJECT_POSITION_TYPE_LOOP_START,
                F_NO_VALIDATE);
            }
        }
    }
  else
    {
      if (type == ARRANGER_OBJECT_RESIZE_FADE)
        {
          tmp = self->fade_out_pos;
          position_add_ticks (&tmp, ticks);
          arranger_object_set_position (
            self, &tmp,
            ARRANGER_OBJECT_POSITION_TYPE_FADE_OUT,
            F_NO_VALIDATE);
        }
      else
        {
          tmp = self->end_pos;
          position_add_ticks (&tmp, ticks);
          arranger_object_set_position (
            self, &tmp,
            ARRANGER_OBJECT_POSITION_TYPE_END,
            F_NO_VALIDATE);

          if (type != ARRANGER_OBJECT_RESIZE_LOOP &&
              arranger_object_type_can_loop (
                self->type))
            {
              tmp = self->loop_end_pos;
              position_add_ticks (&tmp, ticks);
              arranger_object_set_position (
                self, &tmp,
                ARRANGER_OBJECT_POSITION_TYPE_LOOP_END,
                F_NO_VALIDATE);
            }
          if (arranger_object_can_fade (self))
            {
              tmp = self->fade_out_pos;
              position_add_ticks (&tmp, ticks);
              arranger_object_set_position (
                self, &tmp,
                ARRANGER_OBJECT_POSITION_TYPE_FADE_OUT,
                F_NO_VALIDATE);
            }

          if ((type ==
                ARRANGER_OBJECT_RESIZE_STRETCH ||
               type ==
                 ARRANGER_OBJECT_RESIZE_STRETCH_BPM_CHANGE) &&
              self->type ==
                ARRANGER_OBJECT_TYPE_REGION)
            {
              /* move fade out */
              tmp = self->fade_out_pos;
              position_add_ticks (&tmp, ticks);
              arranger_object_set_position (
                self, &tmp,
                ARRANGER_OBJECT_POSITION_TYPE_FADE_OUT,
                F_NO_VALIDATE);

              ZRegion * region = (ZRegion *) self;
              double new_length =
                arranger_object_get_length_in_ticks (
                  self);

              if (type !=
                    ARRANGER_OBJECT_RESIZE_STRETCH_BPM_CHANGE)
                {
                  /* FIXME this flag is not good,
                   * remove from this function and
                   * do it in the arranger */
                  if (during_ui_action)
                    {
                      region->stretch_ratio =
                        new_length /
                        region->before_length;
                    }
                  /* else if as part of an action */
                  else
                    {
                      /* stretch contents */
                      double stretch_ratio =
                        new_length / before_length;
                      g_message ("resizing with %f",
                        stretch_ratio);
                      region_stretch (
                        region, stretch_ratio);
                    }
                }
            }
        }
    }
}

static void
post_deserialize_children (
  ArrangerObject * self)
{
  if (self->type == TYPE (REGION))
    {
      ZRegion * r = (ZRegion *) self;
      for (int i = 0; i < r->num_midi_notes; i++)
        {
          MidiNote * mn = r->midi_notes[i];
          arranger_object_post_deserialize (
            (ArrangerObject *) mn);
        }

      for (int i = 0; i < r->num_aps; i++)
        {
          AutomationPoint * ap = r->aps[i];
          arranger_object_post_deserialize (
            (ArrangerObject *) ap);
        }

      for (int i = 0; i < r->num_chord_objects;
           i++)
        {
          ChordObject * co = r->chord_objects[i];
          arranger_object_post_deserialize (
            (ArrangerObject *) co);
        }
    }
}

void
arranger_object_post_deserialize (
  ArrangerObject * self)
{
  g_return_if_fail (self);

  self->magic = ARRANGER_OBJECT_MAGIC;

  switch (self->type)
    {
    case TYPE (REGION):
      {
        ZRegion * r = (ZRegion *) self;
        r->magic = REGION_MAGIC;
      }
      break;
    case TYPE (SCALE_OBJECT):
      {
        ScaleObject * so = (ScaleObject *) self;
        so->magic = SCALE_OBJECT_MAGIC;
      }
      break;
    case TYPE (MARKER):
      {
        /*Marker * marker = (Marker *) self;*/
      }
      break;
    case TYPE (AUTOMATION_POINT):
      {
        /*AutomationPoint * ap =*/
          /*(AutomationPoint *) self;*/
      }
      break;
    case TYPE (CHORD_OBJECT):
      {
        ChordObject * co = (ChordObject *) self;
        co->magic = CHORD_OBJECT_MAGIC;
      }
      break;
    case TYPE (MIDI_NOTE):
      {
        MidiNote * mn = (MidiNote *) self;
        mn->magic = MIDI_NOTE_MAGIC;
        ((ArrangerObject *) mn->vel)->magic =
          ARRANGER_OBJECT_MAGIC;
      }
      break;
    case TYPE (VELOCITY):
      break;
    default:
      g_warn_if_reached ();
      break;
    }

  post_deserialize_children (self);
}

/**
 * The setter is for use in e.g. the digital meters
 * whereas the set_pos func is used during
 * arranger actions.
 */
void
arranger_object_pos_setter (
  ArrangerObject * self,
  const Position * pos)
{
  arranger_object_set_position (
    self, pos,
    ARRANGER_OBJECT_POSITION_TYPE_START,
    F_VALIDATE);
}

/**
 * The setter is for use in e.g. the digital meters
 * whereas the set_pos func is used during
 * arranger actions.
 */
void
arranger_object_end_pos_setter (
  ArrangerObject * self,
  const Position * pos)
{
  arranger_object_set_position (
    self, pos,
    ARRANGER_OBJECT_POSITION_TYPE_END,
    F_VALIDATE);
}

/**
 * The setter is for use in e.g. the digital meters
 * whereas the set_pos func is used during
 * arranger actions.
 */
void
arranger_object_clip_start_pos_setter (
  ArrangerObject * self,
  const Position * pos)
{
  arranger_object_set_position (
    self, pos,
    ARRANGER_OBJECT_POSITION_TYPE_CLIP_START,
    F_VALIDATE);
}

/**
 * The setter is for use in e.g. the digital meters
 * whereas the set_pos func is used during
 * arranger actions.
 */
void
arranger_object_loop_start_pos_setter (
  ArrangerObject * self,
  const Position * pos)
{
  arranger_object_set_position (
    self, pos,
    ARRANGER_OBJECT_POSITION_TYPE_LOOP_START,
    F_VALIDATE);
}

/**
 * The setter is for use in e.g. the digital meters
 * whereas the set_pos func is used during
 * arranger actions.
 */
void
arranger_object_loop_end_pos_setter (
  ArrangerObject * self,
  const Position * pos)
{
  arranger_object_set_position (
    self, pos,
    ARRANGER_OBJECT_POSITION_TYPE_LOOP_END,
    F_VALIDATE);
}

/**
 * Validates the given Position.
 *
 * @return 1 if valid, 0 otherwise.
 */
int
arranger_object_validate_pos (
  ArrangerObject *           self,
  const Position *           pos,
  ArrangerObjectPositionType type)
{
  switch (self->type)
    {
    case TYPE (REGION):
      switch (type)
        {
        case POSITION_TYPE (START):
          return
            position_is_before (
              pos, &self->end_pos) &&
            position_is_after_or_equal (
              pos, &POSITION_START);
          break;
        default:
          break;
        }
      break;
    default:
      break;
    }

  return 1;
}

/**
 * Validates the given name.
 *
 * @return True if valid, false otherwise.
 */
bool
arranger_object_validate_name (
  ArrangerObject * self,
  const char *     name)
{
  switch (self->type)
    {
    case ARRANGER_OBJECT_TYPE_REGION:
      return true;
    case ARRANGER_OBJECT_TYPE_MARKER:
      if (marker_find_by_name (name))
        {
          return false;
        }
      else
        {
          return true;
        }
      break;
    default:
      g_warn_if_reached ();
    }
  return false;
}

/**
 * Returns the Track this ArrangerObject is in.
 */
Track *
arranger_object_get_track (
  ArrangerObject * self)
{
  g_return_val_if_fail (
    IS_ARRANGER_OBJECT (self), NULL);

  Track * track = NULL;

  switch (self->type)
    {
    case TYPE (REGION):
      {
        ZRegion * r = (ZRegion *) self;
        g_return_val_if_fail (
          r->id.track_pos < TRACKLIST->num_tracks,
          NULL);
        track =
          TRACKLIST->tracks[r->id.track_pos];
      }
      break;
    case TYPE (SCALE_OBJECT):
      {
        return P_CHORD_TRACK;
      }
      break;
    case TYPE (MARKER):
      {
        Marker * marker = (Marker *) self;
        g_return_val_if_fail (
          marker->track_pos <
            TRACKLIST->num_tracks, NULL);
        track =
          TRACKLIST->tracks[marker->track_pos];
      }
      break;
    case TYPE (AUTOMATION_POINT):
      {
        AutomationPoint * ap =
          (AutomationPoint *) self;
        AutomationTrack * at =
          automation_point_get_automation_track (
            ap);
        track =
          automation_track_get_track (at);
      }
      break;
    case TYPE (CHORD_OBJECT):
    case TYPE (MIDI_NOTE):
      g_return_val_if_fail (
        self->region_id.track_pos <
          TRACKLIST->num_tracks,
        NULL);
      track =
        TRACKLIST->tracks[
          self->region_id.track_pos];
      break;
    case TYPE (VELOCITY):
      {
        Velocity * vel = (Velocity *) self;
        MidiNote * mn =
          velocity_get_midi_note (vel);
        ArrangerObject * mn_obj =
          (ArrangerObject *) mn;
        g_return_val_if_fail (
          mn_obj->region_id.track_pos <
            TRACKLIST->num_tracks,
        NULL);
        track =
          TRACKLIST->tracks[
            mn_obj->region_id.track_pos];
      }
      break;
    default:
      g_warn_if_reached ();
      break;
    }

  g_return_val_if_fail (
    IS_TRACK_AND_NONNULL (track), NULL);

  return track;
}

/**
 * Gets the arranger for this arranger object.
 */
ArrangerWidget *
arranger_object_get_arranger (
  ArrangerObject * self)
{
  g_return_val_if_fail (
    IS_ARRANGER_OBJECT (self), NULL);

  Track * track =
    arranger_object_get_track (self);
  g_return_val_if_fail (track, NULL);

  ArrangerWidget * arranger = NULL;
  switch (self->type)
    {
    case TYPE (REGION):
      {
        if (track_is_pinned (track))
          {
            arranger =
              (ArrangerWidget *) (MW_PINNED_TIMELINE);
          }
        else
          {
            arranger =
              (ArrangerWidget *) (MW_TIMELINE);
          }
      }
      break;
    case TYPE (CHORD_OBJECT):
      arranger =
        (ArrangerWidget *) (MW_CHORD_ARRANGER);
      break;
    case TYPE (SCALE_OBJECT):
      {
        if (track_is_pinned (track))
          {
            arranger =
              (ArrangerWidget *) (MW_PINNED_TIMELINE);
          }
        else
          {
            arranger =
              (ArrangerWidget *) (MW_TIMELINE);
          }
      }
      break;
    case TYPE (MARKER):
      {
        if (track_is_pinned (track))
          {
            arranger =
              (ArrangerWidget *) (MW_PINNED_TIMELINE);
          }
        else
          {
            arranger =
              (ArrangerWidget *) (MW_TIMELINE);
          }
      }
      break;
    case TYPE (AUTOMATION_POINT):
      arranger =
        (ArrangerWidget *) (MW_AUTOMATION_ARRANGER);
      break;
    case TYPE (MIDI_NOTE):
      arranger =
        (ArrangerWidget *) (MW_MIDI_ARRANGER);
      break;
    case TYPE (VELOCITY):
      arranger =
        (ArrangerWidget *) (MW_MIDI_MODIFIER_ARRANGER);
      break;
    default:
      g_return_val_if_reached (NULL);
    }

  g_warn_if_fail (arranger);
  return arranger;
}

/**
 * Returns if the lane counterpart should be visible.
 */
int
arranger_object_should_lane_be_visible (
  ArrangerObject * self)
{
  if (self->type == TYPE (REGION))
    {
      return
        arranger_object_get_track (self)->
          lanes_visible;
    }
  else
    {
      return 0;
    }
}

/**
 * Returns if the cached object should be visible,
 * ie, while copy- moving (ctrl+drag) we want to
 * show both the object at its original position
 * and the current object.
 *
 * This refers to the object at its original
 * position (called "transient").
 */
bool
arranger_object_should_orig_be_visible (
  ArrangerObject * self)
{
  g_return_val_if_fail (self, 0);

  if (!ZRYTHM_HAVE_UI)
    {
      return false;
    }

  ArrangerWidget * arranger =
    arranger_object_get_arranger (self);

  /* check trans/non-trans visiblity */
  if (ARRANGER_WIDGET_GET_ACTION (
        arranger, MOVING) ||
      ARRANGER_WIDGET_GET_ACTION (
        arranger, CREATING_MOVING))
    {
      return false;
    }
  else if (
    ARRANGER_WIDGET_GET_ACTION (
      arranger, MOVING_COPY) ||
    ARRANGER_WIDGET_GET_ACTION (
      arranger, MOVING_LINK))
    {
      return true;
    }
  else
    {
      return false;
    }
}

static ArrangerObject *
find_region (
  ZRegion * self)
{
  g_return_val_if_fail (IS_REGION (self), NULL);

  g_debug (
    "looking for project region '%s'",
    self->name);

  ArrangerObject * obj =
    (ArrangerObject *)
    region_find (&self->id);

  if (!IS_REGION (obj))
    {
      g_critical ("region not found");
      return NULL;
    }

  g_debug ("found:");
  region_print ((ZRegion *) obj);

  bool has_warning = false;
  g_debug (
    "verifying positions are the same...");
  ArrangerObject * self_obj =
    (ArrangerObject *) self;
  if (!position_is_equal_ticks (
        &self_obj->pos, &obj->pos))
    {
      char tmp[100];
      position_to_string (&self_obj->pos, tmp);
      char tmp2[100];
      position_to_string (&obj->pos, tmp2);
      g_warning (
        "start positions are not equal: "
        "%s (own) vs %s (project)",
        tmp, tmp2);
      has_warning = true;
    }
  if (!position_is_equal_ticks (
         &self_obj->end_pos, &obj->end_pos))
    {
      char tmp[100];
      position_to_string (&self_obj->end_pos, tmp);
      char tmp2[100];
      position_to_string (&obj->end_pos, tmp2);
      g_warning (
        "end positions are not equal: "
        "%s (own) vs %s (project)",
        tmp, tmp2);
      has_warning = true;
    }

  if (has_warning)
    {
      g_debug ("own region:");
      region_print (self);
      g_debug ("found region:");
      region_print ((ZRegion *) obj);
    }

  g_debug ("checked positions");

  return obj;
}

static ArrangerObject *
find_chord_object (
  ChordObject * clone)
{
  ArrangerObject * clone_obj =
    (ArrangerObject *) clone;

  /* get actual region - clone's region might be
   * an unused clone */
  ZRegion * r =
    region_find (&clone_obj->region_id);
  g_return_val_if_fail (r, NULL);

  g_return_val_if_fail (
    r && r->num_chord_objects > clone->index, NULL);

  return
    (ArrangerObject *)
    r->chord_objects[clone->index];
}

static ArrangerObject *
find_scale_object (
  ScaleObject * clone)
{
  for (int i = 0;
       i < P_CHORD_TRACK->num_scales; i++)
    {
      if (scale_object_is_equal (
            P_CHORD_TRACK->scales[i],
            clone))
        {
          return
            (ArrangerObject *)
            P_CHORD_TRACK->scales[i];
        }
    }
  return NULL;
}

static ArrangerObject *
find_marker (
  Marker * clone)
{
  g_return_val_if_fail (
    P_MARKER_TRACK->num_markers > clone->index,
    NULL);

  Marker * marker =
    P_MARKER_TRACK->markers[clone->index];
  g_warn_if_fail (
    marker_is_equal (marker, clone));

  return (ArrangerObject *) marker;
}

static ArrangerObject *
find_automation_point (
  AutomationPoint * src)
{
  ArrangerObject * src_obj =
    (ArrangerObject *) src;
  ZRegion * region =
    region_find (&src_obj->region_id);
  g_return_val_if_fail (
    region && region->num_aps > src->index, NULL);

  AutomationPoint * ap = region->aps[src->index];
  g_return_val_if_fail (
    automation_point_is_equal (src, ap), NULL);

  return (ArrangerObject *) ap;
}

static ArrangerObject *
find_midi_note (
  MidiNote * src)
{
  ArrangerObject * src_obj =
    (ArrangerObject *) src;
  ZRegion * r =
    region_find (&src_obj->region_id);
  g_return_val_if_fail (
    r && r->num_midi_notes > src->pos, NULL);

  return
    (ArrangerObject *)
    r->midi_notes[src->pos];
}

/**
 * Returns the ArrangerObject matching the
 * given one.
 *
 * This should be called when we have a copy or a
 * clone, to get the actual region in the project.
 */
ArrangerObject *
arranger_object_find (
  ArrangerObject * self)
{
  switch (self->type)
    {
    case TYPE (REGION):
      return
        find_region ((ZRegion *) self);
    case TYPE (CHORD_OBJECT):
      return
        find_chord_object ((ChordObject *) self);
    case TYPE (SCALE_OBJECT):
      return
        find_scale_object ((ScaleObject *) self);
    case TYPE (MARKER):
      return
        find_marker ((Marker *) self);
    case TYPE (AUTOMATION_POINT):
      return
        find_automation_point (
          (AutomationPoint *) self);
    case TYPE (MIDI_NOTE):
      return
        find_midi_note ((MidiNote *) self);
    case TYPE (VELOCITY):
      {
        Velocity * clone =
          (Velocity *) self;
        MidiNote * mn =
          (MidiNote *)
          velocity_get_midi_note (clone);
        g_return_val_if_fail (mn && mn->vel, NULL);
        return (ArrangerObject *) mn->vel;
      }
    default:
      g_return_val_if_reached (NULL);
    }
  g_return_val_if_reached (NULL);
}

static ArrangerObject *
clone_region (
  ZRegion *               region,
  ArrangerObjectCloneFlag flag)
{
  int i, j;

  ArrangerObject * r_obj =
    (ArrangerObject *) region;
  ZRegion * new_region = NULL;
  switch (region->id.type)
    {
    case REGION_TYPE_MIDI:
      {
        ZRegion * mr =
          midi_region_new (
            &r_obj->pos, &r_obj->end_pos,
            region->id.track_pos,
            region->id.lane_pos,
            region->id.idx);
        ZRegion * mr_orig = region;
        for (i = 0;
             i < mr_orig->num_midi_notes; i++)
          {
            MidiNote * orig_mn =
              mr_orig->midi_notes[i];
            ArrangerObject * orig_mn_obj =
              (ArrangerObject *) orig_mn;
            MidiNote * mn;

            region_identifier_copy (
              &orig_mn_obj->region_id,
              &mr_orig->id);
            mn =
              (MidiNote *)
              arranger_object_clone (
                (ArrangerObject *)
                mr_orig->midi_notes[i],
                ARRANGER_OBJECT_CLONE_COPY_MAIN);

            midi_region_add_midi_note (
              mr, mn, F_NO_PUBLISH_EVENTS);
          }

        new_region = (ZRegion *) mr;
      }
    break;
    case REGION_TYPE_AUDIO:
      {
        ZRegion * ar =
          audio_region_new (
            region->pool_id, NULL, NULL, -1,
            NULL, 0, &r_obj->pos,
            region->id.track_pos,
            region->id.lane_pos,
            region->id.idx);

#if 0
        /* copy the actual frames - they might
         * be different from the clip due to
         * eg. stretching */
        AudioClip * clip =
          audio_region_get_clip (region);
        size_t frame_bytes_size =
          sizeof (float) *
            (size_t) region->num_frames *
            clip->channels;
        ar->frames =
          realloc (
            ar->frames, frame_bytes_size);
        ar->num_frames = region->num_frames;
        memcpy (
          &ar->frames[0], &region->frames[0],
          frame_bytes_size);
#endif

        new_region = ar;
        new_region->pool_id = region->pool_id;
        ar->musical_mode = region->musical_mode;
      }
    break;
    case REGION_TYPE_AUTOMATION:
      {
        ZRegion * ar  =
          automation_region_new (
            &r_obj->pos, &r_obj->end_pos,
            region->id.track_pos,
            region->id.at_idx,
            region->id.idx);
        ZRegion * ar_orig = region;

        /* add automation points */
        AutomationPoint * src_ap, * dest_ap;
        for (j = 0; j < ar_orig->num_aps; j++)
          {
            src_ap = ar_orig->aps[j];
            ArrangerObject * src_ap_obj =
              (ArrangerObject *) src_ap;

            dest_ap =
              automation_point_new_float (
                src_ap->fvalue,
                src_ap->normalized_val,
                &src_ap_obj->pos);
            dest_ap->curve_opts =
              src_ap->curve_opts;
            automation_region_add_ap (
              ar, dest_ap, F_NO_PUBLISH_EVENTS);
          }

        new_region = ar;
      }
      break;
    case REGION_TYPE_CHORD:
      {
        ZRegion * cr =
          chord_region_new (
            &r_obj->pos, &r_obj->end_pos,
            region->id.idx);
        ZRegion * cr_orig = region;
        ChordObject * src_co, * dest_co;
        for (i = 0;
             i < cr_orig->num_chord_objects;
             i++)
          {
            src_co = cr_orig->chord_objects[i];

            dest_co =
              (ChordObject *)
              arranger_object_clone (
                (ArrangerObject *) src_co,
                ARRANGER_OBJECT_CLONE_COPY_MAIN);

            chord_region_add_chord_object (
              cr, dest_co, F_NO_PUBLISH_EVENTS);
          }

        new_region = cr;
      }
      break;
    }

  g_return_val_if_fail (
    new_region &&
    new_region->schema_version ==
      REGION_SCHEMA_VERSION,
    NULL);

  /* clone name */
  new_region->name = g_strdup (region->name);

  /* set track to NULL and remember track pos */
  region_identifier_copy (
    &new_region->id, &region->id);
  g_warn_if_fail (new_region->id.idx >= 0);

  return (ArrangerObject *) new_region;
}

/**
 * Returns a pointer to the name of the object,
 * if the object can have names.
 */
const char *
arranger_object_get_name (
  ArrangerObject * self)
{
  switch (self->type)
    {
    case ARRANGER_OBJECT_TYPE_REGION:
      {
        ZRegion * r = (ZRegion *) self;
        return r->name;
      }
      break;
    case ARRANGER_OBJECT_TYPE_MARKER:
      {
        Marker * m = (Marker *) self;
        return m->name;
      }
      break;
    default:
      break;
    }
  return NULL;
}

static ArrangerObject *
clone_midi_note (
  MidiNote *              src,
  ArrangerObjectCloneFlag flag)
{
  ArrangerObject * src_obj =
    (ArrangerObject *) src;
  MidiNote * mn =
    midi_note_new (
      &src_obj->region_id, &src_obj->pos,
      &src_obj->end_pos,
      src->val, src->vel->vel);
  mn->currently_listened = src->currently_listened;
  mn->last_listened_val = src->last_listened_val;
  mn->pos = src->pos;
  mn->vel->vel_at_start = src->vel->vel_at_start;

  return (ArrangerObject *) mn;
}

static ArrangerObject *
clone_chord_object (
  ChordObject *           src,
  ArrangerObjectCloneFlag flag)
{
  ArrangerObject * src_obj =
    (ArrangerObject *) src;
  ChordObject * chord =
    chord_object_new (
      &src_obj->region_id, src->chord_index,
      src->index);

  return (ArrangerObject *) chord;
}

static ArrangerObject *
clone_scale_object (
  ScaleObject *           src,
  ArrangerObjectCloneFlag flag)
{
  int is_main = 0;
  if (flag == ARRANGER_OBJECT_CLONE_COPY_MAIN)
    is_main = 1;

  MusicalScale * musical_scale =
    musical_scale_clone (src->scale);
  ScaleObject * scale =
    scale_object_new (musical_scale, is_main);

  return (ArrangerObject *) scale;
}

static ArrangerObject *
clone_marker (
  Marker *                src,
  ArrangerObjectCloneFlag flag)
{
  Marker * marker = marker_new (src->name);
  marker->index = src->index;
  marker->track_pos = src->track_pos;

  return (ArrangerObject *) marker;
}

static ArrangerObject *
clone_automation_point (
  AutomationPoint *       src,
  ArrangerObjectCloneFlag flag)
{
  if (ZRYTHM_TESTING)
    {
      g_return_val_if_fail (
        math_assert_nonnann (
          src->normalized_val) &&
        math_assert_nonnann (src->fvalue),
        NULL);
    }

  ArrangerObject * src_obj =
    (ArrangerObject *) src;
  AutomationPoint * ap =
    automation_point_new_float (
      src->fvalue, src->normalized_val,
      &src_obj->pos);
  ap->curve_opts = src->curve_opts;
  ArrangerObject * ap_obj =
    (ArrangerObject *) ap;
  region_identifier_copy (
    &ap_obj->region_id, &src_obj->region_id);
  ap->index = src->index;

  return ap_obj;
}

/**
 * Clone the ArrangerObject.
 *
 * Creates a new object and either links to the
 * original or copies every field.
 */
ArrangerObject *
arranger_object_clone (
  ArrangerObject *        self,
  ArrangerObjectCloneFlag flag)
{
  g_return_val_if_fail (self, NULL);

  ArrangerObject * new_obj = NULL;
  switch (self->type)
    {
    case TYPE (REGION):
      new_obj =
        clone_region ((ZRegion *) self, flag);
      break;
    case TYPE (MIDI_NOTE):
      new_obj =
        clone_midi_note ((MidiNote *) self, flag);
      break;
    case TYPE (CHORD_OBJECT):
      new_obj =
        clone_chord_object (
          (ChordObject *) self, flag);
      break;
    case TYPE (SCALE_OBJECT):
      new_obj =
        clone_scale_object (
          (ScaleObject *) self, flag);
      break;
    case TYPE (AUTOMATION_POINT):
      new_obj =
        clone_automation_point (
          (AutomationPoint *) self, flag);
      break;
    case TYPE (MARKER):
      new_obj =
        clone_marker (
          (Marker *) self, flag);
      break;
    case TYPE (VELOCITY):
      {
        Velocity * src = (Velocity *) self;
        MidiNote * mn =
          velocity_get_midi_note (src);
        Velocity * new_vel =
          velocity_new (mn, src->vel);
        new_obj =
          (ArrangerObject *) new_vel;
        new_vel->vel_at_start = src->vel_at_start;
      }
      break;
    default:
      g_return_val_if_reached (NULL);
    }
  g_return_val_if_fail (new_obj, NULL);

  /* set positions */
  g_warn_if_fail (
    self->schema_version ==
      ARRANGER_OBJECT_SCHEMA_VERSION &&
    self->pos.schema_version ==
      POSITION_SCHEMA_VERSION);
  new_obj->pos = self->pos;
  if (arranger_object_type_has_length (self->type))
    {
      new_obj->end_pos = self->end_pos;
    }
  if (arranger_object_type_can_loop (self->type))
    {
      new_obj->clip_start_pos = self->clip_start_pos;
      new_obj->loop_start_pos = self->loop_start_pos;
      new_obj->loop_end_pos = self->loop_end_pos;
    }
  if (arranger_object_can_fade (self))
    {
      new_obj->fade_in_pos = self->fade_in_pos;
      new_obj->fade_out_pos = self->fade_out_pos;
      new_obj->fade_in_opts = self->fade_in_opts;
      new_obj->fade_out_opts = self->fade_out_opts;
    }
  if (arranger_object_can_mute (self))
    {
      new_obj->muted = self->muted;
    }

  new_obj->magic = ARRANGER_OBJECT_MAGIC;
  new_obj->index_in_prev_lane =
    self->index_in_prev_lane;

  return new_obj;
}

/**
 * Splits the given object at the given Position.
 *
 * if \ref is_project is true, it
 * deletes the original object and adds 2 new
 * objects in the same parent (Track or
 * AutomationTrack or Region).
 *
 * @param region The ArrangerObject to split. This
 *   ArrangerObject will be deleted.
 * @param pos The Position to split at.
 * @param pos_is_local If the position is local (1)
 *   or global (0).
 * @param r1 Address to hold the pointer to the
 *   newly created ArrangerObject 1.
 * @param r2 Address to hold the pointer to the
 *   newly created ArrangerObject 2.
 * @param is_project Whether the object being
 *   passed is a project object. If true, it will
 *   be removed from the project and the child
 *   objects will be added to the project,
 *   otherwise it will be untouched and the
 *   children will be mere clones.
 */
void
arranger_object_split (
  ArrangerObject *  self,
  const Position *  pos,
  const bool        pos_is_local,
  ArrangerObject ** r1,
  ArrangerObject ** r2,
  bool              is_project)
{
  g_return_if_fail (IS_ARRANGER_OBJECT (self));

  /* create the new objects */
  *r1 =
    arranger_object_clone (
      self, ARRANGER_OBJECT_CLONE_COPY_MAIN);
  *r2 =
    arranger_object_clone (
      self, ARRANGER_OBJECT_CLONE_COPY_MAIN);

  g_debug ("splitting objects...");

  bool set_clip_editor_region = false;
  if (is_project)
    {
      /* change to r1 if the original region was
       * the clip editor region */
      ZRegion * clip_editor_region =
        clip_editor_get_region (CLIP_EDITOR);
      if (clip_editor_region == (ZRegion *) self)
        {
          set_clip_editor_region = true;
          clip_editor_set_region (
            CLIP_EDITOR, NULL, true);
        }
    }

  /* get global/local positions (the local pos
   * is after traversing the loops) */
  Position globalp, localp;
  if (pos_is_local)
    {
      position_set_to_pos (&globalp, pos);
      position_add_ticks (
        &globalp, self->pos.ticks);
      position_set_to_pos (&localp, pos);
    }
  else
    {
      position_set_to_pos (&globalp, pos);
      if (self->type == ARRANGER_OBJECT_TYPE_REGION)
        {
          long localp_frames =
            region_timeline_frames_to_local (
              (ZRegion *) self, globalp.frames, 1);
          position_from_frames (
            &localp, localp_frames);

          g_return_if_fail (
            position_is_after (
              &globalp, &self->pos) &&
            position_is_before (
              &globalp, &self->end_pos));
        }
      else
        {
          position_set_to_pos (&localp, &globalp);
        }
    }

  /* for first region set the end pos and fade
   * out pos */
  arranger_object_end_pos_setter (
    *r1, &globalp);
  arranger_object_set_position (
    *r1, &localp,
    ARRANGER_OBJECT_POSITION_TYPE_FADE_OUT,
    F_NO_VALIDATE);

  /* for the second set the clip start and start
   * pos. */
  arranger_object_clip_start_pos_setter (
    *r2, &localp);
  arranger_object_pos_setter (
    *r2, &globalp);
  Position r2_fade_out;
  position_set_to_pos (
    &r2_fade_out, &((*r2)->end_pos));
  position_add_ticks (
    &r2_fade_out, - (*r2)->pos.ticks);
  arranger_object_set_position (
    *r2, &r2_fade_out,
    ARRANGER_OBJECT_POSITION_TYPE_FADE_OUT,
    F_NO_VALIDATE);

  /* skip rest if non-project object */
  if (!is_project)
    {
      return;
    }

  /* add them to the parent */
  switch (self->type)
    {
    case ARRANGER_OBJECT_TYPE_REGION:
      {
        Track * track =
          arranger_object_get_track (self);
        ZRegion * src_region =
          (ZRegion *) self;
        ZRegion * region1 =
          (ZRegion *) *r1;
        ZRegion * region2 =
          (ZRegion *) *r2;
        AutomationTrack * at = NULL;
        if (src_region->id.type ==
              REGION_TYPE_AUTOMATION)
          {
            at =
              region_get_automation_track (
                src_region);
          }
        track_add_region (
          track, region1, at,
          src_region->id.lane_pos,
          F_GEN_NAME, F_PUBLISH_EVENTS);
        track_add_region (
          track, region2, at,
          src_region->id.lane_pos,
          F_GEN_NAME, F_PUBLISH_EVENTS);
      }
      break;
    case ARRANGER_OBJECT_TYPE_MIDI_NOTE:
      {
        MidiNote * src_midi_note =
          (MidiNote *) self;
        ZRegion * parent_region =
          midi_note_get_region (src_midi_note);
        midi_region_add_midi_note (
          parent_region, (MidiNote *) *r1, 1);
        midi_region_add_midi_note (
          parent_region, (MidiNote *) *r2, 1);
      }
      break;
    default:
      break;
    }

  /* select the first one */
  ArrangerSelections * sel =
    arranger_object_get_selections_for_type (
      self->type);
  arranger_selections_remove_object (sel, self);
  arranger_selections_add_object (sel, *r1);
  /*arranger_selections_add_object (sel, *r2);*/

  /* remove and free the original object */
  switch (self->type)
    {
    case ARRANGER_OBJECT_TYPE_REGION:
      {
        track_remove_region (
          arranger_object_get_track (self),
          (ZRegion *) self, F_PUBLISH_EVENTS,
          F_FREE);
        ZRegion * region1 = (ZRegion *) *r1;
        ZRegion * region2 = (ZRegion *) *r2;
        if (region1->id.type == REGION_TYPE_CHORD)
          {
            g_return_if_fail (
              region1->id.idx <
                P_CHORD_TRACK->num_chord_regions);
            g_return_if_fail (
              region2->id.idx <
                P_CHORD_TRACK->num_chord_regions);
          }
      }
      break;
    case ARRANGER_OBJECT_TYPE_MIDI_NOTE:
      {
        ZRegion * parent_region =
          midi_note_get_region (
            ((MidiNote *) self));
        midi_region_remove_midi_note (
          parent_region, (MidiNote *) self,
          F_FREE, F_PUBLISH_EVENTS);
      }
      break;
    default:
      g_warn_if_reached ();
      break;
    }

  if (set_clip_editor_region)
    {
      clip_editor_set_region (
        CLIP_EDITOR, (ZRegion *) *r1, true);
    }

  EVENTS_PUSH (ET_ARRANGER_OBJECT_CREATED, *r1);
  EVENTS_PUSH (ET_ARRANGER_OBJECT_CREATED, *r2);
}

/**
 * Undoes what arranger_object_split() did.
 */
void
arranger_object_unsplit (
  ArrangerObject *  r1,
  ArrangerObject *  r2,
  ArrangerObject ** obj,
  bool              fire_events)
{
  g_debug ("unsplitting objects...");

  /* change to the original region if the clip
   * editor region is r1 or r2 */
  ZRegion * clip_editor_region =
    clip_editor_get_region (CLIP_EDITOR);
  bool set_clip_editor_region = false;
  if (clip_editor_region == (ZRegion *) r1 ||
      clip_editor_region == (ZRegion *) r2)
    {
      set_clip_editor_region = true;
      clip_editor_set_region (
        CLIP_EDITOR, NULL, true);
    }

  /* create the new object */
  *obj =
    arranger_object_clone (
      r1, ARRANGER_OBJECT_CLONE_COPY_MAIN);

  /* set the end pos to the end pos of r2 and
   * fade out */
  arranger_object_end_pos_setter (
    *obj, &r2->end_pos);
  Position fade_out_pos;
  position_set_to_pos (
    &fade_out_pos, &r2->end_pos);
  position_add_ticks (
    &fade_out_pos, - r2->pos.ticks);
  arranger_object_set_position (
    *obj, &fade_out_pos,
    ARRANGER_OBJECT_POSITION_TYPE_FADE_OUT,
    F_NO_VALIDATE);

  /* add it to the parent */
  switch (r1->type)
    {
    case ARRANGER_OBJECT_TYPE_REGION:
      {
        ZRegion * r1_region = (ZRegion *) r1;
        AutomationTrack * at = NULL;
        if (r1_region->id.type ==
              REGION_TYPE_AUTOMATION)
          {
            at =
              region_get_automation_track (
                r1_region);
          }
        track_add_region (
          arranger_object_get_track (r1),
          (ZRegion *) *obj, at,
          ((ZRegion *) r1)->id.lane_pos,
          F_GEN_NAME, fire_events);
      }
      break;
    case ARRANGER_OBJECT_TYPE_MIDI_NOTE:
      {
        ZRegion * parent_region =
          midi_note_get_region (
            ((MidiNote *) r1));
        midi_region_add_midi_note (
          parent_region, (MidiNote *) *obj, 1);
      }
      break;
    default:
      break;
    }

  /* generate widgets so update visibility in the
   * arranger can work */
  /*arranger_object_gen_widget (*obj);*/

  /* select it */
  ArrangerSelections * sel =
    arranger_object_get_selections_for_type (
      (*obj)->type);
  arranger_selections_remove_object (
    sel, r1);
  arranger_selections_remove_object (
    sel, r2);
  arranger_selections_add_object (
    sel, *obj);

  /* remove and free the original regions */
  switch (r1->type)
    {
    case ARRANGER_OBJECT_TYPE_REGION:
      {
        track_remove_region (
          arranger_object_get_track (r1),
          (ZRegion *) r1, fire_events,
          F_FREE);
        track_remove_region (
          arranger_object_get_track (r2),
          (ZRegion *) r2, fire_events,
          F_FREE);
      }
      break;
    case ARRANGER_OBJECT_TYPE_MIDI_NOTE:
      {
        MidiNote * mn1 = (MidiNote *) r1;
        MidiNote * mn2 = (MidiNote *) r2;
        ZRegion * region1 =
          midi_note_get_region (mn1);
        ZRegion * region2 =
          midi_note_get_region (mn2);
        midi_region_remove_midi_note (
          region1, mn1,
          fire_events, F_FREE);
        midi_region_remove_midi_note (
          region2, mn2,
          fire_events, F_FREE);
      }
      break;
    default:
      break;
    }

  if (set_clip_editor_region)
    {
      clip_editor_set_region (
        CLIP_EDITOR, (ZRegion *) *obj, true);
    }

  if (fire_events)
    {
      EVENTS_PUSH (
        ET_ARRANGER_OBJECT_CREATED, *obj);
    }
}

/**
 * Sets the name of the object, if the object can
 * have a name.
 */
void
arranger_object_set_name (
  ArrangerObject * self,
  const char *     name,
  int              fire_events)
{
  g_return_if_fail (IS_ARRANGER_OBJECT (self));
  switch (self->type)
    {
    case ARRANGER_OBJECT_TYPE_MARKER:
      {
        arranger_object_set_string (
          Marker, self, name, name);
      }
      break;
    case ARRANGER_OBJECT_TYPE_REGION:
      {
        arranger_object_set_string (
          ZRegion, self, name, name);
      }
      break;
    default:
      break;
    }
  if (fire_events)
    {
      EVENTS_PUSH (
        ET_ARRANGER_OBJECT_CHANGED, self);
    }
}

/**
 * Changes the name and adds an action to the
 * undo stack.
 *
 * Calls arranger_object_set_name() internally.
 */
void
arranger_object_set_name_with_action (
  ArrangerObject * self,
  const char *     name)
{
  /* validate */
  if (!arranger_object_validate_name (self, name))
    {
      char * msg =
        g_strdup_printf (
          _("Invalid object name %s"), name);
      ui_show_error_message (MAIN_WINDOW, msg);
      return;
    }

  ArrangerObject * clone_obj =
    arranger_object_clone (
      self, ARRANGER_OBJECT_CLONE_COPY_MAIN);
  g_return_if_fail (
    IS_ARRANGER_OBJECT_AND_NONNULL (clone_obj));

  /* prepare the before/after selections to
   * create the undoable action */
  ArrangerSelections * before =
    arranger_selections_clone (
      (ArrangerSelections *) TL_SELECTIONS);
  g_return_if_fail (
    IS_ARRANGER_SELECTIONS_AND_NONNULL (before));
  arranger_selections_clear (
    before, F_FREE, F_NO_PUBLISH_EVENTS);
  arranger_selections_add_object (
    before, clone_obj);
  ArrangerSelections * after =
    arranger_selections_clone (
      (ArrangerSelections *) before);
  ArrangerObject * after_obj =
    arranger_selections_get_first_object (after);
  arranger_object_set_name (
    after_obj, name, F_NO_PUBLISH_EVENTS);

  UndoableAction * ua =
    arranger_selections_action_new_edit (
      before, after,
      ARRANGER_SELECTIONS_ACTION_EDIT_NAME,
      F_NOT_ALREADY_EDITED);
  undo_manager_perform (UNDO_MANAGER, ua);
  arranger_selections_free_full (before);
  arranger_selections_free_full (after);
}

static void
set_loop_and_fade_to_full_size (
  ArrangerObject * obj)
{
  if (arranger_object_type_can_loop (obj->type))
    {
      double ticks =
        arranger_object_get_length_in_ticks (obj);
      position_from_ticks (
        &obj->loop_end_pos, ticks);
    }
  if (arranger_object_can_fade (obj))
    {
      double ticks =
        arranger_object_get_length_in_ticks (obj);
      position_from_ticks (
        &obj->fade_out_pos, ticks);
    }
}

/**
 * Sets the end position of the ArrangerObject and
 * also sets the loop end and fade out so that
 * they are at the end.
 */
void
arranger_object_set_start_pos_full_size (
  ArrangerObject * obj,
  Position *       pos)
{
  arranger_object_pos_setter (obj, pos);
  set_loop_and_fade_to_full_size (obj);
  g_warn_if_fail (
    pos->frames == obj->pos.frames);
}

/**
 * Sets the end position of the ArrangerObject and
 * also sets the loop end and fade out to that
 * position.
 */
void
arranger_object_set_end_pos_full_size (
  ArrangerObject * obj,
  Position *       pos)
{
  arranger_object_end_pos_setter (obj, pos);
  set_loop_and_fade_to_full_size (obj);
  g_warn_if_fail (
    pos->frames == obj->end_pos.frames);
}

/**
 * Appends the ArrangerObject to where it belongs
 * in the project (eg, a Track), without taking
 * into account its previous index (eg, before
 * deletion if undoing).
 */
void
arranger_object_add_to_project (
  ArrangerObject * obj,
  bool             fire_events)
{
  /* find the region (if owned by region) */
  ZRegion * region = NULL;
  if (arranger_object_owned_by_region (obj))
    {
      region = region_find (&obj->region_id);
      g_return_if_fail (region);
    }

  switch (obj->type)
    {
    case ARRANGER_OBJECT_TYPE_AUTOMATION_POINT:
      {
        AutomationPoint * ap =
          (AutomationPoint *) obj;

        /* add it to the region */
        g_return_if_fail (region);
        automation_region_add_ap (
          region, ap, fire_events);
      }
      break;
    case ARRANGER_OBJECT_TYPE_CHORD_OBJECT:
      {
        ChordObject * chord =
          (ChordObject *) obj;

        /* add it to the region */
        g_return_if_fail (region);
        chord_region_add_chord_object (
          region, chord, fire_events);
      }
      break;
    case ARRANGER_OBJECT_TYPE_MIDI_NOTE:
      {
        MidiNote * mn =
          (MidiNote *) obj;

        /* add it to the region */
        g_return_if_fail (region);
        midi_region_add_midi_note (
          region, mn, fire_events);
      }
      break;
    case ARRANGER_OBJECT_TYPE_SCALE_OBJECT:
      {
        ScaleObject * scale =
          (ScaleObject *) obj;

        /* add it to the track */
        chord_track_add_scale (
          P_CHORD_TRACK, scale);
      }
      break;
    case ARRANGER_OBJECT_TYPE_MARKER:
      {
        Marker * marker =
          (Marker *) obj;

        /* add it to the track */
        marker_track_add_marker (
          P_MARKER_TRACK, marker);
      }
      break;
    case ARRANGER_OBJECT_TYPE_REGION:
      {
        ZRegion * r = (ZRegion *) obj;

        /* add it to track */
        Track * track =
          TRACKLIST->tracks[r->id.track_pos];
        switch (r->id.type)
          {
          case REGION_TYPE_AUTOMATION:
            {
              AutomationTrack * at =
                track->
                  automation_tracklist.
                    ats[r->id.at_idx];
              track_add_region (
                track, r, at, -1,
                F_GEN_NAME,
                fire_events);
            }
            break;
          case REGION_TYPE_CHORD:
            track_add_region (
              P_CHORD_TRACK, r, NULL,
              -1, F_GEN_NAME,
              fire_events);
            break;
          default:
            track_add_region (
              track, r, NULL, r->id.lane_pos,
              F_GEN_NAME,
              fire_events);
            break;
          }

        /* if region, also set is as the clip
         * editor region */
        clip_editor_set_region (
          CLIP_EDITOR, r, true);
      }
      break;
    default:
      g_warn_if_reached ();
      break;
    }
}

/**
 * Inserts the ArrangerObject where it belongs in
 * the project (eg, a Track).
 *
 * This function assumes that the object already
 * knows the index where it should be inserted
 * in its parent.
 *
 * This is mostly used when undoing.
 */
void
arranger_object_insert_to_project (
  ArrangerObject * obj)
{
  /* find the region (if owned by region) */
  ZRegion * region = NULL;
  if (arranger_object_owned_by_region (obj))
    {
      region = region_find (&obj->region_id);
      g_return_if_fail (
        IS_REGION_AND_NONNULL (region));
    }

  switch (obj->type)
    {
    case ARRANGER_OBJECT_TYPE_AUTOMATION_POINT:
      {
        AutomationPoint * ap =
          (AutomationPoint *) obj;

        /* add it to the region */
        g_return_if_fail (
          IS_REGION_AND_NONNULL (region));
        automation_region_add_ap (
          region, ap, F_NO_PUBLISH_EVENTS);
      }
      break;
    case ARRANGER_OBJECT_TYPE_CHORD_OBJECT:
      {
        ChordObject * chord =
          (ChordObject *) obj;

        /* add it to the region */
        g_return_if_fail (
          IS_REGION_AND_NONNULL (region));
        chord_region_add_chord_object (
          region, chord, F_NO_PUBLISH_EVENTS);
      }
      break;
    case ARRANGER_OBJECT_TYPE_MIDI_NOTE:
      {
        MidiNote * mn =
          (MidiNote *) obj;

        /* add it to the region */
        g_return_if_fail (
          IS_REGION_AND_NONNULL (region));
        midi_region_insert_midi_note (
          region, mn, mn->pos, F_PUBLISH_EVENTS);
      }
      break;
    case ARRANGER_OBJECT_TYPE_SCALE_OBJECT:
      {
        ScaleObject * scale =
          (ScaleObject *) obj;

        /* add it to the track */
        chord_track_add_scale (
          P_CHORD_TRACK, scale);
      }
      break;
    case ARRANGER_OBJECT_TYPE_MARKER:
      {
        Marker * marker =
          (Marker *) obj;

        /* add it to the track */
        marker_track_add_marker (
          P_MARKER_TRACK, marker);
      }
      break;
    case ARRANGER_OBJECT_TYPE_REGION:
      {
        ZRegion * r = (ZRegion *) obj;

        /* add it to track */
        Track * track =
          TRACKLIST->tracks[r->id.track_pos];
        switch (r->id.type)
          {
          case REGION_TYPE_AUTOMATION:
            {
              AutomationTrack * at =
                track->
                  automation_tracklist.
                    ats[r->id.at_idx];
              track_insert_region (
                track, r, at, -1, r->id.idx,
                F_GEN_NAME,
                F_PUBLISH_EVENTS);
            }
            break;
          case REGION_TYPE_CHORD:
            track_insert_region (
              P_CHORD_TRACK, r, NULL,
              -1, r->id.idx, F_GEN_NAME,
              F_PUBLISH_EVENTS);
            break;
          default:
            track_insert_region (
              track, r, NULL, r->id.lane_pos,
              r->id.idx, F_GEN_NAME,
              F_PUBLISH_EVENTS);
            break;
          }

        /* if region, also set is as the clip
         * editor region */
        clip_editor_set_region (
          CLIP_EDITOR, r, true);
      }
      break;
    default:
      g_warn_if_reached ();
      break;
    }
}

/**
 * Removes the object from its parent in the
 * project.
 */
void
arranger_object_remove_from_project (
  ArrangerObject * obj)
{
  /* TODO make sure no event contains this object */
  /*event_manager_remove_events_for_obj (*/
    /*EVENT_MANAGER, obj);*/

  switch (obj->type)
    {
    case ARRANGER_OBJECT_TYPE_AUTOMATION_POINT:
      {
        AutomationPoint * ap =
          (AutomationPoint *) obj;
        ZRegion * region =
          arranger_object_get_region (obj);
        g_return_if_fail (
          IS_REGION_AND_NONNULL (region));
        automation_region_remove_ap (
          region, ap, false, F_FREE);
      }
      break;
    case ARRANGER_OBJECT_TYPE_CHORD_OBJECT:
      {
        ChordObject * chord =
          (ChordObject *) obj;
        ZRegion * region =
          arranger_object_get_region (obj);
        g_return_if_fail (
          IS_REGION_AND_NONNULL (region));
        chord_region_remove_chord_object (
          region, chord, F_FREE,
          F_NO_PUBLISH_EVENTS);
      }
      break;
    case ARRANGER_OBJECT_TYPE_REGION:
      {
        ZRegion * r =
          (ZRegion *) obj;
        Track * track =
          arranger_object_get_track (obj);
        g_return_if_fail (
          IS_TRACK_AND_NONNULL (track));
        track_remove_region (
          track, r, F_PUBLISH_EVENTS, F_FREE);
      }
      break;
    case ARRANGER_OBJECT_TYPE_SCALE_OBJECT:
      {
        ScaleObject * scale =
          (ScaleObject *) obj;
        chord_track_remove_scale (
          P_CHORD_TRACK, scale,
          F_FREE);
      }
      break;
    case ARRANGER_OBJECT_TYPE_MARKER:
      {
        Marker * marker =
          (Marker *) obj;
        marker_track_remove_marker (
          P_MARKER_TRACK, marker, F_FREE);
      }
      break;
    case ARRANGER_OBJECT_TYPE_MIDI_NOTE:
      {
        MidiNote * mn =
          (MidiNote *) obj;
        ZRegion * region =
          arranger_object_get_region (obj);
        midi_region_remove_midi_note (
          region, mn, F_FREE,
          F_NO_PUBLISH_EVENTS);
      }
      break;
    default:
      break;
    }
}

/**
 * Returns whether the arranger object is part of
 * a frozen track.
 */
bool
arranger_object_is_frozen (
  ArrangerObject * obj)
{
  Track * track =
    arranger_object_get_track (obj);
  return track->frozen;
}

/**
 * Returns whether the given object is deletable
 * or not (eg, start marker).
 */
bool
arranger_object_is_deletable (
  ArrangerObject * obj)
{
  switch (obj->type)
    {
    case ARRANGER_OBJECT_TYPE_MARKER:
      {
        Marker * m = (Marker *) obj;
        return marker_is_deletable (m);
      }
      break;
    default:
      break;
    }
  return true;
}

static void
free_region (
  ZRegion * self)
{
  g_return_if_fail (IS_REGION (self));

  g_message ("freeing region %s...", self->name);

#define FREE_R(type,sc) \
  case REGION_TYPE_##type: \
    sc##_region_free_members (self); \
  break

  switch (self->id.type)
    {
      FREE_R (MIDI, midi);
      FREE_R (AUDIO, audio);
      FREE_R (CHORD, chord);
      FREE_R (AUTOMATION, automation);
    }

  g_free_and_null (self->name);
  if (G_IS_OBJECT (self->layout))
    {
      object_free_w_func_and_null (
        g_object_unref, self->layout);
    }

#undef FREE_R

  object_zero_and_free (self);
}

static void
free_midi_note (
  MidiNote * self)
{
  g_return_if_fail (
    IS_MIDI_NOTE (self) && self->vel);
  arranger_object_free (
    (ArrangerObject *) self->vel);

  if (G_IS_OBJECT (self->layout))
    g_object_unref (self->layout);

  object_zero_and_free (self);
}

/**
 * Frees only this object.
 */
void
arranger_object_free (
  ArrangerObject * self)
{
  g_return_if_fail (IS_ARRANGER_OBJECT (self));

  switch (self->type)
    {
    case TYPE (REGION):
      free_region ((ZRegion *) self);
      return;
    case TYPE (MIDI_NOTE):
      free_midi_note ((MidiNote *) self);
      return;
    case TYPE (MARKER):
      {
        Marker * marker = (Marker *) self;
        g_free (marker->name);
        object_zero_and_free (marker);
      }
      return;
    case TYPE (CHORD_OBJECT):
      {
        ChordObject * co = (ChordObject *) self;
        object_zero_and_free (co);
      }
      return;
    case TYPE (SCALE_OBJECT):
      {
        ScaleObject * scale = (ScaleObject *) self;
        musical_scale_free (scale->scale);
        object_zero_and_free (scale);
      }
      return;
    case TYPE (AUTOMATION_POINT):
      {
        AutomationPoint * ap =
          (AutomationPoint *) self;
        object_zero_and_free (ap);
      }
      return;
    case TYPE (VELOCITY):
      {
        Velocity * vel =
          (Velocity *) self;
        object_zero_and_free (vel);
      }
      return;
    default:
      g_return_if_reached ();
    }
  g_return_if_reached ();
}
