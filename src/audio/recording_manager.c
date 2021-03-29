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

#include "actions/arranger_selections.h"
#include "audio/audio_region.h"
#include "audio/automation_region.h"
#include "audio/clip.h"
#include "audio/control_port.h"
#include "audio/engine.h"
#include "audio/recording_event.h"
#include "audio/recording_manager.h"
#include "audio/track.h"
#include "audio/transport.h"
#include "gui/backend/arranger_object.h"
#include "project.h"
#include "utils/arrays.h"
#include "utils/dsp.h"
#include "utils/flags.h"
#include "utils/math.h"
#include "utils/mpmc_queue.h"
#include "utils/object_pool.h"
#include "utils/objects.h"
#include "zrythm.h"

#include <gtk/gtk.h>

#if 0
static int received = 0;
static int returned = 0;

#define UP_RECEIVED(x) \
  received++; \
  /*g_message ("RECEIVED"); \*/
  /*recording_event_print (x)*/

#define UP_RETURNED(x) \
  returned++; \
  /*g_message ("RETURNED"); \*/
  /*recording_event_print (x)*/
#endif

/**
 * Adds the region's identifier to the recorded
 * identifiers (to be used for creating the undoable
 * action when recording stops.
 */
static void
add_recorded_id (
  RecordingManager * self,
  ZRegion *          region)
{
  /*region_identifier_print (&region->id);*/
  region_identifier_copy (
    &self->recorded_ids[self->num_recorded_ids],
    &region->id);
  self->num_recorded_ids++;
}

static void
free_temp_selections (
  RecordingManager * self)
{
  if (self->selections_before_start)
    {
      object_free_w_func_and_null (
        arranger_selections_free,
        self->selections_before_start);
    }
}

static void
handle_stop_recording (
  RecordingManager * self,
  bool               is_automation)
{
  g_return_if_fail (
    self->num_active_recordings > 0);

  /* skip if still recording */
  if (self->num_active_recordings > 1)
    {
      self->num_active_recordings--;
      return;
    }

  g_message (
    "%s%s", "----- stopped recording",
    is_automation ? " (automation)" : "");

  /* cache the current selections */
  ArrangerSelections * prev_selections =
    arranger_selections_clone (
      (ArrangerSelections *) TL_SELECTIONS);

  /* select all the recorded regions */
  arranger_selections_clear (
    (ArrangerSelections *) TL_SELECTIONS,
    F_NO_FREE, F_PUBLISH_EVENTS);
  for (int i = 0; i < self->num_recorded_ids; i++)
    {
      RegionIdentifier * id = &self->recorded_ids[i];

      if ((is_automation &&
           id->type != REGION_TYPE_AUTOMATION) ||
         (!is_automation &&
           id->type == REGION_TYPE_AUTOMATION))
        continue;

      /* do some sanity checks for lane regions */
      if (region_type_has_lane (id->type))
        {
          g_return_if_fail (
            id->track_pos < TRACKLIST->num_tracks);
          Track * track =
            TRACKLIST->tracks[id->track_pos];
          g_return_if_fail (track);

          g_return_if_fail (
            id->lane_pos < track->num_lanes);
          TrackLane * lane =
            track->lanes[id->lane_pos];
          g_return_if_fail (lane);

          g_return_if_fail (
            id->idx <= lane->num_regions);
        }

      /*region_identifier_print (id);*/
      ZRegion * region = region_find (id);
      g_return_if_fail (region);
      arranger_selections_add_object (
        (ArrangerSelections *) TL_SELECTIONS,
        (ArrangerObject *) region);

      if (is_automation)
        {
          region->last_recorded_ap = NULL;
        }
    }

  /* perform the create action */
  UndoableAction * action =
    arranger_selections_action_new_record (
      self->selections_before_start,
      (ArrangerSelections *) TL_SELECTIONS, true);
  undo_manager_perform (UNDO_MANAGER, action);

  /* update frame caches and write audio clips to
   * pool */
  for (int i = 0; i < self->num_recorded_ids; i++)
    {
      ZRegion * r =
        region_find (&self->recorded_ids[i]);
      if (r->id.type == REGION_TYPE_AUDIO)
        {
          AudioClip * clip =
            audio_region_get_clip (r);
          audio_clip_write_to_pool (clip, true);
        }
    }

  /* restore the selections */
  arranger_selections_clear (
    (ArrangerSelections *) TL_SELECTIONS,
    F_NO_FREE, F_PUBLISH_EVENTS);
  int num_objs;
  ArrangerObject ** objs =
    arranger_selections_get_all_objects (
      prev_selections, &num_objs);
  for (int i = 0; i < num_objs; i++)
    {
      ArrangerObject * obj =
        arranger_object_find (objs[i]);
      g_return_if_fail (obj);
      arranger_object_select (
        obj, F_SELECT, F_APPEND,
        F_NO_PUBLISH_EVENTS);
    }

  /* free the temporary selections */
  free_temp_selections (self);

  /* disarm transport record button */
  transport_set_recording (
    TRANSPORT, false, F_PUBLISH_EVENTS);

  self->num_active_recordings--;
  self->num_recorded_ids = 0;
  g_warn_if_fail (self->num_active_recordings == 0);
}

/**
 * Handles the recording logic inside the process
 * cycle.
 *
 * The MidiEvents are already dequeued at this
 * point.
 *
 * @param g_frames_start Global start frames.
 * @param nframes Number of frames to process. If
 *   this is zero, a pause will be added. See \ref
 *   RECORDING_EVENT_TYPE_PAUSE_TRACK_RECORDING and
 *   RECORDING_EVENT_TYPE_PAUSE_AUTOMATION_RECORDING.
 */
