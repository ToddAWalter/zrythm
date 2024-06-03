// SPDX-FileCopyrightText: © 2018-2023 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "actions/tracklist_selections.h"
#include "dsp/audio_region.h"
#include "dsp/channel.h"
#include "dsp/chord_track.h"
#include "dsp/group_target_track.h"
#include "dsp/master_track.h"
#include "dsp/router.h"
#include "dsp/track.h"
#include "dsp/tracklist.h"
#include "gui/backend/event.h"
#include "gui/backend/event_manager.h"
#include "gui/widgets/arranger.h"
#include "gui/widgets/bot_dock_edge.h"
#include "gui/widgets/center_dock.h"
#include "gui/widgets/dialogs/file_import_progress_dialog.h"
#include "gui/widgets/main_window.h"
#include "gui/widgets/mixer.h"
#include "gui/widgets/timeline_arranger.h"
#include "gui/widgets/track.h"
#include "gui/widgets/tracklist.h"
#include "io/file_import.h"
#include "io/midi_file.h"
#include "project.h"
#include "utils/arrays.h"
#include "utils/error.h"
#include "utils/flags.h"
#include "utils/objects.h"
#include "utils/string.h"
#include "zrythm_app.h"

#include <glib/gi18n.h>

typedef enum
{
  Z_AUDIO_TRACKLIST_ERROR_FAILED,
  Z_AUDIO_TRACKLIST_ERROR_NO_DATA,
} ZAudioTracklistError;

#define Z_AUDIO_TRACKLIST_ERROR z_audio_tracklist_error_quark ()
GQuark
z_audio_tracklist_error_quark (void);
G_DEFINE_QUARK (
  z - audio - tracklist - error - quark,
  z_audio_tracklist_error)

/**
 * Initializes the tracklist when loading a project.
 */
void
tracklist_init_loaded (
  Tracklist *       self,
  Project *         project,
  SampleProcessor * sample_processor)
{
  self->project = project;
  self->sample_processor = sample_processor;

  g_message ("initializing loaded Tracklist...");
  for (auto track : TRACKLIST->tracks)
    {
      track_set_magic (track);
    }

  for (auto track : TRACKLIST->tracks)
    {
      if (track->type == TrackType::TRACK_TYPE_CHORD)
        self->chord_track = track;
      else if (track->type == TrackType::TRACK_TYPE_MARKER)
        self->marker_track = track;
      else if (track->type == TrackType::TRACK_TYPE_MASTER)
        self->master_track = track;
      else if (track->type == TrackType::TRACK_TYPE_TEMPO)
        self->tempo_track = track;
      else if (track->type == TrackType::TRACK_TYPE_MODULATOR)
        self->modulator_track = track;

      track_init_loaded (track, self, NULL);
    }
}

