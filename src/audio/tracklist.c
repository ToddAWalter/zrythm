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

#include "actions/tracklist_selections.h"
#include "audio/audio_region.h"
#include "audio/channel.h"
#include "audio/chord_track.h"
#include "audio/midi_file.h"
#include "audio/router.h"
#include "audio/tracklist.h"
#include "audio/track.h"
#include "gui/backend/event.h"
#include "gui/backend/event_manager.h"
#include "gui/widgets/arranger.h"
#include "gui/widgets/bot_dock_edge.h"
#include "gui/widgets/center_dock.h"
#include "gui/widgets/main_window.h"
#include "gui/widgets/mixer.h"
#include "gui/widgets/timeline_arranger.h"
#include "gui/widgets/track.h"
#include "gui/widgets/tracklist.h"
#include "project.h"
#include "utils/arrays.h"
#include "utils/flags.h"
#include "utils/object_utils.h"
#include "utils/objects.h"
#include "utils/string.h"
#include "zrythm_app.h"

#include <glib/gi18n.h>

/**
 * Initializes the tracklist when loading a project.
 */
void
tracklist_init_loaded (
  Tracklist * self)
{
  g_message ("initializing loaded Tracklist...");
  for (int i = 0; i < self->num_tracks; i++)
    {
      Track * track = self->tracks[i];
      track_set_magic (track);
    }

  for (int i = 0; i < self->num_tracks; i++)
    {
      Track * track = self->tracks[i];

      if (track->type == TRACK_TYPE_CHORD)
        self->chord_track = track;
      else if (track->type == TRACK_TYPE_MARKER)
        self->marker_track = track;
      else if (track->type == TRACK_TYPE_MASTER)
        self->master_track = track;
      else if (track->type == TRACK_TYPE_TEMPO)
        self->tempo_track = track;
      else if (track->type == TRACK_TYPE_MODULATOR)
        self->modulator_track = track;

      track_init_loaded (track, true);
    }
}

/**
 * Finds visible tracks and puts them in given array.
 */
void
tracklist_get_visible_tracks (
  Tracklist * self,
  Track **    visible_tracks,
  int *       num_visible)
{
  *num_visible = 0;
  for (int i = 0; i < self->num_tracks; i++)
    {
      Track * track = self->tracks[i];
      if (track->visible)
        {
          visible_tracks[*num_visible++] = track;
        }
    }
}

/**
 * Returns the number of visible Tracks between
 * src and dest (negative if dest is before src).
 */
int
tracklist_get_visible_track_diff (
  Tracklist * self,
  const Track *     src,
  const Track *     dest)
{
  g_return_val_if_fail (
    src && dest, 0);

  int count = 0;
  if (src->pos < dest->pos)
    {
      for (int i = src->pos; i < dest->pos; i++)
        {
          Track * track = self->tracks[i];
          if (track->visible)
            {
              count++;
            }
        }
    }
  else if (src->pos > dest->pos)
    {
      for (int i = dest->pos; i < src->pos; i++)
        {
          Track * track = self->tracks[i];
          if (track->visible)
            {
              count--;
            }
        }
    }

  return count;
}

int
tracklist_contains_master_track (
  Tracklist * self)
{
  for (int i = 0; self->num_tracks; i++)
    {
      Track * track = self->tracks[i];
      if (track->type == TRACK_TYPE_MASTER)
        return 1;
    }
  return 0;
}

int
tracklist_contains_chord_track (
  Tracklist * self)
{
  for (int i = 0; self->num_tracks; i++)
    {
      Track * track = self->tracks[i];
      if (track->type == TRACK_TYPE_CHORD)
        return 1;
    }
  return 0;
}

void
tracklist_print_tracks (
  Tracklist * self)
{
  g_message ("----- tracklist tracks ------");
  Track * track;
  for (int i = 0; i < self->num_tracks; i++)
    {
      track = self->tracks[i];
      g_message ("[idx %d] %s (pos %d)",
                 i, track->name,
                 track->pos);
    }
  g_message ("------ end ------");
}