void
recording_manager_handle_recording (
  RecordingManager * self,
  TrackProcessor *   track_processor,
  const long         g_start_frames,
  const nframes_t    local_offset,
  const nframes_t    nframes)
{
#if 0
  g_message (
    "handling recording from %ld (%" PRIu32
    " frames)",
    g_start_frames + local_offset, nframes);
#endif

  Track * tr =
    track_processor_get_track (track_processor);
  AutomationTracklist * atl =
    track_get_automation_tracklist (tr);
  gint64 cur_time = g_get_monotonic_time ();

  /* whether to skip adding track recording
   * events */
  bool skip_adding_track_events = false;

  /* whether to skip adding automation recording
   * events */
  bool skip_adding_automation_events = false;

  /* whether we are inside the punch range in
   * punch mode or true if otherwise */
  bool inside_punch_range = false;

  g_return_if_fail (
    local_offset + nframes <=
      AUDIO_ENGINE->block_length);

  if (TRANSPORT->punch_mode)
    {
      Position tmp;
      position_from_frames (&tmp, g_start_frames);
      inside_punch_range =
        transport_position_is_inside_punch_range (
          TRANSPORT, &tmp);
    }
  else
    {
      inside_punch_range = true;
    }

  /* ---- handle start/stop/pause recording events
   * ---- */

  /* if not recording at all (recording stopped) */
  if (!TRANSPORT->recording ||
      !tr->recording ||
      !TRANSPORT_IS_ROLLING)
    {
      /* if track had previously recorded */
      if (track_type_can_record (tr->type) &&
          tr->recording_region &&
          !tr->recording_stop_sent)
        {
          tr->recording_stop_sent = true;

          /* send stop recording event */
          RecordingEvent * re =
            (RecordingEvent *)
            object_pool_get (
              self->event_obj_pool);
          recording_event_init (re);
          re->type =
            RECORDING_EVENT_TYPE_STOP_TRACK_RECORDING;
          re->g_start_frames = g_start_frames;
          re->local_offset = local_offset;
          re->nframes = nframes;
          strcpy (re->track_name, tr->name);
          /*UP_RECEIVED (re);*/
          recording_event_queue_push_back_event (
            self->event_queue, re);
        }
      skip_adding_track_events = true;
    }
  /* if pausing */
  else if (nframes == 0)
    {
      if (track_type_can_record (tr->type) &&
            (tr->recording_region ||
             tr->recording_start_sent))
        {
          /* send pause event */
          RecordingEvent * re =
            (RecordingEvent *)
            object_pool_get (
              self->event_obj_pool);
          recording_event_init (re);
          re->type =
            RECORDING_EVENT_TYPE_PAUSE_TRACK_RECORDING;
          re->g_start_frames = g_start_frames;
          re->local_offset = local_offset;
          re->nframes = nframes;
          strcpy (re->track_name, tr->name);
          /*UP_RECEIVED (re);*/
          recording_event_queue_push_back_event (
            self->event_queue, re);

          skip_adding_track_events = true;
        }
    }
  /* if recording and inside punch range */
  else if (inside_punch_range)
    {
      /* if no recording started yet */
      if (track_type_can_record (tr->type) &&
          !tr->recording_region &&
          !tr->recording_start_sent)
        {
          tr->recording_start_sent = true;

          /* send start recording event */
          RecordingEvent * re =
            (RecordingEvent *)
            object_pool_get (
              self->event_obj_pool);
          recording_event_init (re);
          re->type =
            RECORDING_EVENT_TYPE_START_TRACK_RECORDING;
          re->g_start_frames = g_start_frames;
          re->local_offset = local_offset;
          re->nframes = nframes;
          strcpy (re->track_name, tr->name);
          /*UP_RECEIVED (re);*/
          recording_event_queue_push_back_event (
            self->event_queue, re);
        }
    }
  else if (!inside_punch_range)
    {
      skip_adding_track_events = true;
    }

  for (int i = 0; i < atl->num_ats; i++)
    {
      AutomationTrack * at = atl->ats[i];

      /* if should stop automation recording */
      if ((!TRANSPORT_IS_ROLLING ||
           !automation_track_should_be_recording (
             at, cur_time, false)) &&
           at->recording_started)
        {
          /* send stop automation recording event */
          RecordingEvent * re =
            (RecordingEvent *)
            object_pool_get (
              self->event_obj_pool);
          recording_event_init (re);
          re->type =
            RECORDING_EVENT_TYPE_STOP_AUTOMATION_RECORDING;
          re->g_start_frames = g_start_frames;
          re->local_offset = local_offset;
          re->nframes = nframes;
          port_identifier_copy (
            &re->port_id, &at->port_id);
          strcpy (re->track_name, tr->name);
          /*UP_RECEIVED (re);*/
          recording_event_queue_push_back_event (
            self->event_queue, re);

          skip_adding_automation_events = true;
        }
      /* if pausing (only at loop end) */
      else if (at->recording_start_sent &&
               nframes == 0 &&
               (long)
               (g_start_frames + local_offset) ==
                 TRANSPORT->loop_end_pos.frames)
        {
          /* send pause event */
          RecordingEvent * re =
            (RecordingEvent *)
            object_pool_get (
              self->event_obj_pool);
          recording_event_init (re);
          re->type =
            RECORDING_EVENT_TYPE_PAUSE_AUTOMATION_RECORDING;
          re->g_start_frames = g_start_frames;
          re->local_offset = local_offset;
          re->nframes = nframes;
          port_identifier_copy (
            &re->port_id, &at->port_id);
          strcpy (re->track_name, tr->name);
          /*UP_RECEIVED (re);*/
          recording_event_queue_push_back_event (
            self->event_queue, re);

          skip_adding_automation_events = true;
        }

      /* if automatmion should be recording */
      if (TRANSPORT_IS_ROLLING &&
          automation_track_should_be_recording (
            at, cur_time, false))
        {
          /* if recording hasn't started yet */
          if (!at->recording_started &&
              !at->recording_start_sent)
            {
              at->recording_start_sent = true;

              /* send start recording event */
              RecordingEvent * re =
                (RecordingEvent *)
                object_pool_get (
                  self->event_obj_pool);
              recording_event_init (re);
              re->type =
                RECORDING_EVENT_TYPE_START_AUTOMATION_RECORDING;
              re->g_start_frames = g_start_frames;
              re->local_offset = local_offset;
              re->nframes = nframes;
              port_identifier_copy (
                &re->port_id, &at->port_id);
              strcpy (re->track_name, tr->name);
              /*UP_RECEIVED (re);*/
              recording_event_queue_push_back_event (
                self->event_queue, re);
            }
        }
    }

  /* ---- end handling start/stop/pause recording
   * events ---- */

  if (skip_adding_track_events)
    {
      goto add_automation_events;
    }

  /* add recorded track material to event queue */

  if (track_has_piano_roll (tr))
    {
      MidiEvents * midi_events =
        track_processor->midi_in->midi_events;

      for (int i = 0;
           i < midi_events->num_events; i++)
        {
          MidiEvent * me =
            &midi_events->events[i];

          RecordingEvent * re =
            (RecordingEvent *)
            object_pool_get (
              self->event_obj_pool);
          recording_event_init (re);
          re->type = RECORDING_EVENT_TYPE_MIDI;
          re->g_start_frames = g_start_frames;
          re->local_offset = local_offset;
          re->nframes = nframes;
          re->has_midi_event = 1;
          midi_event_copy (&re->midi_event, me);
          strcpy (re->track_name, tr->name);
          /*UP_RECEIVED (re);*/
          recording_event_queue_push_back_event (
            self->event_queue, re);
        }

      if (midi_events->num_events == 0)
        {
          RecordingEvent * re =
            (RecordingEvent *)
            object_pool_get (
              self->event_obj_pool);
          recording_event_init (re);
          re->type = RECORDING_EVENT_TYPE_MIDI;
          re->g_start_frames = g_start_frames;
          re->local_offset = local_offset;
          re->nframes = nframes;
          re->has_midi_event = 0;
          strcpy (re->track_name, tr->name);
          /*UP_RECEIVED (re);*/
          recording_event_queue_push_back_event (
            self->event_queue, re);
        }
    }
  else if (tr->type == TRACK_TYPE_AUDIO)
    {
      RecordingEvent * re =
        (RecordingEvent *)
        object_pool_get (self->event_obj_pool);
      recording_event_init (re);
      re->type = RECORDING_EVENT_TYPE_AUDIO;
      re->g_start_frames = g_start_frames;
      re->local_offset = local_offset;
      re->nframes = nframes;
      dsp_copy (
        &re->lbuf[local_offset],
        &track_processor->stereo_out->l->buf[
          local_offset],
        nframes);
      dsp_copy (
        &re->rbuf[local_offset],
        &track_processor->stereo_out->r->buf[
          local_offset],
        nframes);
      strcpy (re->track_name, tr->name);
      /*UP_RECEIVED (re);*/
      recording_event_queue_push_back_event (
        self->event_queue, re);
    }

add_automation_events:

  if (skip_adding_automation_events)
    return;

  /* add automation events */
  for (int i = 0; i < atl->num_ats; i++)
    {
      AutomationTrack * at = atl->ats[i];

      /* if automation should be recording */
      if (TRANSPORT_IS_ROLLING &&
          at->recording_start_sent &&
          automation_track_should_be_recording (
            at, cur_time, false))
        {
          /* send recording event */
          RecordingEvent * re =
            (RecordingEvent *)
            object_pool_get (
              self->event_obj_pool);
          recording_event_init (re);
          re->type =
            RECORDING_EVENT_TYPE_AUTOMATION;
          re->g_start_frames = g_start_frames;
          re->local_offset = local_offset;
          re->nframes = nframes;
          port_identifier_copy (
            &re->port_id, &at->port_id);
          strcpy (re->track_name, tr->name);
          /*UP_RECEIVED (re);*/
          recording_event_queue_push_back_event (
            self->event_queue, re);
        }
    }
}

