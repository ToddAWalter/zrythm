// SPDX-FileCopyrightText: © 2019-2022, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense
/*
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 * ---
 *
 * Copyright (C) 2017, 2019 Robin Gareus <robin@gareus.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * ---
 */

#include "zrythm-config.h"

#include "gui/dsp/audio_track.h"
#include "gui/dsp/control_port.h"
#include "gui/dsp/engine.h"
#include "gui/dsp/project_graph_builder.h"
#include "utils/debug.h"
#ifdef HAVE_JACK
#  include "gui/dsp/engine_jack.h"
#endif
#ifdef HAVE_PORT_AUDIO
#  include "gui/dsp/engine_pa.h"
#endif
#include "dsp/graph.h"
#include "gui/backend/backend/project.h"
#include "gui/backend/backend/zrythm.h"
#include "gui/dsp/port.h"
#include "gui/dsp/router.h"
#include "gui/dsp/tempo_track.h"
#include "gui/dsp/track_processor.h"
#include "gui/dsp/tracklist.h"

#ifdef HAVE_JACK
#  include "weakjack/weak_libjack.h"
#endif

Router::Router (AudioEngine * engine) : audio_engine_ (engine) { }

nframes_t
Router::get_max_route_playback_latency ()
{
  z_return_val_if_fail (scheduler_, 0);
  max_route_playback_latency_ =
    scheduler_->get_nodes ().get_max_route_playback_latency ();

  return max_route_playback_latency_;
}

void
Router::start_cycle (EngineProcessTimeInfo time_nfo)
{
  z_return_if_fail (scheduler_);
  z_return_if_fail (
    time_nfo.local_offset_ + time_nfo.nframes_ <= AUDIO_ENGINE->nframes_);

  /* only set the kickoff thread when not called from the gtk thread (sometimes
   * this is called from the gtk thread to force some processing) */
  if (!ZRYTHM_IS_QT_THREAD)
    process_kickoff_thread_ = current_thread_id.get ();

  SemaphoreRAII<> sem (graph_access_sem_);
  if (!sem.is_acquired ())
    {
      z_info ("graph access is busy, returning...");
      return;
    }

  global_offset_ =
    max_route_playback_latency_ - AUDIO_ENGINE->remaining_latency_preroll_;
  time_nfo_ = time_nfo;
  z_return_if_fail_cmp (
    time_nfo.g_start_frame_w_offset_, >=, time_nfo.g_start_frame_);

  /* read control port change events */
  ControlPort::ChangeEvent change{};
  while (ctrl_port_change_queue_.read (change))
    {
      if (ENUM_BITSET_TEST (change.flag1, dsp::PortIdentifier::Flags::Bpm))
        {
          P_TEMPO_TRACK->set_bpm (change.real_val, 0.f, true, true);
        }
      else if (
        ENUM_BITSET_TEST (change.flag2, dsp::PortIdentifier::Flags2::BeatsPerBar))
        {
          P_TEMPO_TRACK->set_beats_per_bar (change.ival);
        }
      else if (
        ENUM_BITSET_TEST (change.flag2, dsp::PortIdentifier::Flags2::BeatUnit))
        {
          P_TEMPO_TRACK->set_beat_unit_from_enum (change.beat_unit);
        }
    }

  callback_in_progress_ = true;
  scheduler_->run_cycle (time_nfo_, AUDIO_ENGINE->remaining_latency_preroll_);
  callback_in_progress_ = false;
}

void
Router::recalc_graph (bool soft)
{
  z_info ("Recalculating{}...", soft ? " (soft)" : "");

  auto rebuild_graph = [&] () {
    graph_setup_in_progress_.store (true);
    ProjectGraphBuilder builder (*PROJECT, true);
    dsp::graph::Graph   graph;
    builder.build_graph (graph);
    PROJECT->clip_editor_->set_caches ();
    TRACKLIST->get_track_span ().set_caches (ALL_CACHE_TYPES);
    scheduler_->rechain_from_node_collection (graph.steal_nodes ());
    graph_setup_in_progress_.store (false);
  };

  if (!scheduler_ && !soft)
    {
      scheduler_ = std::make_unique<dsp::graph::GraphScheduler> ();
      rebuild_graph ();
      scheduler_->start_threads ();
      return;
    }

  if (soft)
    {
      graph_access_sem_.acquire ();
      scheduler_->get_nodes ().update_latencies ();
      graph_access_sem_.release ();
    }
  else
    {
      bool running = AUDIO_ENGINE->run_.load ();
      AUDIO_ENGINE->run_.store (false);
      while (AUDIO_ENGINE->cycle_running_.load ())
        {
          std::this_thread::sleep_for (std::chrono::milliseconds (100));
        }
      rebuild_graph ();
      AUDIO_ENGINE->run_.store (running);
    }

  z_info ("done");
}

void
Router::queue_control_port_change (const ControlPort::ChangeEvent &change)
{
  ctrl_port_change_queue_.force_write (change);
}