static void
swap_tracks (
  Tracklist * self,
  int         src,
  int         dest)
{
  self->swapping_tracks = true;

  Track * src_track = self->tracks[src];
  Track * dest_track = self->tracks[dest];

  /* move src somewhere temporarily */
  self->tracks[src] = NULL;
  self->tracks[self->num_tracks] = src_track;
  track_set_pos (src_track, self->num_tracks);

  /* move dest to src */
  self->tracks[src] = dest_track;
  self->tracks[dest] = NULL;
  track_set_pos (dest_track, src);

  /* move src from temp pos to dest */
  self->tracks[dest] = src_track;
  self->tracks[self->num_tracks] = NULL;
  track_set_pos (src_track, dest);

  self->swapping_tracks = false;
}

/**
 * Adds given track to given spot in tracklist.
 *
 * @param publish_events Publish UI events.
 * @param recalc_graph Recalculate routing graph.
 */
void
tracklist_insert_track (
  Tracklist * self,
  Track *     track,
  int         pos,
  int         publish_events,
  int         recalc_graph)
{
  g_message (
    "inserting %s at %d...",
    track->name, pos);

  /* set to -1 so other logic knows it is a new
   * track */
  track->pos = -1;
  if (track->channel)
    {
      track->channel->track_pos = -1;
    }

  track_set_name (track, track->name, 0);

  /* append the track at the end */
  array_append (
    self->tracks, self->num_tracks, track);

  /* if inserting it, swap until it reaches its
   * position */
  if (pos != self->num_tracks - 1)
    {
      for (int i = self->num_tracks - 1; i > pos;
           i--)
        {
          swap_tracks (self, i, i - 1);
        }
    }

  /* make the track the only selected track */
  tracklist_selections_select_single (
    TRACKLIST_SELECTIONS, track,
    publish_events);

  track_set_is_project (track, true);

  /* this is needed again since "set_is_project"
   * made some ports from non-project to project
   * and they weren't considered before */
  track_set_pos (track, pos);

  if (track->channel)
    {
      channel_connect (track->channel);
    }

  /* verify */
  track_validate (track);

  if (ZRYTHM_TESTING)
    {
      for (int i = 0; i < self->num_tracks; i++)
        {
          Track * cur_track = self->tracks[i];
          if (track_type_has_channel (
                cur_track->type))
            {
              Channel * ch = cur_track->channel;
              if (ch->has_output)
                {
                  g_return_if_fail (
                    ch->output_pos != ch->track_pos);
                }
            }
        }
    }

  if (ZRYTHM_HAVE_UI)
    {
      /* generate track widget */
      track->widget = track_widget_new (track);
    }

  if (recalc_graph)
    {
      router_recalc_graph (ROUTER, F_NOT_SOFT);
    }

  if (publish_events)
    {
      EVENTS_PUSH (ET_TRACK_ADDED, track);
    }

  g_message ("%s: done", __func__);
}

ChordTrack *
tracklist_get_chord_track (
  const Tracklist * self)
{
  Track * track;
  for (int i = 0; i < self->num_tracks; i++)
    {
      track = self->tracks[i];
      if (track->type == TRACK_TYPE_CHORD)
        {
          return (ChordTrack *) track;
        }
    }
  g_warn_if_reached ();
  return NULL;
}

/**
 * Returns the Track matching the given name, if
 * any.
 */
Track *
tracklist_find_track_by_name (
  Tracklist *  self,
  const char * name)
{
  Track * track;
  for (int i = 0; i < self->num_tracks; i++)
    {
      track = self->tracks[i];
      if (string_is_equal (name, track->name))
        return track;
    }
  return NULL;
}

void
tracklist_append_track (
  Tracklist * self,
  Track *     track,
  int         publish_events,
  int         recalc_graph)
{
  tracklist_insert_track (
    self, track, self->num_tracks,
    publish_events, recalc_graph);
}