/**
 * Delete automation points since the last recorded
 * automation point and the current position (eg,
 * when in latch mode) if the position is after
 * the last recorded ap.
 */
static void
delete_automation_points (
  AutomationTrack * at,
  ZRegion *         region,
  Position *        pos)
{
  AutomationPoint * aps[100];
  int               num_aps = 0;
  automation_region_get_aps_since_last_recorded (
    region, pos, aps, &num_aps);
  for (int i = 0; i < num_aps; i++)
    {
      AutomationPoint * ap = aps[i];
      automation_region_remove_ap (
        region, ap, false, true);
    }

  /* create a new automation point at the pos with
   * the previous value */
  if (region->last_recorded_ap)
    {
      /* remove the last recorded AP if its
       * previous AP also has the same value */
      AutomationPoint * ap_before_recorded =
        automation_region_get_prev_ap (
          region, region->last_recorded_ap);
      float prev_fvalue =
        region->last_recorded_ap->fvalue;
      float prev_normalized_val =
        region->last_recorded_ap->normalized_val;
      if (ap_before_recorded &&
          math_floats_equal (
            ap_before_recorded->fvalue,
            region->last_recorded_ap->fvalue))
        {
          automation_region_remove_ap (
            region, region->last_recorded_ap,
            false, true);
        }

      ArrangerObject * r_obj =
        (ArrangerObject *) region;
      Position adj_pos;
      position_set_to_pos (&adj_pos, pos);
      position_add_ticks (
        &adj_pos, - r_obj->pos.ticks);
      AutomationPoint * ap =
        automation_point_new_float (
          prev_fvalue,
          prev_normalized_val,
          &adj_pos);
      automation_region_add_ap (
        region, ap, true);
      region->last_recorded_ap = ap;
    }
}

