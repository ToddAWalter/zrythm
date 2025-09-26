// SPDX-FileCopyrightText: © 2024-2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#pragma once

#include "dsp/port.h"
#include "utils/icloneable.h"
#include "utils/monotonic_time_provider.h"
#include "utils/ring_buffer.h"

#include <nlohmann/json.hpp>

namespace zrythm::dsp
{

/**
 * @brief Audio port specifics.
 */
class AudioPort final
    : public QObject,
      public Port,
      public RingBufferOwningPortMixin,
      public PortConnectionsCacheMixin<AudioPort>,
      private utils::QElapsedTimeProvider
{
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE ("")

public:
  AudioPort (utils::Utf8String label, PortFlow flow, bool is_stereo = false);

  static constexpr size_t AUDIO_RING_SIZE = 65536;

  [[gnu::hot]] void
  process_block (EngineProcessTimeInfo time_nfo) noexcept override;

  void clear_buffer (std::size_t offset, std::size_t nframes) override;

  bool is_stereo_port () const { return is_stereo_; }

  void mark_as_stereo () { is_stereo_ = true; }
  void mark_as_sidechain () { is_sidechain_ = true; }
  void mark_as_requires_limiting () { requires_limiting_ = true; }

  friend void init_from (
    AudioPort             &obj,
    const AudioPort       &other,
    utils::ObjectCloneType clone_type);

  void
  prepare_for_processing (sample_rate_t sample_rate, nframes_t max_block_length)
    override;
  void release_resources () override;

private:
  static constexpr auto kIsStereoId = "isStereo"sv;
  static constexpr auto kIsSidechainId = "isSidechain"sv;
  static constexpr auto kRequiresLimitingId = "requiresLimiting"sv;
  friend void           to_json (nlohmann::json &j, const AudioPort &port)
  {
    to_json (j, static_cast<const Port &> (port));
    j[kIsStereoId] = port.is_stereo_;
    j[kIsSidechainId] = port.is_sidechain_;
    j[kRequiresLimitingId] = port.requires_limiting_;
  }
  friend void from_json (const nlohmann::json &j, AudioPort &port)
  {
    from_json (j, static_cast<Port &> (port));
    j.at (kIsStereoId).get_to (port.is_stereo_);
    j.at (kIsSidechainId).get_to (port.is_sidechain_);
    j.at (kRequiresLimitingId).get_to (port.requires_limiting_);
  }

private:
  // Whether this is part of a stereo port group.
  bool is_stereo_{};

  /** See http://lv2plug.in/ns/ext/port-groups/port-groups.html#sideChainOf. */
  bool is_sidechain_{};

  /**
   * @brief Whether to clip the port's data to the range [-2, 2] (3dB).
   *
   * Limiting wastes around 50% of port processing CPU cycles so this is only
   * enabled if requested.
   */
  bool requires_limiting_{};

public:
  /**
   * Audio-like data buffer.
   */
  std::vector<float> buf_;

  /**
   * Ring buffer for saving the contents of the audio buffer to be used in the
   * UI instead of directly accessing the buffer.
   *
   * This should contain blocks of block_length samples and should maintain at
   * least 10 cycles' worth of buffers.
   */
  std::unique_ptr<RingBuffer<float>> audio_ring_;

private:
  BOOST_DESCRIBE_CLASS (
    AudioPort,
    (Port),
    (),
    (),
    (is_stereo_, is_sidechain_, requires_limiting_))
};

/**
 * Convenience factory for L/R audio port pairs.
 */
class StereoPorts final
{
public:
  static std::pair<utils::Utf8String, utils::Utf8String> get_name_and_symbols (
    bool              left,
    utils::Utf8String name,
    utils::Utf8String symbol)
  {
    return std::make_pair (
      utils::Utf8String::from_utf8_encoded_string (
        fmt::format ("{} {}", name, left ? "L" : "R")),
      utils::Utf8String::from_utf8_encoded_string (
        fmt::format ("{}_{}", symbol, left ? "l" : "r")));
  }

  static std::pair<PortUuidReference, PortUuidReference> create_stereo_ports (
    PortRegistry     &port_registry,
    bool              input,
    utils::Utf8String name,
    utils::Utf8String symbol);
};

} // namespace zrythm::dsp