/**
 * Multiplies all tracks' heights and returns if
 * the operation was valid.
 *
 * @param visible_only Only apply to visible tracks.
 */
bool
tracklist_multiply_track_heights (
  Tracklist * self,
  double      multiplier,
  bool        visible_only,
  bool        check_only,
  bool        fire_events)
{
  for (int i = 0; i < self->num_tracks; i++)
    {
      Track * tr = self->tracks[i];

      if (visible_only && !tr->visible)
        continue;

      bool ret =
        track_multiply_heights (
          tr, multiplier, visible_only, check_only);
      /*g_debug ("%s can multiply %d",*/
        /*tr->name, ret);*/

      if (!ret)
        {
          return false;
        }

      if (!check_only && fire_events)
        {
          /* FIXME should be event */
          track_widget_update_size (tr->widget);
        }
    }

  return true;
}

int
tracklist_get_track_pos (
  Tracklist * self,
  Track *     track)
{
  return
    array_index_of (
      (void **) self->tracks,
      self->num_tracks,
      (void *) track);
}

void
tracklist_validate (
  Tracklist * self)
{
  for (int i = 0; i < self->num_tracks; i++)
    {
      Track * track = self->tracks[i];

      g_return_if_fail (
        track && track->is_project);
      track_validate (track);
    }
}

/**
 * Returns the index of the last Track.
 *
 * @param pin_opt Pin option.
 * @param visible_only Only consider visible
 *   Track's.
 */
int
tracklist_get_last_pos (
  Tracklist *              self,
  const TracklistPinOption pin_opt,
  const bool               visible_only)
{
  Track * tr;
  for (int i = self->num_tracks - 1; i >= 0; i--)
    {
      tr = self->tracks[i];

      if (pin_opt ==
            TRACKLIST_PIN_OPTION_PINNED_ONLY &&
          !track_is_pinned (tr))
        {
          continue;
        }
      if (pin_opt ==
            TRACKLIST_PIN_OPTION_UNPINNED_ONLY &&
          track_is_pinned (tr))
        {
          continue;
        }
      if (visible_only && !tr->visible)
        {
          continue;
        }

      return i;
    }

  /* no track with given options found,
   * select the last */
  return self->num_tracks - 1;
}

/**
 * Returns the last Track.
 *
 * @param pin_opt Pin option.
 * @param visible_only Only consider visible
 *   Track's.
 */
Track*
tracklist_get_last_track (
  Tracklist *              self,
  const TracklistPinOption pin_opt,
  const int                visible_only)
{
  int idx =
    tracklist_get_last_pos (
      self, pin_opt, visible_only);
  g_return_val_if_fail (
    idx >= 0 && idx < self->num_tracks, NULL);
  Track * tr =
    self->tracks[idx];

  return tr;
}

/**
 * Returns the Track after delta visible Track's.
 *
 * Negative delta searches backwards.
 *
 * This function searches tracks only in the same
 * Tracklist as the given one (ie, pinned or not).
 */
Track *
tracklist_get_visible_track_after_delta (
  Tracklist * self,
  Track *     track,
  int         delta)
{
  if (delta > 0)
    {
      Track * vis_track = track;
      while (delta > 0)
        {
          vis_track =
            tracklist_get_next_visible_track (
              self, vis_track);

          if (!vis_track)
            return NULL;

          delta--;
        }
      return vis_track;
    }
  else if (delta < 0)
    {
      Track * vis_track = track;
      while (delta < 0)
        {
          vis_track =
            tracklist_get_prev_visible_track (
              self, vis_track);

          if (!vis_track)
            return NULL;

          delta++;
        }
      return vis_track;
    }
  else
    return track;
}

/**
 * Returns the first visible Track.
 *
 * @param pinned 1 to check the pinned tracklist,
 *   0 to check the non-pinned tracklist.
 */