/**
 * Creates a new automation point and deletes
 * anything between the last recorded automation
 * point and this point.
 */
static AutomationPoint *
create_automation_point (
  AutomationTrack * at,
  ZRegion *         region,
  float             val,
  float             normalized_val,
  Position *        pos)
{
  AutomationPoint * aps[100];
  int               num_aps = 0;
  automation_region_get_aps_since_last_recorded (
    region, pos, aps, &num_aps);
  for (int i = 0; i < num_aps; i++)
    {
      AutomationPoint * ap = aps[i];
      automation_region_remove_ap (
        region, ap, false, true);
    }

  ArrangerObject * r_obj =
    (ArrangerObject *) region;
  Position adj_pos;
  position_set_to_pos (&adj_pos, pos);
  position_add_ticks (
    &adj_pos, - r_obj->pos.ticks);
  AutomationPoint * ap =
    automation_point_new_float (
      val, normalized_val, &adj_pos);
  automation_region_add_ap (
    region, ap, true);
  region->last_recorded_ap = ap;

  return ap;
}

/**
 * This is called when recording is paused (eg,
 * when playhead is not in recordable area).
 */
static void
handle_pause_event (
  RecordingManager * self,
  RecordingEvent * ev)
{
  Track * tr = track_get_from_name (ev->track_name);

  /* pausition to pause at */
  Position pause_pos;
  position_from_frames (
    &pause_pos, ev->g_start_frames);

  if (ev->type ==
        RECORDING_EVENT_TYPE_PAUSE_TRACK_RECORDING)
    {
      tr->recording_paused = true;

      /* get the recording region */
      ZRegion * region = tr->recording_region;
      g_return_if_fail (region);

      /* remember lane index */
      tr->last_lane_idx = region->id.lane_pos;

      if (tr->in_signal_type == TYPE_EVENT)
        {
          /* add midi note offs at the end */
          MidiNote * mn;
          while (
            (mn =
              midi_region_pop_unended_note (
                region, -1)))
            {
              ArrangerObject * mn_obj =
                (ArrangerObject *) mn;
              arranger_object_end_pos_setter (
                mn_obj, &pause_pos);
            }
        }
    }
  else if (ev->type ==
             RECORDING_EVENT_TYPE_PAUSE_AUTOMATION_RECORDING)
    {
      AutomationTrack * at =
        automation_track_find_from_port_id (
          &ev->port_id, false);
      at->recording_paused = true;
    }
}

/**
 * Handles cases where recording events are first
 * received after pausing recording.
 *
 * This should be called on every
 * \ref RECORDING_EVENT_TYPE_MIDI,
 * \ref RECORDING_EVENT_TYPE_AUDIO and
 * \ref RECORDING_EVENT_TYPE_AUTOMATION event
 * and it will handle resume logic automatically
 * if needed.
 *
 * Adds new regions if necessary, etc.
 *
 * @return Whether pause was handled.
 */
