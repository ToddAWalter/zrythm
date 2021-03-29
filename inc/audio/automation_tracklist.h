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
 * Automation tracklist containing automation
 * points and curves.
 */

#ifndef __AUDIO_AUTOMATION_TRACKLIST_H__
#define __AUDIO_AUTOMATION_TRACKLIST_H__

#include "audio/automation_track.h"
#include "utils/yaml.h"

typedef struct AutomationTrack AutomationTrack;
typedef struct Track Track;
typedef struct Automatable Automatable;
typedef struct AutomationLane AutomationLane;

/**
 * @addtogroup audio
 *
 * @{
 */

#define AUTOMATION_TRACKLIST_SCHEMA_VERSION 1

/**
 * Each track has an automation tracklist with automation
 * tracks to be generated at runtime, and filled in with
 * automation points/curves when loading projects.
 */
typedef struct AutomationTracklist
{
  int               schema_version;

  /**
   * Automation tracks in this automation
   * tracklist.
   *
   * These should be updated with ALL of the
   * automatables available in the channel and its
   * plugins every time there is an update.
   *
   * Active automation lanes that are
   * shown in the UI, including hidden ones, can
   * be found using \ref AutomationTrack.created
   * and \ref AutomationTrack.visible.
   *
   * Automation tracks become active automation
   * lanes when they have automation or are
   * selected.
   */
  AutomationTrack ** ats;
  int               num_ats;

  /**
   * Allocated size for the automation track
   * pointer array.
   */
  size_t            ats_size;

  /**
   * Pointer back to the track.
   *
   * This should be set during initialization.
   */
  int               track_pos;
} AutomationTracklist;

static const cyaml_schema_field_t
  automation_tracklist_fields_schema[] =
{
  YAML_FIELD_INT (
    AutomationTracklist, schema_version),
  YAML_FIELD_DYN_ARRAY_VAR_COUNT (
    AutomationTracklist, ats,
    automation_track_schema),
  YAML_FIELD_INT (
    AutomationTracklist, track_pos),

  CYAML_FIELD_END
};

static const cyaml_schema_value_t
  automation_tracklist_schema =
{
  YAML_VALUE_PTR (
    AutomationTracklist,
    automation_tracklist_fields_schema),
};

void
automation_tracklist_init (
  AutomationTracklist * self,
  Track *               track);

/**
 * Inits a loaded AutomationTracklist.
 */
void
automation_tracklist_init_loaded (
  AutomationTracklist * self);

Track *
automation_tracklist_get_track (
  AutomationTracklist * self);

void
automation_tracklist_add_at (
  AutomationTracklist * self,
  AutomationTrack *     at);

/**
 * Prints info about all the automation tracks.
 *
 * Used for debugging.
 */
void
automation_tracklist_print_ats (
  AutomationTracklist * self);

/**
 * Updates the frames of each position in each child
 * of the automation tracklist recursively.
 */
void
automation_tracklist_update_frames (
  AutomationTracklist * self);

/**
 * Gets the currently visible AutomationTrack's
 * (regardless of whether the automation tracklist
 * is visible in the UI or not.
 */
void
automation_tracklist_get_visible_tracks (
  AutomationTracklist * self,
  AutomationTrack **    visible_tracks,
  int *                 num_visible);

/**
 * Updates the Track position of the Automatable's
 * and AutomationTrack's.
 *
 * @param track The Track to update to.
 */
void
automation_tracklist_update_track_pos (
  AutomationTracklist * self,
  Track *               track);

/**
 * Updates the automation tracks. (adds missing)
 *
 * Builds an automation track for each automatable in the channel and its plugins,
 * unless it already exists.
 */
//void
//automation_tracklist_update (
  //AutomationTracklist * self);

/**
 * Removes the AutomationTrack from the
 * AutomationTracklist, optionally freeing it.
 */
void
automation_tracklist_remove_at (
  AutomationTracklist * self,
  AutomationTrack *     at,
  bool                  free,
  bool                  fire_events);

/**
 * Removes the AutomationTrack's associated with
 * this channel from the AutomationTracklist in the
 * corresponding Track.
 */
void
automation_tracklist_remove_channel_ats (
  AutomationTracklist * self,
  Channel *             ch);

/**
 * Clones the automation tracklist elements from
 * src to dest.
 */
void
automation_tracklist_clone (
  AutomationTracklist * src,
  AutomationTracklist * dest);

/**
 * Returns the AutomationTrack corresponding to the
 * given Port.
 */
AutomationTrack *
automation_tracklist_get_at_from_port (
  AutomationTracklist * self,
  Port *                port);

/**
 * Unselects all arranger objects.
 */
void
automation_tracklist_unselect_all (
  AutomationTracklist * self);

/**
 * Removes all objects recursively.
 */
void
automation_tracklist_clear (
  AutomationTracklist * self);

/**
 * Sets the index of the AutomationTrack and swaps
 * it with the AutomationTrack at that index or
 * pushes the other AutomationTrack's down.
 *
 * A special case is when \ref index == \ref
 * AutomationTracklist.num_ats. In this case, the
 * given automation track is set last and all the
 * other automation tracks are pushed upwards.
 *
 * @param push_down False to swap positions with the
 *   current AutomationTrack, or true to push down
 *   all the tracks below.
 */
void
automation_tracklist_set_at_index (
  AutomationTracklist * self,
  AutomationTrack *     at,
  int                   index,
  bool                  push_down);

/**
 * Gets the automation track with the given label.
 *
 * Only works for plugin port labels and mainly
 * used in tests.
 */
AutomationTrack *
automation_tracklist_get_plugin_at (
  AutomationTracklist * self,
  PluginSlotType        slot_type,
  const int             plugin_slot,
  const char *          label);

AutomationTrack *
automation_tracklist_get_first_invisible_at (
  AutomationTracklist * self);

/**
 * Returns the number of visible AutomationTrack's.
 */
int
automation_tracklist_get_num_visible (
  AutomationTracklist * self);

/**
 * Verifies the identifiers on a live automation
 * tracklist (in the project, not a clone).
 *
 * @return True if pass.
 */
bool
automation_tracklist_validate (
  AutomationTracklist * self);

void
automation_tracklist_free_members (
  AutomationTracklist * self);

/**
 * @}
 */

#endif
