// SPDX-FileCopyrightText: © 2019-2021, 2024 Alexandros Theodotou <alex@zrythm.org>
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

#ifndef ZRYTHM_DSP_GRAPH_NODE_H
#define ZRYTHM_DSP_GRAPH_NODE_H

#include "dsp/itransport.h"
#include "utils/types.h"

namespace zrythm::dsp
{

/**
 * @brief Represents a node in a DSP graph.
 *
 * GraphNode is a fundamental building block of the DSP graph, responsible for
 * processing audio/MIDI data. It encapsulates the necessary functions and
 * properties to handle the processing, latency compensation, and connection
 * management within the graph.
 *
 * The class provides the following key features:
 * - Configurable processing function and name getter
 * - Playback latency management and compensation
 * - Ability to connect to other GraphNode instances
 * - Skipping of processing for muting/disabling the node
 *
 * GraphNode is designed to be used as part of the larger DSP graph system,
 * providing the necessary functionality to handle the individual nodes and
 * their interactions.
 */
class GraphNode
{
public:
  /**
   * @brief Function to get a human-readable identifying label for this node.
   *
   * Currently used for debugging.
   */
  using NameGetter = std::function<std::string ()>;

  /**
   * @brief Function to process the node.
   */
  using ProcessFunc = std::function<void (EngineProcessTimeInfo)>;

  static ProcessFunc DefaultProcessFunc ()
  {
    return [] (EngineProcessTimeInfo) {};
  }

  /**
   * @brief Function to return the single playback latency of the node in frames.
   */
  using SinglePlaybackLatencyGetter = std::function<nframes_t ()>;

  static SinglePlaybackLatencyGetter DefaultSinglePlaybackLatencyGetter ()
  {
    return [] () -> nframes_t { return 0; };
  }

  using Id = int;

  GraphNode (
    Id                          id,
    NameGetter                  name_getter,
    dsp::ITransport            &transport,
    ProcessFunc                 process_func,
    SinglePlaybackLatencyGetter playback_latency_getter =
      DefaultSinglePlaybackLatencyGetter ());

  /**
   * Returns a human friendly name of the node.
   */
  std::string get_name () const;

  /** For general debugging. */
  std::string print_to_str () const;

  void print () const;

  /**
   * Processes the GraphNode.
   *
   * @param remaining_preroll_frames The number of frames remaining for preroll
   * (as part of playback latency adjustment).
   * @param processing_cycle_in_progress Whether the processing cycle is
   * currently in progress (and this function is called as part of it), as
   * opposed to being called before/after a processing cycle (e.g., for some
   * special nodes that are processed before/after the actual processing).
   */
  ATTR_HOT void
  process (EngineProcessTimeInfo time_nfo, nframes_t remaining_preroll_frames)
    const;

  /**
   * Returns the latency of only the given port, without adding the
   * previous/next latencies.
   *
   * It returns the plugin's latency if plugin, otherwise 0.
   */
  ATTR_HOT nframes_t get_single_playback_latency () const;

  /**
   * Sets the playback latency of the given node recursively.
   *
   * Used only when (re)creating the graph.
   *
   * @param dest_latency The total destination latency so far.
   */
  void set_route_playback_latency (nframes_t dest_latency);

  void connect_to (GraphNode &target);

  /**
   * Sets whether processing should be skipped for this node.
   *
   * When set to true, the node's processing function will be bypassed,
   * effectively muting/disabling the node while keeping it in the graph.
   *
   * @param skip True to skip processing, false to process normally
   */
  void set_skip_processing (bool skip) { bypass_ = skip; }

private:
  ATTR_HOT void process_internal (EngineProcessTimeInfo time_nfo) const;

  void add_feeds (GraphNode &dest);
  void add_depends (GraphNode &src);

  /**
   * Handles latency compensation when transport is rolling.
   *
   * Adjusts the time info based on the difference between route playback
   * latency and remaining preroll frames. This ensures proper timing when
   * processing nodes with different latencies in the signal chain.
   *
   * @param time_nfo Time info to be adjusted
   * @param remaining_preroll_frames Frames remaining in preroll period
   */
  ATTR_HOT void compensate_latency (
    EngineProcessTimeInfo &time_nfo,
    nframes_t              remaining_preroll_frames) const;

  /**
   * Processes audio in chunks when loop points are encountered.
   *
   * Splits processing into multiple chunks when the playhead crosses the
   * transport loop points, ensuring seamless audio playback during looping.
   * Updates time info to handle loop point transitions correctly.
   *
   * @param time_nfo Time info containing frame counts and offsets
   */
  ATTR_HOT void process_chunks_after_splitting_at_loop_points (
    EngineProcessTimeInfo &time_nfo) const;

public:
  /**
   * @brief Outgoing nodes.
   *
   * Downstream nodes to activate when this node has completed processing.
   *
   * @note These are not owned.
   */
  std::vector<GraphNode *> childnodes_;

  /** Initial incoming node count. */
  int init_refcount_ = 0;

  /** The playback latency of the node, in samples. */
  nframes_t playback_latency_ = 0;

  /** The route's playback latency so far. */
  nframes_t route_playback_latency_ = 0;

  /** For debugging. */
  bool terminal_ = false;
  bool initial_ = false;

  /** Incoming node count. */
  std::atomic<int> refcount_ = 0;

private:
  int id_ = 0;

  /**
   * @brief Incoming nodes.
   *
   * Used when creating the graph so we can traverse it backwards to set the
   * latencies.
   *
   * @note These are not owned.
   */
  std::vector<GraphNode *> parentnodes_;

  dsp::ITransport            &transport_;
  NameGetter                  name_getter_;
  ProcessFunc                 process_func_;
  SinglePlaybackLatencyGetter playback_latency_getter_;

  /**
   * @brief Flag to skip processing.
   */
  bool bypass_ = false;
};

} // namespace zrythm::dsp

#endif