Track *
tracklist_get_first_visible_track (
  Tracklist * self,
  const int   pinned)
{
  Track * tr;
  for (int i = 0; i < self->num_tracks; i++)
    {
      tr = self->tracks[i];
      if (tr->visible &&
          track_is_pinned (tr) == pinned)
        {
          return self->tracks[i];
        }
    }
  g_warn_if_reached ();
  return NULL;
}

/**
 * Returns the previous visible Track.
 */
Track *
tracklist_get_prev_visible_track (
  Tracklist * self,
  Track *     track)
{
  Track * tr;
  for (int i =
       tracklist_get_track_pos (self, track) - 1;
       i >= 0; i--)
    {
      tr = self->tracks[i];
      if (tr->visible)
        {
          g_warn_if_fail (tr != track);
          return tr;
        }
    }
  return NULL;
}

#if 0
/**
 * Pins or unpins the Track.
 */
void
tracklist_set_track_pinned (
  Tracklist * self,
  Track *     track,
  const int   pinned,
  int         publish_events,
  int         recalc_graph)
{
  if (track->pinned == pinned)
    return;

  int last_pinned_pos =
    tracklist_get_last_pos (
      self,
      TRACKLIST_PIN_OPTION_PINNED_ONLY, false);
  if (pinned)
    {
      /* move track to last pinned pos + 1 */
      tracklist_move_track (
        self, track, last_pinned_pos + 1,
        publish_events,
        recalc_graph);
      track->pinned = 1;
      track->pos_before_pinned =
        track->pos;
    }
  else
    {
      /* move track to previous pos */
      g_return_if_fail (
        track->pos_before_pinned >= 0);
      int pos_to_move_to;
      if (track->pos_before_pinned <=
            last_pinned_pos)
        pos_to_move_to = last_pinned_pos + 1;
      else
        pos_to_move_to = track->pos_before_pinned;
      track->pinned = 0;
      tracklist_move_track (
        self, track, pos_to_move_to,
        publish_events, recalc_graph);
      track->pos_before_pinned = -1;
    }
}
#endif

/**
 * Returns the next visible Track in the same
 * Tracklist.
 */
Track *
tracklist_get_next_visible_track (
  Tracklist * self,
  Track *     track)
{
  Track * tr;
  for (int i =
       tracklist_get_track_pos (self, track) + 1;
       i < self->num_tracks; i++)
    {
      tr = self->tracks[i];
      if (tr->visible)
        {
          g_warn_if_fail (tr != track);
          return tr;
        }
    }
  return NULL;
}

/**
 * Removes a track from the Tracklist and the
 * TracklistSelections.
 *
 * Also disconnects the channel.
 *
 * @param rm_pl Remove plugins or not.
 * @param free Free the track or not (free later).
 * @param publish_events Push a track deleted event
 *   to the UI.
 * @param recalc_graph Recalculate the mixer graph.
 */
void
tracklist_remove_track (
  Tracklist * self,
  Track *     track,
  bool        rm_pl,
  bool        free_track,
  bool        publish_events,
  bool        recalc_graph)
{
  g_return_if_fail (self && IS_TRACK (track));
  g_message (
    "%s: removing %s - remove plugins %d - "
    "free track %d - pub events %d - "
    "recalc graph %d", __func__, track->name,
    rm_pl, free_track, publish_events,
    recalc_graph);

  Track * prev_visible =
    tracklist_get_prev_visible_track (
      TRACKLIST, track);
  Track * next_visible =
    tracklist_get_next_visible_track (
      TRACKLIST, track);

  /* remove/deselect all objects */
  track_clear (track);

  int idx =
    array_index_of (
      self->tracks, self->num_tracks, track);
  g_warn_if_fail (track->pos == idx);

  /* move track to the end */
  tracklist_move_track (
    self, track, TRACKLIST->num_tracks - 1,
    F_NO_PUBLISH_EVENTS, F_NO_RECALC_GRAPH);

  track_disconnect (
    track, rm_pl, F_NO_RECALC_GRAPH);

  tracklist_selections_remove_track (
    TRACKLIST_SELECTIONS, track, publish_events);
  array_delete (
    self->tracks, self->num_tracks, track);

  /* if it was the only track selected, select
   * the next one */
  if (TRACKLIST_SELECTIONS->num_tracks == 0)
    {
      Track * track_to_select = NULL;
      if (prev_visible || next_visible)
        {
          track_to_select =
            next_visible ?
              next_visible : prev_visible;
        }
      else
        {
          track_to_select = TRACKLIST->tracks[0];
        }
      tracklist_selections_add_track (
        TRACKLIST_SELECTIONS, track_to_select,
        publish_events);
    }

  track_set_pos (track, -1);

  track_set_is_project (track, false);

  if (free_track)
    {
      object_free_w_func_and_null (
        track_free, track);
    }

  if (recalc_graph)
    {
      router_recalc_graph (ROUTER, F_NOT_SOFT);
    }

  if (publish_events)
    {
      EVENTS_PUSH (ET_TRACKS_REMOVED, NULL);
    }

  g_message ("%s: done", __func__);
}

