// SPDX-FileCopyrightText: © 2019-2023 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

/**
 * \file
 *
 * Mixer selections.
 */

#ifndef __GUI_BACKEND_MIXER_SELECTIONS_H__
#define __GUI_BACKEND_MIXER_SELECTIONS_H__

#include "dsp/automation_point.h"
#include "dsp/channel.h"
#include "dsp/midi_region.h"
#include "dsp/region.h"

typedef struct Plugin Plugin;

/**
 * @addtogroup gui_backend
 *
 * @{
 */

#define MIXER_SELECTIONS_SCHEMA_VERSION 1

#define MIXER_SELECTIONS (PROJECT->mixer_selections)

#define MIXER_SELECTIONS_MAX_SLOTS 60

/**
 * Selections to be used for the timeline's current
 * selections, copying, undoing, etc.
 */
typedef struct MixerSelections
{
  int schema_version;

  ZPluginSlotType type;

  /** Slots selected. */
  int slots[MIXER_SELECTIONS_MAX_SLOTS];

  /** Cache, used in actions. */
  Plugin * plugins[MIXER_SELECTIONS_MAX_SLOTS];

  int num_slots;

  /** Channel selected. */
  unsigned int track_name_hash;

  /** Whether any slot is selected. */
  int has_any;
} MixerSelections;

void
mixer_selections_init_loaded (MixerSelections * ms, bool is_project);

MixerSelections *
mixer_selections_new (void);

void
mixer_selections_init (MixerSelections * self);

/**
 * Clone the struct for copying, undoing, etc.
 *
 * @bool src_is_project Whether \ref src are the
 *   project selections.
 */
MixerSelections *
mixer_selections_clone (const MixerSelections * src, bool src_is_project);

/**
 * Returns if there are any selections.
 */
int
mixer_selections_has_any (MixerSelections * ms);

/**
 * Gets highest slot in the selections.
 */
int
mixer_selections_get_highest_slot (MixerSelections * ms);

/**
 * Gets lowest slot in the selections.
 */
int
mixer_selections_get_lowest_slot (MixerSelections * ms);

void
mixer_selections_post_deserialize (MixerSelections * self);

/**
 * Returns whether the selections can be pasted to
 * MixerWidget.paste_slot.
 */
NONNULL bool
mixer_selections_can_be_pasted (
  MixerSelections * self,
  Channel *         ch,
  ZPluginSlotType   type,
  int               slot);

/**
 * Paste the selections starting at the slot in the
 * given channel.
 */
NONNULL void
mixer_selections_paste_to_slot (
  MixerSelections * ms,
  Channel *         ch,
  ZPluginSlotType   type,
  int               slot);

/**
 * Get current Track.
 */
Track *
mixer_selections_get_track (const MixerSelections * const self);

/**
 * Returns if the slot is selected or not.
 */
bool
mixer_selections_contains_slot (
  MixerSelections * ms,
  ZPluginSlotType   type,
  int               slot);

/**
 * Returns if the plugin is selected or not.
 */
bool
mixer_selections_contains_plugin (MixerSelections * ms, Plugin * pl);

bool
mixer_selections_contains_uninstantiated_plugin (
  const MixerSelections * const self);

/**
 * Adds a slot to the selections.
 *
 * The selections can only be from one channel.
 *
 * @param track The track.
 * @param slot The slot to add to the selections.
 * @param clone_pl Whether to clone the plugin
 *   when storing it in \ref
 *   MixerSelections.plugins. Used in some actions.
 */
void
mixer_selections_add_slot (
  MixerSelections * ms,
  Track *           track,
  ZPluginSlotType   type,
  int               slot,
  bool              clone_pl,
  const bool        fire_events);

/**
 * Removes a slot from the selections.
 *
 * Assumes that the channel is the one already
 * selected.
 */
NONNULL void
mixer_selections_remove_slot (
  MixerSelections * ms,
  int               slot,
  ZPluginSlotType   type,
  bool              publish_events);

/**
 * Sorts the selections by slot index.
 *
 * @param asc Ascending or not.
 */
NONNULL void
mixer_selections_sort (MixerSelections * self, bool asc);

/**
 * Returns the first selected plugin if any is
 * selected, otherwise NULL.
 */
NONNULL Plugin *
mixer_selections_get_first_plugin (MixerSelections * self);

/**
 * Fills in the array with the plugins in the selections.
 *
 * @memberof MixerSelections
 */
int
mixer_selections_get_plugins (
  const MixerSelections * const self,
  GPtrArray *                   arr,
  bool                          from_cache);

NONNULL bool
mixer_selections_validate (MixerSelections * self);

/**
 * Clears selections.
 */
NONNULL void
mixer_selections_clear (MixerSelections * ms, const int pub_events);

NONNULL void
mixer_selections_free (MixerSelections * self);

/**
 * @}
 */

#endif
