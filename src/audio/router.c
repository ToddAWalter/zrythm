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
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 * Copyright (C) 2017, 2019 Robin Gareus <robin@gareus.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "zrythm-config.h"

#include "audio/audio_track.h"
#include "audio/engine.h"
#include "audio/engine_alsa.h"
#ifdef HAVE_JACK
#include "audio/engine_jack.h"
#endif
#ifdef HAVE_PORT_AUDIO
#include "audio/engine_pa.h"
#endif
#include "audio/graph.h"
#include "audio/graph_thread.h"
#include "audio/master_track.h"
#include "audio/midi.h"
#include "audio/midi_track.h"
#include "audio/pan.h"
#include "audio/port.h"
#include "audio/router.h"
#include "audio/stretcher.h"
#include "audio/tempo_track.h"
#include "audio/track.h"
#include "audio/track_processor.h"
#include "project.h"
#include "utils/arrays.h"
#include "utils/env.h"
#include "utils/mpmc_queue.h"
#include "utils/object_utils.h"
#include "utils/objects.h"
#include "utils/stoat.h"

#ifdef HAVE_JACK
#include "weak_libjack.h"
#endif

/**
 * Returns the max playback latency of the trigger
 * nodes.
 */
nframes_t
router_get_max_route_playback_latency (
  Router * router)
{
  g_return_val_if_fail (
    router && router->graph, 0);
  router->max_route_playback_latency =
    graph_get_max_route_playback_latency (
      router->graph, false);

  return router->max_route_playback_latency;
}

/**
 * Starts a new cycle.
 *
 * @param local_offset The local offset to start
 *   playing from in this cycle:
 *   (0 - <engine buffer size>)
 */
void
router_start_cycle (
  Router *         self,
  const nframes_t  nsamples,
  const nframes_t  local_offset,
  const Position * pos)
{
  g_return_if_fail (self && self->graph);
  g_return_if_fail (
    local_offset + nsamples <=
      AUDIO_ENGINE->nframes);

  if (!zix_sem_try_wait (&self->graph_access))
    {
      g_message (
        "graph access is busy, returning...");
      return;
    }

  self->nsamples = nsamples;
  self->global_offset =
    self->max_route_playback_latency -
    AUDIO_ENGINE->remaining_latency_preroll;
  self->local_offset = local_offset;

  /* process tempo track ports first */
  if (self->graph->bpm_node)
    {
      graph_node_process (
        self->graph->bpm_node, nsamples);
    }
  if (self->graph->beats_per_bar_node)
    {
      graph_node_process (
        self->graph->beats_per_bar_node, nsamples);
    }
  if (self->graph->beat_unit_node)
    {
      graph_node_process (
        self->graph->beat_unit_node, nsamples);
    }

  self->callback_in_progress = true;
  zix_sem_post (&self->graph->callback_start);
  zix_sem_wait (&self->graph->callback_done);
  self->callback_in_progress = false;

  zix_sem_post (&self->graph_access);
}

/**
 * Recalculates the process acyclic directed graph.
 *
 * @param soft If true, only readjusts latencies.
 */
void
router_recalc_graph (
  Router * self,
  bool     soft)
{
  g_message (
    "Recalculating%s...", soft ? " (soft)" : "");

  g_return_if_fail (self);

  if (!self->graph && !soft)
    {
      self->graph = graph_new (self);
      graph_setup (self->graph, 1, 1);
      graph_start (self->graph);
      return;
    }

  if (soft)
    {
      zix_sem_wait (&self->graph_access);
      graph_update_latencies (self->graph, false);
      zix_sem_post (&self->graph_access);
    }
  else
    {
      zix_sem_wait (&self->graph_access);
      graph_setup (self->graph, 1, 1);
      zix_sem_post (&self->graph_access);
    }

  g_message ("done");
}

/**
 * Creates a new Router.
 *
 * There is only 1 router needed in a project.
 */
Router *
router_new (void)
{
  g_message ("Creating new router...");

  Router * self = object_new (Router);

  zix_sem_init (&self->graph_access, 1);

  g_message ("done");

  return self;
}

/**
 * Returns if the current thread is a
 * processing thread.
 */
bool
router_is_processing_thread (
  Router * self)
{
  if (!self || !self->graph)
    return false;

  for (int j = 0;
       j < self->graph->num_threads; j++)
    {
#ifdef HAVE_JACK
      if (AUDIO_ENGINE->audio_backend ==
            AUDIO_BACKEND_JACK)
        {
          if (pthread_equal (
                pthread_self (),
                self->graph->threads[j]->jthread))
            return true;
        }
#endif
#ifndef HAVE_JACK
      if (pthread_equal (
            pthread_self (),
            self->graph->threads[j]->pthread))
        return true;
#endif
    }

#ifdef HAVE_JACK
  if (AUDIO_ENGINE->audio_backend ==
        AUDIO_BACKEND_JACK)
    {
      if (pthread_equal (
            pthread_self (),
            self->graph->main_thread->jthread))
        return true;
    }
#endif
#ifndef HAVE_JACK
  if (pthread_equal (
        pthread_self (),
        self->graph->main_thread->pthread))
    return true;
#endif

  return false;
}

void
router_free (
  Router * self)
{
  g_debug ("%s: freeing...", __func__);

  if (self->graph)
    graph_destroy (self->graph);
  self->graph = NULL;

  zix_sem_destroy (&self->graph_access);
  object_set_to_zero (&self->graph_access);

  object_zero_and_free (self);

  g_debug ("%s: done", __func__);
}