/**
 * Moves a track from its current position to the
 * position given by \p pos.
 *
 * @param publish_events Push UI update events or
 *   not.
 * @param recalc_graph Recalculate routing graph.
 */
void
tracklist_move_track (
  Tracklist * self,
  Track *     track,
  int         pos,
  int         publish_events,
  int         recalc_graph)
{
  g_message (
    "%s: %s from %d to %d",
    __func__, track->name, track->pos, pos);
  /*int prev_pos = track->pos;*/
  bool move_higher = pos < track->pos;

  Track * prev_visible =
    tracklist_get_prev_visible_track (
      TRACKLIST, track);
  Track * next_visible =
    tracklist_get_next_visible_track (
      TRACKLIST, track);

  /* clear the editor region if it exists and
   * belongs to this track */
  ZRegion * region =
    clip_editor_get_region (CLIP_EDITOR);
  if (region &&
      arranger_object_get_track (
        (ArrangerObject *) region) == track)
    {
      clip_editor_set_region (
        CLIP_EDITOR, NULL, publish_events);
    }

  /* deselect all objects */
  track_unselect_all (track);

  int idx =
    array_index_of (
      self->tracks, self->num_tracks, track);
  g_warn_if_fail (
    track->pos == idx);

  tracklist_selections_remove_track (
    TRACKLIST_SELECTIONS, track, publish_events);

  /* if it was the only track selected, select
   * the next one */
  if (TRACKLIST_SELECTIONS->num_tracks == 0 &&
      (prev_visible || next_visible))
    {
      tracklist_selections_add_track (
        TRACKLIST_SELECTIONS,
        next_visible ? next_visible : prev_visible,
        publish_events);
    }

  if (move_higher)
    {
      /* move all other tracks 1 track further */
      for (int i = track->pos; i > pos; i--)
        {
          swap_tracks (self, i, i - 1);
        }
    }
  else
    {
      /* move all other tracks 1 track earlier */
      for (int i = track->pos; i < pos; i++)
        {
          swap_tracks (self, i, i + 1);
        }
    }

  /* make the track the only selected track */
  tracklist_selections_select_single (
    TRACKLIST_SELECTIONS, track, publish_events);

  if (recalc_graph)
    {
      router_recalc_graph (ROUTER, F_NOT_SOFT);
    }

  if (publish_events)
    {
      EVENTS_PUSH (ET_TRACKS_MOVED, NULL);
    }

  g_message ("%s: finished moving track", __func__);
}

/**
 * Returns 1 if the track name is not taken.
 *
 * @param track_to_skip Track to skip when searching.
 */
int
tracklist_track_name_is_unique (
  Tracklist *  self,
  const char * name,
  Track *      track_to_skip)
{
  for (int i = 0; i < self->num_tracks; i++)
    {
      if (!g_strcmp0 (name, self->tracks[i]->name) &&
          self->tracks[i] != track_to_skip)
        return 0;
    }
  return 1;
}

/**
 * Returns if the tracklist has soloed tracks.
 */