static bool
handle_resume_event (
  RecordingManager * self,
  RecordingEvent * ev)
{
  Track * tr = track_get_from_name (ev->track_name);
  gint64 cur_time = g_get_monotonic_time ();

  /* position to resume from */
  Position resume_pos;
  position_from_frames (
    &resume_pos,
    ev->g_start_frames + ev->local_offset);

  /* position 1 frame afterwards */
  Position end_pos;
  position_from_frames (
    &end_pos,
    ev->g_start_frames + ev->local_offset + 1);

  if (ev->type == RECORDING_EVENT_TYPE_MIDI ||
      ev->type == RECORDING_EVENT_TYPE_AUDIO)
    {
      /* not paused, nothing to do */
      if (!tr->recording_paused)
        {
          return false;
        }

      tr->recording_paused = false;

      if (TRANSPORT->recording_mode ==
            RECORDING_MODE_TAKES ||
          TRANSPORT->recording_mode ==
            RECORDING_MODE_TAKES_MUTED ||
          ev->type == RECORDING_EVENT_TYPE_AUDIO)
        {
          /* mute the previous region */
          if ((TRANSPORT->recording_mode ==
                RECORDING_MODE_TAKES_MUTED ||
               (TRANSPORT->recording_mode ==
                  RECORDING_MODE_OVERWRITE_EVENTS &&
                ev->type ==
                  RECORDING_EVENT_TYPE_AUDIO)) &&
              tr->recording_region)
            {
              arranger_object_set_muted (
                (ArrangerObject *)
                tr->recording_region,
                F_MUTE, F_PUBLISH_EVENTS);
            }

          /* start new region in new lane */
          int new_lane_pos =
            tr->last_lane_idx + 1;
          int idx_inside_lane =
            tr->num_lanes > new_lane_pos ?
              tr->lanes[new_lane_pos]->
                num_regions : 0;
          ZRegion * new_region = NULL;
          if (tr->in_signal_type == TYPE_EVENT)
            {
              new_region =
                midi_region_new (
                  &resume_pos, &end_pos, tr->pos,
                  new_lane_pos, idx_inside_lane);
            }
          else if (tr->in_signal_type == TYPE_AUDIO)
            {
              char * name =
                audio_pool_gen_name_for_recording_clip (
                  AUDIO_POOL, tr, new_lane_pos);
              new_region =
                audio_region_new (
                  -1, NULL, NULL, 1, name, 2,
                  &resume_pos,
                  tr->pos, new_lane_pos,
                  idx_inside_lane);
            }
          g_return_val_if_fail (new_region, false);
          track_add_region (
            tr, new_region, NULL, new_lane_pos,
            F_GEN_NAME, F_PUBLISH_EVENTS);

          /* remember region */
          add_recorded_id (
            self, new_region);
          tr->recording_region = new_region;
        }
      /* if MIDI and overwriting or merging
       * events */
      else if (tr->recording_region)
        {
          /* extend the previous region */
          ArrangerObject * r_obj =
            (ArrangerObject *) tr->recording_region;
          if (position_is_before (
                &resume_pos, &r_obj->pos))
            {
              double ticks_delta =
                r_obj->pos.ticks -
                  resume_pos.ticks;
              arranger_object_set_start_pos_full_size (
                r_obj, &resume_pos);
              arranger_object_add_ticks_to_children (
                r_obj, ticks_delta);
            }
          if (position_is_after (
                &end_pos, &r_obj->end_pos))
            {
              arranger_object_set_end_pos_full_size (
                r_obj, &end_pos);
            }
        }
    }
  else if (ev->type ==
             RECORDING_EVENT_TYPE_AUTOMATION)
    {
      AutomationTrack * at =
        automation_track_find_from_port_id (
          &ev->port_id, false);
      g_return_val_if_fail (at, false);

      /* not paused, nothing to do */
      if (!at->recording_paused)
        return false;

      Port * port =
        automation_track_get_port (at);
      float value =
        port_get_control_value (port, false);
      float normalized_value =
        port_get_control_value (port, true);

      /* get or start new region at resume pos */
      ZRegion * new_region =
        automation_track_get_region_before_pos (
          at, &resume_pos, true);
      if (!new_region &&
          automation_track_should_be_recording (
            at, cur_time, false))
        {
          /* create region */
          new_region =
            automation_region_new (
              &resume_pos, &end_pos, tr->pos,
              at->index, at->num_regions);
          g_return_val_if_fail (new_region, false);
          track_add_region (
            tr, new_region, at, -1,
            F_GEN_NAME, F_PUBLISH_EVENTS);
        }
      add_recorded_id (
        self, new_region);

      if (automation_track_should_be_recording (
            at, cur_time, true))
        {
          while (new_region->num_aps > 0 &&
                 position_is_equal (
                   &((ArrangerObject *)
                      new_region->aps[0])->pos,
                   &resume_pos))
            {
              automation_region_remove_ap (
                new_region, new_region->aps[0],
                false, true);
            }

          /* create/replace ap at loop start */
          create_automation_point (
            at, new_region, value, normalized_value,
            &resume_pos);
        }
    }

  return true;
}

static void
handle_audio_event (
  RecordingManager * self,
  RecordingEvent * ev)
{
  bool handled_resume =
    handle_resume_event (self, ev);
  g_debug ("handled resume %d", handled_resume);

  long g_start_frames = ev->g_start_frames;
  nframes_t nframes = ev->nframes;
  nframes_t local_offset = ev->local_offset;
  Track * tr = track_get_from_name (ev->track_name);

  /* get end position */
  long start_frames =
    g_start_frames + ev->local_offset;
  long end_frames =
    start_frames + (long) nframes;

  Position start_pos, end_pos;
  position_from_frames (
    &start_pos, start_frames);
  position_from_frames (
    &end_pos, end_frames);

  /* get the recording region */
  ZRegion * region = tr->recording_region;
  g_return_if_fail (region);
  ArrangerObject * r_obj =
    (ArrangerObject *) region;

  /* the clip */
  AudioClip * clip = NULL;

  clip = audio_region_get_clip (region);

  /* set region end pos */
  arranger_object_set_end_pos_full_size (
    r_obj, &end_pos);
  /*r_obj->end_pos.frames = end_pos.frames;*/

  clip->num_frames =
    r_obj->end_pos.frames - r_obj->pos.frames;
  g_return_if_fail (clip->num_frames >= 0);
  clip->frames =
    (sample_t *)
    realloc (
      clip->frames,
      (size_t)
      (clip->num_frames *
         (long) clip->channels) *
      sizeof (sample_t));
#if 0
  region->frames =
    (sample_t *) realloc (
      region->frames,
      (size_t)
      (clip->num_frames *
         (long) clip->channels) *
      sizeof (sample_t));
  region->num_frames = (size_t) clip->num_frames;
  dsp_copy (
    &region->frames[0], &clip->frames[0],
    (size_t) clip->num_frames * clip->channels);
#endif

  position_from_frames (
    &r_obj->loop_end_pos,
    r_obj->end_pos.frames - r_obj->pos.frames);

  r_obj->fade_out_pos = r_obj->loop_end_pos;

  /* handle the samples normally */
  nframes_t cur_local_offset = 0;
  for (long i = start_frames - r_obj->pos.frames;
       i < end_frames - r_obj->pos.frames;
       i++)
    {
      g_return_if_fail (
        i >= 0 && i < clip->num_frames);
      g_warn_if_fail (
        cur_local_offset >= local_offset &&
        cur_local_offset < local_offset + nframes);

      /* set clip frames */
      clip->frames[i * clip->channels] =
        ev->lbuf[cur_local_offset];
      clip->frames[i * clip->channels + 1] =
        ev->rbuf[cur_local_offset];
#if 0
      region->frames[i * clip->channels] =
        ev->lbuf[cur_local_offset];
      region->frames[i * clip->channels + 1] =
        ev->rbuf[cur_local_offset];
#endif

      cur_local_offset++;
    }

  audio_clip_update_channel_caches (
    clip, (size_t) clip->frames_written);

  /* write to pool if 2 seconds passed since last
   * write */
  gint64 cur_time = g_get_monotonic_time ();
  gint64 nano_sec_to_wait = 2 * 1000 * 1000;
  if (ZRYTHM_TESTING)
    {
      nano_sec_to_wait = 20 * 1000;
    }
  if ((cur_time - clip->last_write) >
        nano_sec_to_wait)
    {
      audio_clip_write_to_pool (clip, true);
    }

#if 0
  g_message (
    "%s wrote from %ld to %ld", __func__,
    start_frames, end_frames);
#endif
}

