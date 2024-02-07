// SPDX-License-Identifier: LicenseRef-ZrythmLicense
/*
 * SPDX-FileCopyrightText: © 2018-2019 Alexandros Theodotou <alex@zrythm.org>
 */

#include <stdlib.h>

#include "dsp/automation_tracklist.h"
#include "dsp/channel_track.h"

void
channel_track_setup (ChannelTrack * self)
{
  Track * track = (Track *) self;

  automation_tracklist_init (&self->automation_tracklist, track);
}

/**
 * Frees the track.
 *
 * TODO
 */
void
channel_track_free (ChannelTrack * track)
{
}