void
tracklist_select_all (Tracklist * self, bool select, bool fire_events)
{
  for (size_t i = 0; i < self->tracks.size (); i++)
    {
      Track * track = self->tracks[i];

      track_select (track, select, F_NOT_EXCLUSIVE, fire_events);

      if (!select && i == self->tracks.size () - 1)
        {
          track_select (track, F_SELECT, F_EXCLUSIVE, fire_events);
        }
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
  for (auto track : TRACKLIST->tracks)
    {
      if (track_get_should_be_visible (track))
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
  Tracklist *   self,
  const Track * src,
  const Track * dest)
{
  g_return_val_if_fail (src && dest, 0);

  int count = 0;
  if (src->pos < dest->pos)
    {
      for (int i = src->pos; i < dest->pos; i++)
        {
          Track * track = self->tracks[i];
          if (track_get_should_be_visible (track))
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
          if (track_get_should_be_visible (track))
            {
              count--;
            }
        }
    }

  return count;
}

int
tracklist_contains_master_track (Tracklist * self)
{
  for (auto track : TRACKLIST->tracks)
    {
      if (track->type == TrackType::TRACK_TYPE_MASTER)
        return 1;
    }
  return 0;
}

int
tracklist_contains_chord_track (Tracklist * self)
{
  for (auto track : TRACKLIST->tracks)
    {
      if (track->type == TrackType::TRACK_TYPE_CHORD)
        return 1;
    }
  return 0;
}

void
tracklist_print_tracks (Tracklist * self)
{
  g_message ("----- tracklist tracks ------");
  for (int i = 0; i < self->tracks.size (); i++)
    {
      Track * track = self->tracks[i];
      if (track)
        {
          char parent_str[200];
          strcpy (parent_str, "");
          GPtrArray * parents = g_ptr_array_new ();
          track_add_folder_parents (track, parents, false);
          for (size_t j = 0; j < parents->len; j++)
            {
              strcat (parent_str, "--");
              if (j == parents->len - 1)
                strcat (parent_str, " ");
            }

          g_message (
            "[%03d] %s%s (pos %d, parents %d, "
            "size %d)",
            i, parent_str, track->name, track->pos, parents->len, track->size);
          g_ptr_array_unref (parents);
        }
      else
        {
          g_message ("[%03d] (null)", i);
        }
    }
  g_message ("------ end ------");
}

static void
swap_tracks (Tracklist * self, int src, int dest)
{
  g_return_if_fail (self->tracks.size () > std::max (src, dest));
  self->swapping_tracks = true;

  Track * src_track = self->tracks[src];
  Track * dest_track = self->tracks[dest];
  g_debug (
    "swapping tracks %s [%d] and %s [%d]...",
    src_track ? src_track->name : "(null)", src,
    dest_track ? dest_track->name : "(null)", dest);

  std::iter_swap (self->tracks.begin () + src, self->tracks.begin () + dest);

  if (dest_track)
    dest_track->pos = src;
  if (src_track)
    src_track->pos = dest;

  self->swapping_tracks = false;
  g_debug ("tracks swapped");
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
    "inserting %s at %d (has output %d)...", track->name, pos,
    track->channel && track->channel->has_output);

  /* TODO throw critical if attempted to add a special track
   * (like master) when it already exists */

  /* set to -1 so other logic knows it is a new track */
  track->pos = -1;
  if (track->channel)
    {
      track->channel->track_pos = -1;
    }

  /* this needs to be called before appending the track to the tracklist */
  track_set_name (track, track->name, F_NO_PUBLISH_EVENTS);

  /* append the track at the end */
  self->tracks.push_back (track);
  track->tracklist = self;

  /* add flags for auditioner track ports */
  if (tracklist_is_auditioner (self))
    {
      GPtrArray * ports = g_ptr_array_new ();
      track_append_ports (track, ports, true);
      for (size_t i = 0; i < ports->len; i++)
        {
          Port * port = (Port *) g_ptr_array_index (ports, i);
          port->id_.flags2_ |= PortIdentifier::Flags2::SAMPLE_PROCESSOR_TRACK;
        }
      object_free_w_func_and_null (g_ptr_array_unref, ports);
    }

  /* if inserting it, swap until it reaches its position */
  if (pos != self->tracks.size () - 1)
    {
      for (int i = self->tracks.size () - 1; i > pos; i--)
        {
          swap_tracks (self, i, i - 1);
        }
    }

  track->pos = pos;

  if (
    tracklist_is_in_active_project (self)
    /* auditioner doesn't need automation */
    && !tracklist_is_auditioner (self))
    {
      /* make the track the only selected track */
      tracklist_selections_select_single (
        TRACKLIST_SELECTIONS, track, publish_events);

      /* set automation track on ports */
      AutomationTracklist * atl = track_get_automation_tracklist (track);
      if (atl)
        {
          for (int i = 0; i < atl->num_ats; i++)
            {
              AutomationTrack * at = atl->ats[i];
              Port * port = Port::find_from_identifier (&at->port_id);
              g_return_if_fail (IS_PORT_AND_NONNULL (port));
              port->at_ = at;
            }
        }
    }

  if (track->channel)
    {
      track->channel->connect ();
    }

  /* if audio output route to master */
  if (
    track->out_signal_type == PortType::Audio
    && track->type != TrackType::TRACK_TYPE_MASTER)
    {
      group_target_track_add_child (
        self->master_track, track_get_name_hash (*track), F_CONNECT,
        F_NO_RECALC_GRAPH, F_NO_PUBLISH_EVENTS);
    }

  if (tracklist_is_in_active_project (self))
    track_activate_all_plugins (track, F_ACTIVATE);

  if (!tracklist_is_auditioner (self))
    {
      /* verify */
      track_validate (track);
    }

  if (ZRYTHM_TESTING)
    {
      for (auto cur_track : TRACKLIST->tracks)
        {
          if (track_type_has_channel (cur_track->type))
            {
              Channel * ch = cur_track->channel;
              if (ch->has_output)
                {
                  g_return_if_fail (
                    ch->output_name_hash != track_get_name_hash (*cur_track));
                }
            }
        }
    }

  if (ZRYTHM_HAVE_UI && !tracklist_is_auditioner (self))
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
      EVENTS_PUSH (EventType::ET_TRACK_ADDED, track);
    }

  g_message (
    "%s: done - inserted track '%s' (%u) at %d", __func__, track->name,
    track_get_name_hash (*track), pos);
}

