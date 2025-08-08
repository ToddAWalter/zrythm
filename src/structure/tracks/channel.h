// SPDX-FileCopyrightText: © 2018-2022, 2024-2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#pragma once

#include "dsp/passthrough_processors.h"
#include "plugins/plugin_all.h"
#include "structure/tracks/channel_send.h"
#include "structure/tracks/fader.h"
#include "utils/icloneable.h"

namespace zrythm::structure::tracks
{
/**
 * The slot where post-fader sends begin (starting from 0).
 */
constexpr int CHANNEL_SEND_POST_FADER_START_SLOT = 6;

class ChannelMidiPassthroughProcessor final
    : public QObject,
      public dsp::MidiPassthroughProcessor
{
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE ("")

public:
  ChannelMidiPassthroughProcessor (
    dsp::ProcessorBase::ProcessorBaseDependencies dependencies,
    QObject *                                     parent = nullptr);
};

class ChannelAudioPassthroughProcessor final
    : public QObject,
      public dsp::StereoPassthroughProcessor
{
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE ("")

public:
  ChannelAudioPassthroughProcessor (
    dsp::ProcessorBase::ProcessorBaseDependencies dependencies,
    QObject *                                     parent = nullptr);
};

/**
 * @brief Represents a channel strip on the mixer.
 *
 * The Channel class encapsulates the functionality of a channel strip,
 * including its plugins, fader, sends, and other properties.
 *
 * Channels are owned by Track's and handle the second part of the signal
 * chain when processing a track, where the signal is fed to each Channel
 * subcomponent. (TrackProcessor handles the first part where any track inputs
 * and arranger events are processed).
 *
 * @see ChannelTrack, ProcessableTrack and TrackProcessor.
 */
class Channel : public QObject
{
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY (Fader * fader READ fader CONSTANT)
  Q_PROPERTY (QVariant preFader READ preFader CONSTANT)
  Q_PROPERTY (dsp::AudioPort * leftAudioOut READ getLeftAudioOut CONSTANT)
  Q_PROPERTY (dsp::AudioPort * rightAudioOut READ getRightAudioOut CONSTANT)
  Q_PROPERTY (dsp::MidiPort * midiOut READ getMidiOut CONSTANT)
  QML_UNCREATABLE ("")

public:
  using PortType = zrythm::dsp::PortType;
  using Plugin = plugins::Plugin;
  using PluginDescriptor = zrythm::plugins::PluginDescriptor;
  using PluginSlot = zrythm::plugins::PluginSlot;
  using PluginSlotType = zrythm::plugins::PluginSlotType;
  using PluginPtrVariant = plugins::PluginPtrVariant;
  using PluginUuid = Plugin::Uuid;

  /**
   * Number of plugin slots per channel.
   */
  static constexpr auto STRIP_SIZE = 9u;

  using NameProvider = std::function<utils::Utf8String ()>;

public:
  explicit Channel (
    plugins::PluginRegistry                      &plugin_registry,
    dsp::ProcessorBase::ProcessorBaseDependencies processor_dependencies,
    dsp::PortType                                 signal_type,
    NameProvider                                  name_provider,
    bool                                          hard_limit_fader_output,
    Fader::ShouldBeMutedCallback                  should_be_muted_cb,
    QObject *                                     parent = nullptr);

  // ============================================================================
  // QML Interface
  // ============================================================================

  Fader *  fader () const { return fader_.get (); }
  QVariant preFader () const
  {
    return is_midi () ? QVariant::fromValue (midi_prefader_.get ())
                      : QVariant::fromValue (audio_prefader_.get ());
  }
  dsp::AudioPort * getLeftAudioOut () const
  {
    return is_audio ()
             ? std::addressof (audio_postfader_->get_audio_out_port (0))
             : nullptr;
  }
  dsp::AudioPort * getRightAudioOut () const
  {
    return is_audio ()
             ? std::addressof ((audio_postfader_->get_audio_out_port (1)))
             : nullptr;
  }
  dsp::MidiPort * getMidiOut () const
  {
    return is_midi () ? std::addressof (midi_postfader_->get_midi_out_port (0))
                      : nullptr;
  }

  // ============================================================================

  bool is_midi () const { return signal_type_ == PortType::Midi; }
  bool is_audio () const { return signal_type_ == PortType::Audio; }

  /**
   * Adds given plugin to given position in the strip.
   *
   * This does not concern itself with automation tracks or graph rebuilding.
   * When plugins are added or removed, automation tracks should be
   * generated/moved accordingly and the DSP graph should be regenerated.
   *
   * This removes the existing plugin, so callers should handle related logic
   * like asking for confirmation or storing the previous plugin.
   *
   * @param slot The position in the strip starting from 0.
   * @param plugin The plugin to add.
   *
   * @return The previous plugin that was at that slot, if any.
   */
  std::optional<plugins::PluginUuidReference>
  add_plugin (plugins::PluginUuidReference plugin_id, PluginSlot slot);

