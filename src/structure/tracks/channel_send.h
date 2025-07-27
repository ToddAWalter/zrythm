// SPDX-FileCopyrightText: © 2020-2021, 2024-2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#pragma once

#include "dsp/audio_port.h"
#include "dsp/midi_port.h"
#include "dsp/parameter.h"
#include "dsp/port_connection.h"
#include "dsp/processor_base.h"
#include "structure/tracks/track.h"
#include "utils/icloneable.h"

namespace zrythm::structure::tracks
{

/**
 * Channel send.
 *
 * The actual connection is tracked separately by PortConnectionsManager.
 */
class ChannelSend final : public QObject, public dsp::ProcessorBase
{
  Q_OBJECT
public:
  using PortType = dsp::PortType;
  using PortConnection = dsp::PortConnection;

  struct SlotTag
  {
    int value_{};
  };

public:
  Z_DISABLE_COPY_MOVE (ChannelSend)

  // FIXME confusing constructors - maybe use a builder

  /**
   * To be used when creating a new (identity) ChannelSend.
   */
  ChannelSend (
    ChannelTrack                    &track,
    TrackRegistry                   &track_registry,
    dsp::PortRegistry               &port_registry,
    dsp::ProcessorParameterRegistry &param_registry,
    int                              slot)
      : ChannelSend (track_registry, port_registry, param_registry, track, slot, true)
  {
  }

  /**
   * To be used when deserializing.
   */
  ChannelSend (
    ChannelTrack                    &track,
    TrackRegistry                   &track_registry,
    dsp::PortRegistry               &port_registry,
    dsp::ProcessorParameterRegistry &param_registry)
      : ChannelSend (
          track_registry,
          port_registry,
          param_registry,
          track,
          std::nullopt,
          false)
  {
  }

  /**
   * @brief To be used when instantiating or cloning an existing identity.
   */
  ChannelSend (
    TrackRegistry                   &track_registry,
    dsp::PortRegistry               &port_registry,
    dsp::ProcessorParameterRegistry &param_registry)
      : ChannelSend (
          track_registry,
          port_registry,
          param_registry,
          std::nullopt,
          std::nullopt,
          false)
  {
  }

private:
  /**
   * @brief Internal implementation detail.
   */
  ChannelSend (
    TrackRegistry                   &track_registry,
    dsp::PortRegistry               &port_registry,
    dsp::ProcessorParameterRegistry &param_registry,
    OptionalRef<ChannelTrack>        track,
    std::optional<int>               slot,
    bool                             new_identity);

private:
  auto &get_port_registry () { return port_registry_; }
  auto &get_port_registry () const { return port_registry_; }

public:
  void init_loaded (ChannelTrack * track);

  utils::Utf8String get_full_designation_for_port (const dsp::Port &port) const;

  bool is_prefader () const
  {
    return slot_ < CHANNEL_SEND_POST_FADER_START_SLOT;
  }

  /**
   * Gets the owner track.
   */
  ChannelTrack * get_track () const;

  bool is_enabled () const;

  bool is_empty () const { return !is_enabled (); }

  /**
   * Returns whether the channel send target is a
   * sidechain port (rather than a target track).
   */
  bool is_target_sidechain ();

  /**
   * Gets the target track.
   */
  Track * get_target_track ();

  /**
   * Gets the amount to be used in widgets (0.0-1.0).
   */
  float get_amount_for_widgets () const;

  /**
   * Sets the amount from a widget amount (0.0-1.0).
   */
  void set_amount_from_widget (float val);

  /**
   * Connects a send to stereo ports.
   *
   * @throw ZrythmException if the connection fails.
   */
  bool connect_stereo (
    dsp::AudioPort &l,
    dsp::AudioPort &r,
    bool            sidechain,
    bool            recalc_graph,
    bool            validate);

  /**
   * Connects a send to a midi port.
   *
   * @throw ZrythmException if the connection fails.
   */
  bool connect_midi (dsp::MidiPort &port, bool recalc_graph, bool validate);

  /**
   * Removes the connection at the given send.
   */
  void disconnect (bool recalc_graph);

  /**
   * @brief Set the amount in amplitude (0-2).
   *
   * @param amount
   */
  void set_amount_in_amplitude (float amount);