static void
handle_midi_event (
  RecordingManager * self,
  RecordingEvent * ev)
{
  handle_resume_event (self, ev);

  long g_start_frames = ev->g_start_frames;
  nframes_t nframes = ev->nframes;
  Track * tr = track_get_from_name (ev->track_name);

  g_return_if_fail (tr->recording_region);

  /* get end position */
  long start_frames =
    g_start_frames + ev->local_offset;
  long end_frames =
    start_frames + (long) nframes;

  Position start_pos, end_pos;
  position_from_frames (
    &start_pos, start_frames);
  position_from_frames (
    &end_pos, end_frames);

  /* get the recording region */
  ZRegion * region = tr->recording_region;
  ArrangerObject * r_obj =
    (ArrangerObject *) region;

  /* set region end pos */
  bool set_end_pos = false;
  switch (TRANSPORT->recording_mode)
    {
    case RECORDING_MODE_OVERWRITE_EVENTS:
    case RECORDING_MODE_MERGE_EVENTS:
      if (position_is_before (
            &r_obj->end_pos, &end_pos))
        {
          set_end_pos = true;
        }
      break;
    case RECORDING_MODE_TAKES:
    case RECORDING_MODE_TAKES_MUTED:
      set_end_pos = true;
      break;
    }
  if (set_end_pos)
    {
      arranger_object_set_end_pos_full_size (
        r_obj, &end_pos);
    }

  tr->recording_region = region;

  /* get local positions */
  Position local_pos, local_end_pos;
  position_set_to_pos (
    &local_pos, &start_pos);
  position_set_to_pos (
    &local_end_pos, &end_pos);
  position_add_ticks (
    &local_pos, - r_obj->pos.ticks);
  position_add_ticks (
    &local_end_pos, - r_obj->pos.ticks);

  /* if overwrite mode, clear any notes inside the
   * range */
  if (TRANSPORT->recording_mode ==
        RECORDING_MODE_OVERWRITE_EVENTS)
    {
      for (int i = region->num_midi_notes - 1;
           i >= 0; i--)
        {
          MidiNote * mn = region->midi_notes[i];
          ArrangerObject * mn_obj =
            (ArrangerObject *) mn;

          if (position_is_between_excl_start (
                &mn_obj->pos, &local_pos,
                &local_end_pos) ||
              position_is_between_excl_start (
                &mn_obj->end_pos, &local_pos,
                &local_end_pos) ||
              (position_is_before (
                 &mn_obj->pos, &local_pos) &&
               position_is_after_or_equal (
                 &mn_obj->end_pos, &local_end_pos)))
            {
              midi_region_remove_midi_note (
                region, mn, F_FREE,
                F_NO_PUBLISH_EVENTS);
            }
        }
    }

  if (!ev->has_midi_event)
    return;

  /* convert MIDI data to midi notes */
  MidiNote * mn;
  ArrangerObject * mn_obj;
  MidiEvent * mev = &ev->midi_event;
  switch (mev->type)
    {
      case MIDI_EVENT_TYPE_NOTE_ON:
        g_return_if_fail (region);
        midi_region_start_unended_note (
          region, &local_pos, &local_end_pos,
          mev->note_pitch, mev->velocity, 1);
        break;
      case MIDI_EVENT_TYPE_NOTE_OFF:
        g_return_if_fail (region);
        mn =
          midi_region_pop_unended_note (
            region, mev->note_pitch);
        if (mn)
          {
            mn_obj =
              (ArrangerObject *) mn;
            arranger_object_end_pos_setter (
              mn_obj, &local_end_pos);
          }
        break;
      default:
        /* TODO */
        break;
    }
}

