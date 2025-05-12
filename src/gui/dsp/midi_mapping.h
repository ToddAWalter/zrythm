// SPDX-FileCopyrightText: © 2019-2022, 2024-2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#pragma once

#include "gui/dsp/ext_port.h"
#include "gui/dsp/port.h"
#include "utils/icloneable.h"

using WrappedObjectWithChangeSignal = struct _WrappedObjectWithChangeSignal;

/**
 * @addtogroup dsp
 *
 * @{
 */

#define MIDI_MAPPINGS (PROJECT->midi_mappings_)

/**
 * A mapping from a MIDI CC value to a destination ControlPort.
 */
class MidiMapping final : public QObject, public ICloneable<MidiMapping>
{
  Q_OBJECT
  QML_ELEMENT

public:
  using PortIdentifier = zrythm::dsp::PortIdentifier;

  MidiMapping (QObject * parent = nullptr);

public:
  void init_after_cloning (const MidiMapping &other, ObjectCloneType clone_type)
    override;

  void set_enabled (bool enabled) { enabled_.store (enabled); }

  void apply (std::array<midi_byte_t, 3> buf);

private:
  static constexpr auto kKeyKey = "key"sv;
  static constexpr auto kDevicePortKey = "devicePort"sv;
  static constexpr auto kDestIdKey = "destId"sv;
  static constexpr auto kEnabledKey = "enabled"sv;
  friend void           to_json (nlohmann::json &j, const MidiMapping &mapping)
  {
    j = nlohmann::json{
      { kKeyKey,        mapping.key_             },
      { kDevicePortKey, mapping.device_port_     },
      { kDestIdKey,     mapping.dest_id_         },
      { kEnabledKey,    mapping.enabled_.load () },
    };
  }
  friend void from_json (const nlohmann::json &j, MidiMapping &mapping)
  {
    j.at (kKeyKey).get_to (mapping.key_);
    j.at (kDevicePortKey).get_to (mapping.device_port_);
    j.at (kDestIdKey).get_to (mapping.dest_id_);
    mapping.enabled_.store (j.at (kEnabledKey).get<bool> ());
  }

public:
  /** Raw MIDI signal. */
  std::array<midi_byte_t, 3> key_ = {};

  /** The device that this connection will be mapped for. */
  std::unique_ptr<ExtPort> device_port_;

  /** Destination. */
  std::optional<PortIdentifier::PortUuid> dest_id_;

  /**
   * Destination pointer, for convenience.
   *
   * @note This pointer is not owned by this instance.
   */
  Port * dest_ = nullptr;

  /** Whether this binding is enabled. */
  /* TODO: check if really should be atomic */
  std::atomic<bool> enabled_ = false;
};

/**
 * All MIDI mappings in Zrythm.
 */
class MidiMappings final : public ICloneable<MidiMappings>
{
public:
  void init_loaded ();

  /**
   * Binds the CC represented by the given raw buffer (must be size 3) to the
   * given Port.
   *
   * @param idx Index to insert at.
   * @param buf The buffer used for matching at [0] and [1].
   * @param device_port Device port, if custom mapping.
   */
  void bind_at (
    std::array<midi_byte_t, 3> buf,
    ExtPort *                  device_port,
    Port                      &dest_port,
    int                        idx,
    bool                       fire_events);

  /**
   * Unbinds the given binding.
   *
   * @note This must be called inside a port operation lock, such as inside an
   * undoable action.
   */
  void unbind (int idx, bool fire_events);

  void bind_device (
    std::array<midi_byte_t, 3> buf,
    ExtPort *                  dev_port,
    Port                      &dest_port,
    bool                       fire_events)
  {
    bind_at (
      buf, dev_port, dest_port, static_cast<int> (mappings_.size ()),
      fire_events);
  }

  void
  bind_track (std::array<midi_byte_t, 3> buf, Port &dest_port, bool fire_events)
  {
    bind_at (
      buf, nullptr, dest_port, static_cast<int> (mappings_.size ()),
      fire_events);
  }

  int get_mapping_index (const MidiMapping &mapping) const;

  /**
   * Applies the events to the appropriate mapping.
   *
   * This is used only for TrackProcessor.cc_mappings.
   *
   * @note Must only be called while transport is recording.
   */
  void apply_from_cc_events (MidiEventVector &events);

  /**
   * Applies the given buffer to the matching ports.
   */
  void apply (const midi_byte_t * buf);

  /**
   * Get MIDI mappings for the given port.
   *
   * @param arr Optional array to fill with the mappings.
   *
   * @return The number of results.
   */
  int
  get_for_port (const Port &dest_port, std::vector<MidiMapping *> * arr) const;

  void
  init_after_cloning (const MidiMappings &other, ObjectCloneType clone_type)
    override
  {
    clone_unique_ptr_container (mappings_, other.mappings_);
  }

private:
  static constexpr auto kMappingsKey = "mappings"sv;
  friend void to_json (nlohmann::json &j, const MidiMappings &mappings)
  {
    j[kMappingsKey] = mappings.mappings_;
  }
  friend void from_json (const nlohmann::json &j, MidiMappings &mappings);

public:
  std::vector<std::unique_ptr<MidiMapping>> mappings_;
};

/**
 * @}
 */