  std::pair<dsp::AudioPort &, dsp::AudioPort &> get_stereo_in_ports () const
  {
    auto * l = get_input_ports ().at (0).get_object_as<dsp::AudioPort> ();
    auto * r = get_input_ports ().at (1).get_object_as<dsp::AudioPort> ();
    return { *l, *r };
  }
  dsp::MidiPort &get_midi_in_port () const
  {
    return *get_input_ports ().front ().get_object_as<dsp::MidiPort> ();
  }
  std::pair<dsp::AudioPort &, dsp::AudioPort &> get_stereo_out_ports () const
  {
    auto * l = get_output_ports ().at (0).get_object_as<dsp::AudioPort> ();
    auto * r = get_output_ports ().at (1).get_object_as<dsp::AudioPort> ();
    return { *l, *r };
  }
  dsp::MidiPort &get_midi_out_port () const
  {
    return *get_output_ports ().front ().get_object_as<dsp::MidiPort> ();
  }
  auto &get_amount_param () const
  {
    return *amount_id_.get_object_as<dsp::ProcessorParameter> ();
  }
  auto &get_enabled_param () const
  {
    return *enabled_id_.get_object_as<dsp::ProcessorParameter> ();
  }

  float get_current_amount_value () const
  {
    const auto &amount_param = get_amount_param ();
    return amount_param.range ().convertFrom0To1 (amount_param.currentValue ());
  }

  /**
   * Get the name of the destination, or a placeholder text if empty.
   */
  utils::Utf8String get_dest_name () const;

  void copy_values_from (const ChannelSend &other);

  friend void init_from (
    ChannelSend           &obj,
    const ChannelSend     &other,
    utils::ObjectCloneType clone_type);

  /**
   * Appends the connection(s), if non-empty, to the given array (if not
   * nullptr) and returns the number of connections added.
   */
  int append_connection (
    const zrythm::dsp::PortConnectionsManager * mgr,
    std::vector<PortConnection *>              &arr) const;

  void prepare_process (std::size_t block_length);

  void custom_process_block (EngineProcessTimeInfo time_nfo) override;

  bool
  is_connected_to (std::pair<dsp::Port::Uuid, dsp::Port::Uuid> stereo) const
  {
    return is_connected_to (stereo, std::nullopt);
  }
  bool is_connected_to (const dsp::Port::Uuid &midi) const
  {
    return is_connected_to (std::nullopt, midi);
  }

  bool is_audio () const { return get_signal_type () == PortType::Audio; }
  bool is_midi () const { return get_signal_type () == PortType::Event; }

private:
  static constexpr auto kSlotKey = "slot"sv;
  static constexpr auto kAmountKey = "amount"sv;
  static constexpr auto kEnabledKey = "enabled"sv;
  static constexpr auto kIsSidechainKey = "isSidechain"sv;
  static constexpr auto kMidiInKey = "midiIn"sv;
  static constexpr auto kStereoInLKey = "stereoInL"sv;
  static constexpr auto kStereoInRKey = "stereoInR"sv;
  static constexpr auto kMidiOutKey = "midiOut"sv;
  static constexpr auto kStereoOutLKey = "stereoOutL"sv;
  static constexpr auto kStereoOutRKey = "stereoOutR"sv;
  static constexpr auto kTrackIdKey = "trackId"sv;
  friend void           to_json (nlohmann::json &j, const ChannelSend &p)
  {
    to_json (j, static_cast<const dsp::ProcessorBase &> (p));
    j[kSlotKey] = p.slot_;
    j[kAmountKey] = p.amount_id_;
    j[kEnabledKey] = p.enabled_id_;
    j[kIsSidechainKey] = p.is_sidechain_;
    j[kTrackIdKey] = p.track_id_;
  }
  friend void from_json (const nlohmann::json &j, ChannelSend &p);

  PortType get_signal_type () const;

  void disconnect_midi ();
  void disconnect_audio ();

  void construct_for_slot (ChannelTrack &track, int slot);

  dsp::PortConnectionsManager * get_port_connections_manager () const;

  /**
   * Returns whether the send is connected to the given ports.
   */
  bool is_connected_to (
    std::optional<std::pair<dsp::Port::Uuid, dsp::Port::Uuid>> stereo,
    std::optional<dsp::Port::Uuid>                             midi) const;

public:
  dsp::PortRegistry               &port_registry_;
  dsp::ProcessorParameterRegistry &param_registry_;
  TrackRegistry                   &track_registry_;

  /** Slot index in the channel sends. */
  int slot_ = 0;

  /** Send amount (amplitude), 0 to 2 for audio, velocity multiplier for
   * MIDI. */
  dsp::ProcessorParameterUuidReference amount_id_;

  /**
   * Whether the send is currently enabled.
   *
   * If enabled, corresponding connection(s) will exist in
   * PortConnectionsManager.
   */
  dsp::ProcessorParameterUuidReference enabled_id_;

  /** If the send is a sidechain. */
  bool is_sidechain_ = false;

  /** Pointer back to owner track. */
  // ChannelTrack * track_ = nullptr;

  /** Owner track ID. */
  TrackUuid track_id_;

  /**
   * @brief Use this if set (via the new identity constructo).
   */
  OptionalRef<ChannelTrack> track_;
};

} // namespace zrythm::structure::tracks