static void
handle_automation_event (
  RecordingManager * self,
  RecordingEvent * ev)
{
  handle_resume_event (self, ev);

  long g_start_frames = ev->g_start_frames;
  nframes_t nframes = ev->nframes;
  /*nframes_t local_offset = ev->local_offset;*/
  Track * tr = track_get_from_name (ev->track_name);
  AutomationTrack * at =
    automation_track_find_from_port_id (
      &ev->port_id, false);
  Port * port =
    automation_track_get_port (at);
  float value =
    port_get_control_value (port, false);
  float normalized_value =
    port_get_control_value (port, true);
  if (ZRYTHM_TESTING)
    {
      math_assert_nonnann (value);
      math_assert_nonnann (normalized_value);
    }
  bool automation_value_changed =
    !port->value_changed_from_reading &&
    !math_floats_equal (
      value, at->last_recorded_value);
  gint64 cur_time = g_get_monotonic_time ();

  /* get end position */
  long start_frames =
    g_start_frames + ev->local_offset;
  long end_frames =
    start_frames + (long) nframes;

  Position start_pos, end_pos;
  position_from_frames (
    &start_pos, start_frames);
  position_from_frames (
    &end_pos, end_frames);

  bool new_region_created = false;

  /* get the recording region */
  ZRegion * region =
    automation_track_get_region_before_pos (
      at, &start_pos, true);
#if 0
  position_print (&start_pos);
  position_print (&end_pos);
  if (region)
    {
      arranger_object_print (
        (ArrangerObject *) region);
    }
  else
    {
      g_message ("no region");
    }
#endif

  ZRegion * region_at_end =
    automation_track_get_region_before_pos (
      at, &end_pos, true);
  if (!region && automation_value_changed)
    {
      /* create region */
      Position pos_to_end_new_r;
      if (region_at_end)
        {
          ArrangerObject * r_at_end_obj =
            (ArrangerObject *) region_at_end;
          position_set_to_pos (
            &pos_to_end_new_r, &r_at_end_obj->pos);
        }
      else
        {
          position_set_to_pos (
            &pos_to_end_new_r, &end_pos);
        }
      region =
        automation_region_new (
          &start_pos, &pos_to_end_new_r, tr->pos,
          at->index, at->num_regions);
      new_region_created = true;
      g_return_if_fail (region);
      track_add_region (
        tr, region, at, -1,
        F_GEN_NAME, F_PUBLISH_EVENTS);

      add_recorded_id (
        self, region);
    }

  at->recording_region = region;
  ArrangerObject * r_obj =
    (ArrangerObject *) region;

  if (new_region_created ||
      (r_obj &&
       position_is_before (
         &r_obj->end_pos, &end_pos)))
    {
      /* set region end pos */
      arranger_object_set_end_pos_full_size (
        r_obj, &end_pos);
    }

  at->recording_region = region;

  /* handle the samples normally */
  if (automation_value_changed)
    {
      create_automation_point (
        at, region,
        value, normalized_value,
        &start_pos);
      at->last_recorded_value = value;
    }
  else if (at->record_mode ==
             AUTOMATION_RECORD_MODE_LATCH)
    {
      g_return_if_fail (region);
      delete_automation_points (
        at, region, &start_pos);
    }

  /* if we left touch mode, set last recorded ap
   * to NULL */
  if (at->record_mode ==
        AUTOMATION_RECORD_MODE_TOUCH &&
      !automation_track_should_be_recording (
        at, cur_time, true) &&
      at->recording_region)
    {
      at->recording_region->last_recorded_ap = NULL;
    }
}

static void
handle_start_recording (
  RecordingManager * self,
  RecordingEvent *   ev,
  bool               is_automation)
{
  Track * tr = track_get_from_name (ev->track_name);
  gint64 cur_time = g_get_monotonic_time ();
  AutomationTrack * at = NULL;
  if (is_automation)
    {
      at =
        automation_track_find_from_port_id (
          &ev->port_id, false);
    }

  if (self->num_active_recordings == 0)
    {
      self->selections_before_start =
          arranger_selections_clone (
            (ArrangerSelections *) TL_SELECTIONS);
    }

  /* this could be called multiple times, ignore
   * if already processed */
  if (tr->recording_region && !is_automation)
    {
      g_warning ("record start already processed");
      self->num_active_recordings++;
      return;
    }

  /* get end position */
  long start_frames =
    ev->g_start_frames + ev->local_offset;
  long end_frames =
    start_frames + (long) ev->nframes;

  g_message (
    "start %ld, end %ld", start_frames, end_frames);

  /* this is not needed because the cycle is
   * already split */
#if 0
  /* adjust for transport loop end */
  if (transport_is_loop_point_met (
        TRANSPORT, start_frames, ev->nframes))
    {
      start_frames =
        TRANSPORT->loop_start_pos.frames;
      end_frames =
        (end_frames -
           TRANSPORT->loop_end_pos.frames) +
        start_frames;
    }
#endif
  g_return_if_fail (start_frames < end_frames);

  Position start_pos, end_pos;
  position_from_frames (
    &start_pos, start_frames);
  position_from_frames (
    &end_pos, end_frames);

  if (is_automation)
    {
      /* don't unset recording paused, this will
       * be unset by handle_resume() */
      /*at->recording_paused = false;*/

      /* nothing, wait for event to start
       * writing data */
      Port * port =
        automation_track_get_port (at);
      float value =
        port_get_control_value (port, false);

      if (automation_track_should_be_recording (
            at, cur_time, true))
        {
          /* set recorded value to something else
           * to force the recorder to start
           * writing */
          g_message ("SHOULD BE RECORDING");
          at->last_recorded_value = value + 2.f;
        }
      else
        {
          g_message ("SHOULD NOT BE RECORDING");
          /** set the current value so that
           * nothing is recorded until it changes */
          at->last_recorded_value = value;
        }
    }
  else
    {
      tr->recording_paused = false;

      if (track_has_piano_roll (tr))
        {
          /* create region */
          int new_lane_pos = tr->num_lanes - 1;
          ZRegion * region =
            midi_region_new (
              &start_pos, &end_pos, tr->pos,
              new_lane_pos,
              tr->lanes[new_lane_pos]->
                num_regions);
          g_return_if_fail (region);
          track_add_region (
            tr, region, NULL, new_lane_pos,
            F_GEN_NAME, F_PUBLISH_EVENTS);

          tr->recording_region = region;
          add_recorded_id (
            self, region);
        }
      else if (tr->type == TRACK_TYPE_AUDIO)
        {
          /* create region */
          int new_lane_pos = tr->num_lanes - 1;
          char * name =
            audio_pool_gen_name_for_recording_clip (
              AUDIO_POOL, tr, new_lane_pos);
          ZRegion * region =
            audio_region_new (
              -1, NULL, NULL, ev->nframes, name, 2,
              &start_pos, tr->pos, new_lane_pos,
              tr->lanes[new_lane_pos]->num_regions);
          g_return_if_fail (region);
          track_add_region (
            tr, region, NULL, new_lane_pos,
            F_GEN_NAME, F_PUBLISH_EVENTS);

          tr->recording_region = region;
          add_recorded_id (
            self, region);

#if 0
          g_message (
            "region start %ld, end %ld",
            region->base.pos.frames,
            region->base.end_pos.frames);
#endif
        }
    }

  self->num_active_recordings++;
}