  /**
   * @brief Returns all existing plugins in the channel.
   *
   * @param pls Vector to add plugins to.
   */
  void get_plugins (std::vector<Plugin *> &pls) const;

  std::optional<PluginPtrVariant> get_plugin_at_slot (PluginSlot slot) const;

  auto get_plugin_slot (const PluginUuid &plugin_id) const -> PluginSlot;

  std::optional<PluginPtrVariant> get_instrument () const
  {
    return instrument_ ? std::make_optional (instrument_->get_object ()) : std::nullopt;
  }

  /**
   * @brief Removes the given plugin.
   *
   * @note If moving the plugin, remember to also move automation tracks for
   * this plugin. This method is not concerned with that.
   */
  plugins::PluginUuidReference remove_plugin (plugins::Plugin::Uuid id);

  friend void init_from (
    Channel               &obj,
    const Channel         &other,
    utils::ObjectCloneType clone_type);

  Fader &get_fader () const { return *fader_; }
  auto  &get_midi_pre_fader () const { return *midi_prefader_; }
  auto  &get_audio_pre_fader () const { return *audio_prefader_; }
  auto  &get_midi_post_fader () const { return *midi_postfader_; }
  auto  &get_audio_post_fader () const { return *audio_postfader_; }

  auto &sends () const { return sends_; }

private:
  static constexpr auto kMidiFxKey = "midiFx"sv;
  static constexpr auto kInsertsKey = "inserts"sv;
  static constexpr auto kSendsKey = "sends"sv;
  static constexpr auto kInstrumentKey = "instrument"sv;
  static constexpr auto kMidiPrefaderKey = "midiPrefader"sv;
  static constexpr auto kAudioPrefaderKey = "audioPrefader"sv;
  static constexpr auto kFaderKey = "fader"sv;

  friend void to_json (nlohmann::json &j, const Channel &c)
  {
    j[kMidiFxKey] = c.midi_fx_;
    j[kInsertsKey] = c.inserts_;
    j[kSendsKey] = c.sends_;
    j[kInstrumentKey] = c.instrument_;
    j[kMidiPrefaderKey] = c.midi_prefader_;
    j[kAudioPrefaderKey] = c.audio_prefader_;
    j[kFaderKey] = c.fader_;
  }
  friend void from_json (const nlohmann::json &j, Channel &c);

  dsp::ProcessorBase::ProcessorBaseDependencies dependencies () const
  {
    return dependencies_;
  }

private:
  dsp::ProcessorBase::ProcessorBaseDependencies dependencies_;
  plugins::PluginRegistry                      &plugin_registry_;

  NameProvider name_provider_;

  dsp::PortType signal_type_;

  bool                         hard_limit_fader_output_;
  Fader::ShouldBeMutedCallback should_be_muted_cb_;

  /**
   * The MIDI effect strip on instrument/MIDI tracks.
   *
   * This is processed before the instrument/inserts.
   */
  std::array<std::optional<plugins::PluginUuidReference>, STRIP_SIZE> midi_fx_;

  /** The channel insert strip. */
  std::array<std::optional<plugins::PluginUuidReference>, STRIP_SIZE> inserts_;

  /** The instrument plugin, if instrument track. */
  std::optional<plugins::PluginUuidReference> instrument_;

  /**
   * The sends strip.
   *
   * The first 6 (slots 0-5) are pre-fader and the rest are post-fader.
   *
   * @note See CHANNEL_SEND_POST_FADER_START_SLOT.
   */
  std::array<utils::QObjectUniquePtr<ChannelSend>, STRIP_SIZE> sends_;

  /** The channel fader. */
  utils::QObjectUniquePtr<Fader> fader_;

  /**
   * Prefader.
   *
   * The last plugin should connect to this.
   */
  utils::QObjectUniquePtr<ChannelMidiPassthroughProcessor>  midi_prefader_;
  utils::QObjectUniquePtr<ChannelAudioPassthroughProcessor> audio_prefader_;

  /**
   * @brief Post-fader passthrough processor.
   *
   * This is used so we avoid custom logic for the channel output by offloading
   * that task to these.
   */
  utils::QObjectUniquePtr<ChannelMidiPassthroughProcessor>  midi_postfader_;
  utils::QObjectUniquePtr<ChannelAudioPassthroughProcessor> audio_postfader_;
};

}; // namespace zrythm::structure::tracks