ChordTrack *
tracklist_get_chord_track (const Tracklist * self)
{
  for (auto track : self->tracks)
    {
      if (track->type == TrackType::TRACK_TYPE_CHORD)
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
tracklist_find_track_by_name (Tracklist * self, const char * name)
{
  if (G_UNLIKELY (ROUTER && router_is_processing_thread (ROUTER)))
    {
      g_critical ("attempted to call from DSP thread");
      return NULL;
    }

  for (auto track : TRACKLIST->tracks)
    {
      if (string_is_equal (name, track->name))
        return track;
    }
  return NULL;
}

/**
 * Returns the Track matching the given name, if
 * any.
 */
NONNULL Track *
tracklist_find_track_by_name_hash (Tracklist * self, unsigned int hash)
{
  if (
    G_LIKELY (tracklist_is_in_active_project (self)) && ROUTER
    && router_is_processing_thread (ROUTER) && !tracklist_is_auditioner (self))
    {
      for (auto track : self->tracks)
        {
          if (G_UNLIKELY (track->name_hash == hash))
            return track;
        }
    }
  else
    {
      for (auto track : self->tracks)
        {
          if (ZRYTHM_TESTING)
            {
              g_return_val_if_fail (IS_TRACK_AND_NONNULL (track), NULL);
            }
          if (G_UNLIKELY (track_get_name_hash (*track) == hash))
            return track;
        }
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
    self, track, self->tracks.size (), publish_events, recalc_graph);
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
  for (auto tr : self->tracks)
    {
      if (visible_only && !track_get_should_be_visible (tr))
        continue;

      bool ret =
        track_multiply_heights (tr, multiplier, visible_only, check_only);
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

/**
 * Returns the track at the given index or NULL if
 * the index is invalid.
 */
Track *
tracklist_get_track (Tracklist * self, int idx)
{
  if (idx < 0 || idx >= self->tracks.size ())
    {
      g_warning ("invalid track idx %d", idx);
      return NULL;
    }

  Track * tr = self->tracks[idx];
  g_return_val_if_fail (IS_TRACK_AND_NONNULL (tr), NULL);

  return tr;
}

int
tracklist_get_track_pos (Tracklist * self, Track * track)
{
  auto it = std::find (self->tracks.cbegin (), self->tracks.cend (), track);
  g_return_val_if_fail (it != self->tracks.cend (), -1);
  return std::distance (self->tracks.cbegin (), it);
}

/**
 * Returns the first track found with the given
 * type.
 */
Track *
tracklist_get_track_by_type (Tracklist * self, TrackType type)
{
  for (auto track : self->tracks)
    {
      if (track->type == type)
        return track;
    }
  return NULL;
}

bool
tracklist_validate (Tracklist * self)
{
  /* this validates tracks in parallel */
  std::vector<std::future<bool>> ret_vals;
  for (auto track : self->tracks)
    {
      ret_vals.emplace_back (std::async ([self, track] () {
        g_return_val_if_fail (
          track && track_is_in_active_project (track), false);

        if (!track_validate (track))
          return false;

        /* validate size */
        g_return_val_if_fail (
          track->pos + track->size <= self->tracks.size (), false);

        /* validate connections */
        if (track->channel)
          {
            Channel * ch = track->channel;
            for (int j = 0; j < STRIP_SIZE; j++)
              {
                ChannelSend * send = ch->sends[j];
                channel_send_validate (send);
              }
          }
        return true;
      }));
    }

  bool valid = true;
  for (auto &t : ret_vals)
    {
      bool res = t.get ();
      if (!res)
        valid = false;
    }

  return valid;
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
  for (int i = self->tracks.size () - 1; i >= 0; i--)
    {
      tr = self->tracks[i];

      if (
        pin_opt == TracklistPinOption::TRACKLIST_PIN_OPTION_PINNED_ONLY
        && !track_is_pinned (tr))
        {
          continue;
        }
      if (
        pin_opt == TracklistPinOption::TRACKLIST_PIN_OPTION_UNPINNED_ONLY
        && track_is_pinned (tr))
        {
          continue;
        }
      if (visible_only && !track_get_should_be_visible (tr))
        {
          continue;
        }

      return i;
    }

  /* no track with given options found, select the last */
  return self->tracks.size () - 1;
}

Track *
tracklist_get_last_track (
  Tracklist *              self,
  const TracklistPinOption pin_opt,
  const int                visible_only)
{
  int idx = tracklist_get_last_pos (self, pin_opt, visible_only);
  g_return_val_if_fail (idx >= 0 && idx < self->tracks.size (), NULL);
  Track * tr = self->tracks[idx];

  return tr;
}

/**
 * Returns the Track after delta visible Track's.
 *
 * Negative delta searches backwards.
 *
 * This function searches tracks only in the same Tracklist
 * as the given one (ie, pinned or not).
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
          vis_track = tracklist_get_next_visible_track (self, vis_track);

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
          vis_track = tracklist_get_prev_visible_track (self, vis_track);

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
tracklist_get_first_visible_track (Tracklist * self, const int pinned)
{
  for (auto tr : self->tracks)
    {
      if (track_get_should_be_visible (tr) && track_is_pinned (tr) == pinned)
        {
          return tr;
        }
    }
  g_warn_if_reached ();
  return NULL;
}

/**
 * Returns the previous visible Track.
 */
Track *
tracklist_get_prev_visible_track (Tracklist * self, Track * track)
{
  Track * tr;
  for (int i = tracklist_get_track_pos (self, track) - 1; i >= 0; i--)
    {
      tr = self->tracks[i];
      if (track_get_should_be_visible (tr))
        {
          g_warn_if_fail (tr != track);
          return tr;
        }
    }
  return NULL;
}

/**
 * Returns the next visible Track in the same
 * Tracklist.
 */
Track *
tracklist_get_next_visible_track (Tracklist * self, Track * track)
{
  for (
    int i = tracklist_get_track_pos (self, track) + 1; i < self->tracks.size ();
    i++)
    {
      Track * tr = self->tracks[i];
      if (track_get_should_be_visible (tr))
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
  g_return_if_fail (IS_TRACK (track));
  g_message (
    "%s: removing [%d] %s - remove plugins %d - "
    "free track %d - pub events %d - "
    "recalc graph %d - "
    "num tracks before deletion: %zu",
    __func__, track->pos, track->name, rm_pl, free_track, publish_events,
    recalc_graph, self->tracks.size ());

  Track * prev_visible = NULL;
  Track * next_visible = NULL;
  if (!tracklist_is_auditioner (self))
    {
      prev_visible = tracklist_get_prev_visible_track (self, track);
      next_visible = tracklist_get_next_visible_track (self, track);
    }

  /* remove/deselect all objects */
  track_clear (track);

  int idx = tracklist_get_track_pos (self, track);
  g_return_if_fail (track->pos == idx);

  track_disconnect (track, rm_pl, F_NO_RECALC_GRAPH);

  /* move track to the end */
  int end_pos = self->tracks.size () - 1;
  tracklist_move_track (
    self, track, end_pos, false, F_NO_PUBLISH_EVENTS, F_NO_RECALC_GRAPH);

  if (!tracklist_is_auditioner (self))
    {
      tracklist_selections_remove_track (
        TRACKLIST_SELECTIONS, track, publish_events);
    }

  self->tracks.erase (
    std::remove (self->tracks.begin (), self->tracks.end (), track));

  if (tracklist_is_in_active_project (self) && !tracklist_is_auditioner (self))
    {
      /* if it was the only track selected, select the next one */
      if (TRACKLIST_SELECTIONS->num_tracks == 0)
        {
          Track * track_to_select = NULL;
          if (prev_visible || next_visible)
            {
              track_to_select = next_visible ? next_visible : prev_visible;
            }
          else if (self->tracks.size () > 0)
            {
              track_to_select = self->tracks[0];
            }
          if (track_to_select)
            {
              tracklist_selections_add_track (
                TRACKLIST_SELECTIONS, track_to_select, publish_events);
            }
        }
    }

  track->pos = -1;

  if (free_track)
    {
      track_free (track);
    }

  if (recalc_graph)
    {
      router_recalc_graph (ROUTER, F_NOT_SOFT);
    }

  if (publish_events)
    {
      EVENTS_PUSH (EventType::ET_TRACKS_REMOVED, NULL);
    }

  g_message ("%s: done", __func__);
}

void
tracklist_move_track (
  Tracklist * self,
  Track *     track,
  int         pos,
  bool        always_before_pos,
  bool        publish_events,
  bool        recalc_graph)
{
  g_message ("%s: %s from %d to %d", __func__, track->name, track->pos, pos);

  if (pos == track->pos)
    return;

  bool move_higher = pos < track->pos;

  Track * prev_visible = tracklist_get_prev_visible_track (self, track);
  Track * next_visible = tracklist_get_next_visible_track (self, track);

  int idx = tracklist_get_track_pos (self, track);
  g_return_if_fail (track->pos == idx);

  /* the current implementation currently moves some tracks to tracks.size() + 1
   * temporarily, so we expand the vector here and resize it back at the end */
  bool expanded = false;
  if (pos >= self->tracks.size ())
    {
      self->tracks.resize (pos + 1, nullptr);
      expanded = true;
    }

  if (tracklist_is_in_active_project (self) && !tracklist_is_auditioner (self))
    {
      /* clear the editor region if it exists and belongs to this track */
      Region * region = clip_editor_get_region (CLIP_EDITOR);
      if (
        region && arranger_object_get_track ((ArrangerObject *) region) == track)
        {
          clip_editor_set_region (CLIP_EDITOR, NULL, publish_events);
        }

      /* deselect all objects */
      track_unselect_all (track);

      tracklist_selections_remove_track (
        TRACKLIST_SELECTIONS, track, publish_events);

      /* if it was the only track selected, select
       * the next one */
      if (
        TRACKLIST_SELECTIONS->num_tracks == 0 && (prev_visible || next_visible))
        {
          tracklist_selections_add_track (
            TRACKLIST_SELECTIONS, next_visible ? next_visible : prev_visible,
            publish_events);
        }
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

      if (always_before_pos && pos > 0)
        {
          /* swap with previous track */
          swap_tracks (self, pos, pos - 1);
        }
    }

  if (expanded)
    {
      /* resize back */
      self->tracks.resize (self->tracks.size () - 1);
    }

  if (tracklist_is_in_active_project (self) && !tracklist_is_auditioner (self))
    {
      /* make the track the only selected track */
      tracklist_selections_select_single (
        TRACKLIST_SELECTIONS, track, publish_events);
    }

  if (recalc_graph)
    {
      router_recalc_graph (ROUTER, F_NOT_SOFT);
    }

  if (publish_events)
    {
      EVENTS_PUSH (EventType::ET_TRACKS_MOVED, NULL);
    }

  g_debug ("%s: finished moving track", __func__);
}

bool
tracklist_track_name_is_unique (
  Tracklist *  self,
  const char * name,
  Track *      track_to_skip)
{
  for (auto track : self->tracks)
    {
      if (string_is_equal (name, track->name) && track != track_to_skip)
        return false;
    }
  return true;
}

bool
tracklist_has_soloed (const Tracklist * self)
{
  for (auto track : self->tracks)
    {
      if (track->channel && track_get_soloed (track))
        return true;
    }
  return false;
}

/**
 * Returns if the tracklist has listened tracks.
 */
NONNULL bool
tracklist_has_listened (const Tracklist * self)
{
  for (auto track : self->tracks)
    {
      if (track->channel && track_get_listened (track))
        return true;
    }
  return false;
}

int
tracklist_get_num_muted_tracks (const Tracklist * self)
{
  int count = 0;
  for (auto track : self->tracks)
    {
      if (track_type_has_channel (track->type) && track_get_muted (track))
        {
          count++;
        }
    }

  return count;
}

int
tracklist_get_num_soloed_tracks (const Tracklist * self)
{
  int count = 0;
  for (auto track : self->tracks)
    {
      if (track_type_has_channel (track->type) && track_get_soloed (track))
        {
          count++;
        }
    }

  return count;
}

int
tracklist_get_num_listened_tracks (const Tracklist * self)
{
  int count = 0;
  for (auto track : self->tracks)
    {
      if (track_type_has_channel (track->type) && track_get_listened (track))
        {
          count++;
        }
    }

  return count;
}

int
tracklist_get_plugins (const Tracklist * const self, GPtrArray * arr)
{
  int total = 0;
  for (auto track : self->tracks)
    {
      total += track_get_plugins (track, arr);
    }

  return total;
}

/**
 * Activate or deactivate all plugins.
 *
 * This is useful for exporting: deactivating and
 * reactivating a plugin will reset its state.
 */
void
tracklist_activate_all_plugins (Tracklist * self, bool activate)
{
  for (auto track : self->tracks)
    {
      track_activate_all_plugins (track, activate);
    }
}

/**
 * @param visible 1 for visible, 0 for invisible.
 */
int
tracklist_get_num_visible_tracks (Tracklist * self, int visible)
{
  int ret = 0;
  for (auto track : self->tracks)
    {
      if (track_get_should_be_visible (track) == visible)
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
tracklist_expose_ports_to_backend (Tracklist * self)
{
  g_return_if_fail (self);

  for (auto track : self->tracks)
    {
      g_return_if_fail (track);

      if (track_type_has_channel (track->type))
        {
          Channel * ch = track_get_channel (track);
          g_return_if_fail (IS_CHANNEL_AND_NONNULL (ch));
          ch->expose_ports_to_backend ();
        }
    }
}

bool
tracklist_import_regions (
  GPtrArray *            region_arrays,
  const FileImportInfo * import_info,
  TracksReadyCallback    ready_cb,
  GError **              error)
{
  g_message ("Adding regions into the project...");

  EngineState state;
  engine_wait_for_pause (AUDIO_ENGINE, &state, Z_F_NO_FORCE, true);
  int  executed_actions = 0;
  bool result = true;
  for (size_t j = 0; j < region_arrays->len; j++)
    {
      GPtrArray * regions = (GPtrArray *) g_ptr_array_index (region_arrays, j);
      g_debug ("REGION ARRAY %zu (%u elements)", j, regions->len);
      int i = 0;
      while (regions->len > 0)
        {
          int iter = i++;
          g_debug ("REGION %d", iter);
          Region *  r = (Region *) g_ptr_array_steal_index (regions, 0);
          TrackType track_type = ENUM_INT_TO_VALUE (TrackType, 0);
          bool      gen_name = true;
          if (r->id.type == RegionType::REGION_TYPE_AUDIO)
            {
              track_type = TrackType::TRACK_TYPE_AUDIO;
            }
          else if (r->id.type == RegionType::REGION_TYPE_MIDI)
            {
              track_type = TrackType::TRACK_TYPE_MIDI;
              /* name could already be generated based on the
               * track name (if any) in the MIDI file */
              if (r->name)
                gen_name = false;
            }
          else
            {
              g_warn_if_reached ();
              continue;
            }

          Track * track = NULL;
          if (import_info->track_name_hash)
            {
              track = tracklist_find_track_by_name_hash (
                TRACKLIST, import_info->track_name_hash);
            }
          else
            {
              GError * err = NULL;
              int      index = import_info->track_idx + (int) iter;
              bool     success =
                track_create_empty_at_idx_with_action (track_type, index, &err);
              if (!success)
                {
                  PROPAGATE_PREFIXED_ERROR_LITERAL (
                    error, err, _ ("Failed to create MIDI track"));
                  result = false;
                  goto finish_import_regions;
                }
              track = tracklist_get_track (TRACKLIST, index);
              executed_actions++;
            }
          g_return_val_if_fail (track, false);

          GError * err = NULL;
          bool     success = track_add_region (
            track, r, NULL, 0, gen_name, F_NO_PUBLISH_EVENTS, &err);
          if (!success)
            {
              PROPAGATE_PREFIXED_ERROR (
                error, err, _ ("Failed to add region %d to track"), iter);
              result = false;
              goto finish_import_regions;
            }

          arranger_object_select (
            (ArrangerObject *) r, F_SELECT, F_NO_APPEND, F_PUBLISH_EVENTS);
          success =
            arranger_selections_action_perform_create (TL_SELECTIONS, &err);
          if (!success)
            {
              PROPAGATE_PREFIXED_ERROR (
                error, err, _ ("Failed to create region %d inside track %s"),
                iter, track->name);
              result = false;
              goto finish_import_regions;
            }
          executed_actions++;
        } /* end foreach region */
    }

finish_import_regions:
  if (executed_actions > 0)
    {
      UndoableAction * last_action = undo_manager_get_last_action (UNDO_MANAGER);
      last_action->num_actions = executed_actions;
    }

  engine_resume (AUDIO_ENGINE, &state);

  if (ready_cb)
    {
      ready_cb (import_info, *error);
    }

  return result;
}

/**
 * Handles a file drop inside the timeline or in
 * empty space in the tracklist.
 *
 * @param uri_list URI list, if URI list was dropped.
 * @param file File, if FileDescriptor was dropped.
 * @param track Track, if any.
 * @param lane TrackLane, if any.
 * @param index Index to insert new tracks at, or -1 to insert
 *   at end.
 * @param pos Position the file was dropped at, if
 *   inside track.
 *
 * @return Whether successful.
 */
bool
tracklist_import_files (
  Tracklist *            self,
  char **                uri_list,
  const FileDescriptor * orig_file,
  Track *                track,
  TrackLane *            lane,
  int                    index,
  const Position *       pos,
  TracksReadyCallback    ready_cb,
  GError **              error)
{
  std::vector<FileDescriptor> file_arr;
  if (orig_file)
    {
      file_arr.push_back (*orig_file);
    }
  else
    {
      g_return_val_if_fail (uri_list, NULL);

      char * uri = NULL;
      int    i = 0;
      while ((uri = uri_list[i++]) != NULL)
        {
          /* strip "file://" */
          if (!string_contains_substr (uri, "file://"))
            continue;

          GError * err = NULL;
          auto     file = FileDescriptor::new_from_uri (uri, &err);
          if (!file)
            {
              PROPAGATE_PREFIXED_ERROR_LITERAL (
                error, err, "Failed to create a FileImport instance");
              return false;
            }
          file_arr.push_back (*file);
        }
    }

  if (file_arr.size () == 0)
    {
      g_set_error_literal (
        error, Z_AUDIO_TRACKLIST_ERROR, Z_AUDIO_TRACKLIST_ERROR_FAILED,
        _ ("No file was found"));
      return false;
    }
  else if (track && file_arr.size () > 1)
    {
      g_set_error_literal (
        error, Z_AUDIO_TRACKLIST_ERROR, Z_AUDIO_TRACKLIST_ERROR_FAILED,
        _ ("Can only drop 1 file at a time on existing tracks"));
      return false;
    }

  for (auto &file : file_arr)
    {
      if (file.is_supported () && file.is_audio ())
        {
          if (track && track->type != TrackType::TRACK_TYPE_AUDIO)
            {
              g_set_error_literal (
                error, Z_AUDIO_TRACKLIST_ERROR, Z_AUDIO_TRACKLIST_ERROR_FAILED,
                _ ("Can only drop audio files on audio tracks"));
              return false;
            }
        }
      else if (file.is_midi ())
        {
          if (
            track && track->type != TrackType::TRACK_TYPE_MIDI
            && track->type != TrackType::TRACK_TYPE_INSTRUMENT)
            {
              g_set_error_literal (
                error, Z_AUDIO_TRACKLIST_ERROR, Z_AUDIO_TRACKLIST_ERROR_FAILED,
                _ ("Can only drop MIDI files on MIDI/instrument tracks"));
              return false;
            }
        }
      else
        {
          char * descr = FileDescriptor::get_type_description (file.type);
          g_set_error (
            error, Z_AUDIO_TRACKLIST_ERROR, Z_AUDIO_TRACKLIST_ERROR_FAILED,
            _ ("Unsupported file type %s"), descr);
          g_free (descr);
          return false;
        }
    }

  GStrvBuilder * builder = g_strv_builder_new ();
  for (auto &file : file_arr)
    {
      g_strv_builder_add (builder, file.abs_path.c_str ());
    }
  char **          filepaths = g_strv_builder_end (builder);
  FileImportInfo * nfo = file_import_info_new ();
  nfo->track_name_hash = track ? track->name_hash : 0;
  nfo->lane = lane ? lane->pos : 0;
  if (pos)
    {
      position_set_to_pos (&nfo->pos, pos);
    }
  else
    {
      position_init (&nfo->pos);
    }
  nfo->track_idx =
    track ? track->pos : (index >= 0 ? index : TRACKLIST->tracks.size ());
  if (ZRYTHM_TESTING)
    {
      int          i = 0;
      const char * filepath;
      while ((filepath = filepaths[i++]) != NULL)
        {
          FileImport * fi = file_import_new (filepath, nfo);
          GError *     err = NULL;
          GPtrArray *  regions = file_import_sync (fi, &err);
          if (!regions)
            {
              PROPAGATE_PREFIXED_ERROR_LITERAL (
                error, err, "File import failed");
              return false;
            }
          GPtrArray * region_arrays = g_ptr_array_new ();
          g_ptr_array_add (region_arrays, regions);
          bool success =
            tracklist_import_regions (region_arrays, nfo, ready_cb, &err);
          g_ptr_array_unref (region_arrays);
          if (!success)
            {
              PROPAGATE_PREFIXED_ERROR_LITERAL (
                error, err, "Failed to import regions");
              return false;
            }
        }
    }
  else /* not testing */
    {
      FileImportProgressDialog * dialog = file_import_progress_dialog_new (
        (const char **) filepaths, nfo, ready_cb, UI_ACTIVE_WINDOW_OR_NULL);
      file_import_progress_dialog_run (dialog);
    }
  g_strfreev (filepaths);
  file_import_info_free (nfo);
  return true;
}

static void
move_after_copying_or_moving_inside (
  TracklistSelections * after_tls,
  int                   diff_between_track_below_and_parent)
{
  Track * lowest_cloned_track =
    tracklist_selections_get_lowest_track (after_tls);
  int lowest_cloned_track_pos = lowest_cloned_track->pos;

  GError * err = NULL;
  bool     ret = tracklist_selections_action_perform_move (
    after_tls, PORT_CONNECTIONS_MGR,
    lowest_cloned_track_pos + diff_between_track_below_and_parent, &err);
  object_free_w_func_and_null (tracklist_selections_free, after_tls);
  if (!ret)
    {
      HANDLE_ERROR (
        err, "%s",
        _ ("Failed to move tracks after "
           "copying or moving inside folder"));
      return;
    }
  UndoableAction * ua = undo_manager_get_last_action (UNDO_MANAGER);
  ua->num_actions = 2;
}

/**
 * Handles a move or copy action based on a drag.
 *
 * @param this_track The track at the cursor (where
 *   the selection was dropped to.
 * @param location Location relative to @ref
 *   this_track.
 */
void
tracklist_handle_move_or_copy (
  Tracklist *          self,
  Track *              this_track,
  TrackWidgetHighlight location,
  GdkDragAction        action)
{
  g_debug (
    "%s: this track '%s' - location %s - "
    "action %s",
    __func__, this_track->name, track_widget_highlight_to_str (location),
    action == GDK_ACTION_COPY ? "copy" : "move");

  int pos = -1;
  if (location == TrackWidgetHighlight::TRACK_WIDGET_HIGHLIGHT_TOP)
    {
      pos = this_track->pos;
    }
  else
    {
      Track * next = tracklist_get_next_visible_track (TRACKLIST, this_track);
      if (next)
        pos = next->pos;
      /* else if last track, move to end */
      else if (this_track->pos == TRACKLIST->tracks.size () - 1)
        pos = TRACKLIST->tracks.size ();
      /* else if last visible track but not last track */
      else
        pos = this_track->pos + 1;
    }

  if (pos == -1)
    return;

  tracklist_selections_select_foldable_children (TRACKLIST_SELECTIONS);

  if (action == GDK_ACTION_COPY)
    {
      if (tracklist_selections_contains_uncopyable_track (TRACKLIST_SELECTIONS))
        {
          g_message (
            "cannot copy - track selection "
            "contains uncopyable track");
          return;
        }

      if (location == TrackWidgetHighlight::TRACK_WIDGET_HIGHLIGHT_INSIDE)
        {
          GError * err = NULL;
          bool     ret = tracklist_selections_action_perform_copy_inside (
            TRACKLIST_SELECTIONS, PORT_CONNECTIONS_MGR, this_track->pos, &err);
          if (!ret)
            {
              HANDLE_ERROR (err, "%s", _ ("Failed to copy tracks inside"));
              return;
            }
        }
      /* else if not highlighted inside */
      else
        {
          TracklistSelections * tls = TRACKLIST_SELECTIONS;
          int                   num_tls = tls->num_tracks;
          TracklistSelections * after_tls = NULL;
          int                   diff_between_track_below_and_parent = 0;
          bool                  copied_inside = false;
          if (pos < TRACKLIST->tracks.size ())
            {
              Track * track_below = TRACKLIST->tracks[pos];
              Track * track_below_parent =
                track_get_direct_folder_parent (track_below);
              tracklist_selections_sort (TRACKLIST_SELECTIONS, true);
              Track * cur_parent = TRACKLIST_SELECTIONS->tracks[0];

              if (track_below_parent)
                {
                  diff_between_track_below_and_parent =
                    track_below->pos - track_below_parent->pos;
                }

              /* first copy inside new parent */
              if (track_below_parent && track_below_parent != cur_parent)
                {
                  GError * err = NULL;
                  bool ret = tracklist_selections_action_perform_copy_inside (
                    tls, PORT_CONNECTIONS_MGR, track_below_parent->pos, &err);
                  if (!ret)
                    {
                      HANDLE_ERROR (
                        err, "%s",
                        _ ("Failed to copy track "
                           "inside"));
                      return;
                    }

                  after_tls = tracklist_selections_new (F_NOT_PROJECT);
                  for (int j = 1; j <= num_tls; j++)
                    {
                      err = NULL;
                      Track * clone_tr = track_clone (
                        TRACKLIST->tracks[track_below_parent->pos + j], &err);
                      if (!clone_tr)
                        {
                          HANDLE_ERROR (
                            err, "%s",
                            _ ("Failed to clone "
                               "track"));
                          return;
                        }
                      tracklist_selections_add_track (
                        after_tls, clone_tr, F_NO_PUBLISH_EVENTS);
                    }

                  copied_inside = true;
                }
            }

          /* if not copied inside, copy normally */
          if (!copied_inside)
            {
              GError * err = NULL;
              bool     ret = tracklist_selections_action_perform_copy (
                tls, PORT_CONNECTIONS_MGR, pos, &err);
              if (!ret)
                {
                  HANDLE_ERROR (err, "%s", _ ("Failed to copy tracks"));
                  return;
                }
            }
          /* else if copied inside and there is
           * a track difference, also move */
          else if (diff_between_track_below_and_parent != 0)
            {
              move_after_copying_or_moving_inside (
                after_tls, diff_between_track_below_and_parent);
            }
        }
    }
  else if (action == GDK_ACTION_MOVE)
    {
      if (location == TrackWidgetHighlight::TRACK_WIDGET_HIGHLIGHT_INSIDE)
        {
          if (tracklist_selections_contains_track (
                TRACKLIST_SELECTIONS, this_track))
            {
              if (!ZRYTHM_TESTING)
                {
                  ui_show_error_message (
                    _ ("Error"), _ ("Cannot drag folder into itself"));
                }
              return;
            }
          /* else if selections do not contain the
           * track dragged into */
          else
            {
              GError * err = NULL;
              bool     ret = tracklist_selections_action_perform_move_inside (
                TRACKLIST_SELECTIONS, PORT_CONNECTIONS_MGR, this_track->pos,
                &err);
              if (!ret)
                {
                  HANDLE_ERROR (
                    err, "%s",
                    _ ("Failed to move track "
                       "inside folder"));
                  return;
                }
            }
        }
      /* else if not highlighted inside */
      else
        {
          TracklistSelections * tls = TRACKLIST_SELECTIONS;
          int                   num_tls = tls->num_tracks;
          TracklistSelections * after_tls = NULL;
          int                   diff_between_track_below_and_parent = 0;
          bool                  moved_inside = false;
          if (pos < TRACKLIST->tracks.size ())
            {
              Track * track_below = TRACKLIST->tracks[pos];
              Track * track_below_parent =
                track_get_direct_folder_parent (track_below);
              tracklist_selections_sort (TRACKLIST_SELECTIONS, true);
              Track * cur_parent = TRACKLIST_SELECTIONS->tracks[0];

              if (track_below_parent)
                {
                  diff_between_track_below_and_parent =
                    track_below->pos - track_below_parent->pos;
                }

              /* first move inside new parent */
              if (track_below_parent && track_below_parent != cur_parent)
                {
                  GError * err = NULL;
                  bool ret = tracklist_selections_action_perform_move_inside (
                    tls, PORT_CONNECTIONS_MGR, track_below_parent->pos, &err);
                  if (!ret)
                    {
                      HANDLE_ERROR (
                        err, "%s",
                        _ ("Failed to move track "
                           "inside folder"));
                      return;
                    }

                  after_tls = tracklist_selections_new (F_NOT_PROJECT);
                  for (int j = 1; j <= num_tls; j++)
                    {
                      err = NULL;
                      Track * clone_tr = track_clone (
                        TRACKLIST->tracks[track_below_parent->pos + j], &err);
                      if (!clone_tr)
                        {
                          HANDLE_ERROR (
                            err, "%s",
                            _ ("Failed to clone "
                               "track"));
                          return;
                        }
                      tracklist_selections_add_track (
                        after_tls, clone_tr, F_NO_PUBLISH_EVENTS);
                    }

                  moved_inside = true;
                }
            } /* endif moved to an existing track */

          /* if not moved inside, move normally */
          if (!moved_inside)
            {
              GError * err = NULL;
              bool     ret = tracklist_selections_action_perform_move (
                tls, PORT_CONNECTIONS_MGR, pos, &err);
              if (!ret)
                {
                  HANDLE_ERROR (err, "%s", _ ("Failed to move tracks"));
                  return;
                }
            }
          /* else if moved inside and there is
           * a track difference, also move */
          else if (diff_between_track_below_and_parent != 0)
            {
              move_after_copying_or_moving_inside (
                after_tls, diff_between_track_below_and_parent);
            }
        }
    } /* endif action is MOVE */
}

/**
 * Marks or unmarks all tracks for bounce.
 */
void
tracklist_mark_all_tracks_for_bounce (Tracklist * self, bool bounce)
{
  for (auto track : self->tracks)
    {
      track_mark_for_bounce (
        track, bounce, F_MARK_REGIONS, F_NO_MARK_CHILDREN, F_NO_MARK_PARENTS);
    }
}

void
tracklist_get_total_bars (Tracklist * self, int * total_bars)
{
  for (auto track : self->tracks)
    {
      track_get_total_bars (track, total_bars);
    }
}

/**
 * Set various caches (snapshots, track name hashes, plugin
 * input/output ports, etc).
 */
void
tracklist_set_caches (Tracklist * self, CacheTypes types)
{
  for (auto track : self->tracks)
    {
      track_set_caches (track, types);
    }
}

/**
 * Only clones what is needed for project save.
 *
 * @param src Source tracklist. Must be the
 *   tracklist of the project in use.
 */
Tracklist *
tracklist_clone (Tracklist * src)
{
  Tracklist * self = new Tracklist ();

  self->pinned_tracks_cutoff = src->pinned_tracks_cutoff;

  for (auto track : src->tracks)
    {
      GError * err = NULL;
      Track *  clone = track_clone (track, &err);
      g_return_val_if_fail (clone, NULL);
      self->tracks.push_back (clone);
    }

  return self;
}

Tracklist *
tracklist_new (Project * project, SampleProcessor * sample_processor)
{
  Tracklist * self = new Tracklist ();
  self->project = project;
  self->sample_processor = sample_processor;

  if (project)
    {
      project->tracklist = self;
    }

  return self;
}

void
tracklist_free (Tracklist * self)
{
  g_message ("%s: freeing...", __func__);

  size_t num_tracks = self->tracks.size ();
  for (int i = num_tracks - 1; i >= 0; i--)
    {
      Track * track = self->tracks[i];
      g_return_if_fail (IS_TRACK_AND_NONNULL (track));
      if (track != self->tempo_track)
        tracklist_remove_track (
          self, track, F_REMOVE_PL, F_FREE, F_NO_PUBLISH_EVENTS,
          F_NO_RECALC_GRAPH);
    }

  /* remove tempo track last (used when printing positions) */
  if (self->tempo_track)
    {
      tracklist_remove_track (
        self, self->tempo_track, F_REMOVE_PL, F_FREE, F_NO_PUBLISH_EVENTS,
        F_NO_RECALC_GRAPH);
      self->tempo_track = NULL;
    }

  delete self;

  g_message ("%s: done", __func__);
}
