// SPDX-FileCopyrightText: © 2021 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

/**
 * \file
 *
 * Common logic for foldable tracks.
 */

#ifndef __AUDIO_FOLDABLE_TRACK_H__
#define __AUDIO_FOLDABLE_TRACK_H__

typedef struct Track Track;

enum class FoldableTrackMixerStatus
{
  FOLDABLE_TRACK_MIXER_STATUS_MUTED,
  FOLDABLE_TRACK_MIXER_STATUS_SOLOED,
  FOLDABLE_TRACK_MIXER_STATUS_IMPLIED_SOLOED,
  FOLDABLE_TRACK_MIXER_STATUS_LISTENED,
};

void
foldable_track_init (Track * track);

/**
 * Used to check if soloed/muted/etc.
 */
bool
foldable_track_is_status (Track * self, FoldableTrackMixerStatus status);

/**
 * Returns whether @p child is a folder child of
 * @p self.
 */
bool
foldable_track_is_direct_child (Track * self, Track * child);

/**
 * Returns whether @p child is a folder child of
 * @p self.
 */
bool
foldable_track_is_child (Track * self, Track * child);

/**
 * Adds to the size recursively.
 *
 * This must only be called from the lowest-level
 * foldable track.
 */
void
foldable_track_add_to_size (Track * self, int delta);

#endif /* __AUDIO_FOLDABLE_TRACK_H__ */