int
tracklist_has_soloed (
  const Tracklist * self)
{
  Track * track;
  for (int i = 0; i < self->num_tracks; i++)
    {
      track = self->tracks[i];

      if (track->channel && track_get_soloed (track))
        return 1;
    }
  return 0;
}

/**
 * Activate or deactivate all plugins.
 *
 * This is useful for exporting: deactivating and
 * reactivating a plugin will reset its state.
 */
void
tracklist_activate_all_plugins (
  Tracklist * self,
  bool        activate)
{
  for (int i = 0; i < self->num_tracks; i++)
    {
      track_activate_all_plugins (
        self->tracks[i], activate);
    }
}

/**
 * @param visible 1 for visible, 0 for invisible.
 */
int
tracklist_get_num_visible_tracks (
  Tracklist * self,
  int         visible)
{
  int ret = 0;
  for (int i = 0; i < self->num_tracks; i++)
    {
      Track * track = self->tracks[i];

      if (track->visible == visible)
        ret++;
    }

  return ret;
}

/**
 * Exposes each track's ports that should be
 * exposed to the backend.
 *
 * This should be called after setting up the
 * engine.
 */
void
tracklist_expose_ports_to_backend (
  Tracklist * self)
{
  g_return_if_fail (self);

  for (int i = 0; i < self->num_tracks; i++)
    {
      Track * track = self->tracks[i];
      g_return_if_fail (track);

      if (track_type_has_channel (track->type))
        {
          Channel * ch = track_get_channel (track);
          channel_expose_ports_to_backend (ch);
        }
    }
}

/**
 * Handles a file drop inside the timeline or in
 * empty space in the tracklist.
 *
 * @param uri_list URI list, if URI list was dropped.
 * @param file File, if SupportedFile was dropped.
 * @param track Track, if any.
 * @param lane TrackLane, if any.
 * @param pos Position the file was dropped at, if
 *   inside track.
 * @param perform_actions Whether to perform
 *   undoable actions in addition to creating the
 *   regions/tracks.
 */
