// SPDX-FileCopyrightText: © 2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#pragma once

#include "dsp/graph.h"
#include "dsp/port_all.h"

namespace zrythm::structure::tracks
{
/**
 * @brief A helper class to add nodes and standard connections for a channel to
 * a DSP graph.
 */
class ChannelSubgraphBuilder
{
public:
  static void add_nodes (
    dsp::graph::Graph &graph,
    dsp::ITransport   &transport,
    Channel           &ch,
    bool               skip_unnecessary);

  /**
   * @brief
   *
   * @param graph
   * @param ch
   * @param track_processor_outputs Track processor outputs to connect to the
   * channel's input (or the first plugin).
   */
  static void add_connections (
    dsp::graph::Graph                &graph,
    Channel                          &ch,
    std::span<dsp::PortUuidReference> track_processor_outputs,
    bool                              skip_unnecessary);
};
} // namespace zrythm::structure::tracks