/**
 * GSourceFunc to be added using idle add.
 *
 * This will loop indefinintely.
 *
 * It can also be called to process the events
 * immediately. Should only be called from the
 * GTK thread.
 */
int
recording_manager_process_events (
  RecordingManager * self)
{
  /*gint64 curr_time = g_get_monotonic_time ();*/
  /*g_message ("starting processing");*/
  RecordingEvent * ev;
  while (recording_event_queue_dequeue_event (
           self->event_queue, &ev))
    {
      if (ev->type < 0)
        {
          g_warn_if_reached ();
          continue;
        }

      /*g_message ("event type %d", ev->type);*/

      switch (ev->type)
        {
        case RECORDING_EVENT_TYPE_MIDI:
          /*g_message ("-------- RECORD MIDI");*/
          handle_midi_event (self, ev);
          break;
        case RECORDING_EVENT_TYPE_AUDIO:
          /*g_message ("-------- RECORD AUDIO");*/
          handle_audio_event (self, ev);
          break;
        case RECORDING_EVENT_TYPE_AUTOMATION:
          /*g_message ("-------- RECORD AUTOMATION");*/
          handle_automation_event (self, ev);
          break;
        case RECORDING_EVENT_TYPE_PAUSE_TRACK_RECORDING:
          g_message ("-------- PAUSE TRACK RECORDING");
          handle_pause_event (self, ev);
          break;
        case RECORDING_EVENT_TYPE_PAUSE_AUTOMATION_RECORDING:
          g_message ("-------- PAUSE AUTOMATION RECORDING");
          handle_pause_event (self, ev);
          break;
        case RECORDING_EVENT_TYPE_STOP_TRACK_RECORDING:
          g_message (
            "-------- STOP TRACK RECORDING (%s)",
            ev->track_name);
          {
            Track * track =
              track_get_from_name (ev->track_name);
            g_return_val_if_fail (
              track, G_SOURCE_REMOVE);
            handle_stop_recording (self, false);
            track->recording_region = NULL;
            track->recording_start_sent = false;
            track->recording_stop_sent = false;
          }
          g_message (
            "num active recordings: %d",
            self->num_active_recordings);
          break;
        case RECORDING_EVENT_TYPE_STOP_AUTOMATION_RECORDING:
          g_message ("-------- STOP AUTOMATION RECORDING");
          {
            AutomationTrack * at =
              automation_track_find_from_port_id (
                &ev->port_id, false);
            g_return_val_if_fail (
              at, G_SOURCE_REMOVE);
            if (at->recording_started)
              {
                handle_stop_recording (self, true);
              }
            at->recording_started = false;
            at->recording_start_sent = false;
            at->recording_region = NULL;
          }
          g_message (
            "num active recordings: %d",
            self->num_active_recordings);
          break;
        case RECORDING_EVENT_TYPE_START_TRACK_RECORDING:
          g_message (
            "-------- START TRACK RECORDING (%s)",
            ev->track_name);
          handle_start_recording (self, ev, false);
          g_message (
            "num active recordings: %d",
            self->num_active_recordings);
          break;
        case RECORDING_EVENT_TYPE_START_AUTOMATION_RECORDING:
          g_message (
            "-------- START AUTOMATION RECORDING");
          {
            AutomationTrack * at =
              automation_track_find_from_port_id (
                &ev->port_id, false);
            g_return_val_if_fail (
              at, G_SOURCE_REMOVE);
            if (!at->recording_started)
              {
                handle_start_recording (
                  self, ev, true);
              }
            at->recording_started = true;
          }
          g_message (
            "num active recordings: %d",
            self->num_active_recordings);
          break;
        default:
          g_warning (
            "recording event %d not implemented yet",
            ev->type);
          break;
        }

      /*UP_RETURNED (ev);*/
      object_pool_return (
        self->event_obj_pool, ev);
    }
  /*g_message ("processed %d events", i);*/

  return G_SOURCE_CONTINUE;
}

/**
 * Creates the event queue and starts the event loop.
 *
 * Must be called from a GTK thread.
 */
RecordingManager *
recording_manager_new (void)
{
  RecordingManager * self =
    object_new (RecordingManager);

  const size_t max_events = 10000;
  self->event_obj_pool =
    object_pool_new (
      (ObjectCreatorFunc) recording_event_new,
      (ObjectFreeFunc) recording_event_free,
      (int) max_events);
  self->event_queue = mpmc_queue_new ();
  mpmc_queue_reserve (
    self->event_queue, max_events);

  self->source_id =
    g_timeout_add (
      12,
      (GSourceFunc)
      recording_manager_process_events, self);

  return self;
}

void
recording_manager_free (
  RecordingManager * self)
{
  g_message ("%s: Freeing...", __func__);

  /* stop source func */
  g_source_remove_and_zero (self->source_id);

  /* process pending events */
  recording_manager_process_events (self);

  /* free objects */
  object_free_w_func_and_null (
    mpmc_queue_free, self->event_queue);
  object_free_w_func_and_null (
    object_pool_free, self->event_obj_pool);

  free_temp_selections (self);

  object_zero_and_free (self);

  g_message ("%s: done", __func__);
}