void
tracklist_handle_file_drop (
  Tracklist *     self,
  char **         uri_list,
  SupportedFile * orig_file,
  Track *         track,
  TrackLane *     lane,
  Position *      pos,
  bool            perform_actions)
{
  /* get a local file */
  SupportedFile * file = NULL;
  if (orig_file)
    {
      file = supported_file_clone (orig_file);
    }
  else
    {
      g_return_if_fail (uri_list);

      char * uri, * filepath = NULL;
      int i = 0;
      while ((uri = uri_list[i++]) != NULL)
        {
          /* strip "file://" */
          if (!string_contains_substr (
                uri, "file://"))
            continue;

          if (filepath)
            g_free (filepath);
          GError * err = NULL;
          filepath =
            g_filename_from_uri (
              uri, NULL, &err);
          if (err)
            {
              g_warning (
                "%s", err->message);
            }

          /* only accept 1 file for now */
          break;
        }

      if (filepath)
        {
          file =
            supported_file_new_from_path (
              filepath);
          g_free (filepath);
        }
    }

  if (!file)
    {
      ui_show_error_message (
        MAIN_WINDOW, _("No file was found"));

      return;
    }

  TrackType track_type = 0;
  if (supported_file_type_is_supported (
        file->type) &&
      supported_file_type_is_audio (
        file->type))
    {
      track_type = TRACK_TYPE_AUDIO;
    }
  else if (supported_file_type_is_midi (
             file->type))
    {
      track_type = TRACK_TYPE_MIDI;
    }
  else
    {
      char * descr =
        supported_file_type_get_description (
          file->type);
      char * msg =
        g_strdup_printf (
          _("Unsupported file type %s"),
          descr);
      g_free (descr);
      ui_show_error_message (
        MAIN_WINDOW, msg);

      goto free_file_and_return;
    }

  if (perform_actions)
    {
      /* if current track exists and track type are
       * incompatible, do nothing */
      if (track)
        {
          if (track_type == TRACK_TYPE_MIDI)
            {
              if (track->type != TRACK_TYPE_MIDI &&
                  track->type !=
                    TRACK_TYPE_INSTRUMENT)
                {
                  ui_show_error_message (
                    MAIN_WINDOW,
                    _("Can only drop MIDI files on "
                    "MIDI/instrument tracks"));
                  goto free_file_and_return;
                }

              int num_nonempty_tracks =
                midi_file_get_num_tracks (
                  file->abs_path, true);
              if (num_nonempty_tracks > 1)
                {
                  char msg[600];
                  sprintf (
                    msg,
                    _("This MIDI file contains %d "
                    "tracks. It cannot be dropped "
                    "into an existing track"),
                    num_nonempty_tracks);
                  ui_show_error_message (
                    MAIN_WINDOW, msg);
                  goto free_file_and_return;
                }
            }
          else if (track_type == TRACK_TYPE_AUDIO &&
              track->type != TRACK_TYPE_AUDIO)
            {
              ui_show_error_message (
                MAIN_WINDOW,
                _("Can only drop audio files on "
                "audio tracks"));
              goto free_file_and_return;
            }

          int lane_pos =
            lane ? lane->pos :
            (track->num_lanes == 1 ?
             0 : track->num_lanes - 2);
          int idx_in_lane =
            track->lanes[lane_pos]->num_regions;
          ZRegion * region = NULL;
          switch (track_type)
            {
            case TRACK_TYPE_AUDIO:
              /* create audio region in audio
               * track */
              region =
                audio_region_new (
                  -1, file->abs_path, NULL, -1, NULL,
                  0, pos, track->pos, lane_pos,
                  idx_in_lane);
              break;
            case TRACK_TYPE_MIDI:
              region =
                midi_region_new_from_midi_file (
                  pos, file->abs_path, track->pos,
                  lane_pos, idx_in_lane, 0);
              break;
            default:
              break;
            }

          if (region)
            {
              track_add_region (
                track, region, NULL, lane_pos,
                F_GEN_NAME, F_PUBLISH_EVENTS);
              arranger_object_select (
                (ArrangerObject *) region,
                F_SELECT,
                F_NO_APPEND, F_NO_PUBLISH_EVENTS);
              UndoableAction * ua =
                arranger_selections_action_new_create (
                  TL_SELECTIONS);
              undo_manager_perform (
                UNDO_MANAGER, ua);
            }
          else
            {
              g_warn_if_reached ();
            }

          goto free_file_and_return;
        }
      else
        {
          UndoableAction * ua =
            tracklist_selections_action_new_create (
              track_type, NULL, file,
              TRACKLIST->num_tracks,
              pos, 1);
          undo_manager_perform (UNDO_MANAGER, ua);
        }
    }
  else
    {
      g_warning ("operation not supported yet");
    }

free_file_and_return:
  supported_file_free (file);

  return;
}

/**
 * Marks or unmarks all tracks for bounce.
 */
void
tracklist_mark_all_tracks_for_bounce (
  Tracklist * self,
  bool        bounce)
{
  for (int i = 0; i < self->num_tracks; i++)
    {
      Track * track = self->tracks[i];
      track_mark_for_bounce (
        track, bounce, true, false);
    }
}

Tracklist *
tracklist_new (Project * project)
{
  Tracklist * self = object_new (Tracklist);

  if (project)
    {
      project->tracklist = self;
    }

  return self;
}

void
tracklist_free (
  Tracklist * self)
{
  g_message ("%s: freeing...", __func__);

  int num_tracks = self->num_tracks;
  for (int i = num_tracks - 1; i >= 0; i--)
    {
      Track * track = self->tracks[i];
      tracklist_remove_track (
        self, track, F_REMOVE_PL, F_FREE,
        F_NO_PUBLISH_EVENTS, F_NO_RECALC_GRAPH);
    }

  object_zero_and_free (self);

  g_message ("%s: done", __func__);
}
