// SPDX-FileCopyrightText: © 2018-2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include <cstdlib>

#include "common/dsp/track.h"
#include "common/plugins/carla_native_plugin.h"
#include "common/plugins/collections.h"
#include "common/plugins/plugin_descriptor.h"
#include "common/plugins/plugin_manager.h"
#include "common/utils/objects.h"
#include "gui/backend/backend/zrythm.h"

#include <glib/gi18n.h>

using namespace zrythm::plugins;

void
PluginDescriptor::init_after_cloning (
  const zrythm::plugins::PluginDescriptor &other)
{
  author_ = other.author_;
  name_ = other.name_;
  website_ = other.website_;
  category_ = other.category_;
  category_str_ = other.category_str_;
  protocol_ = other.protocol_;
  num_audio_ins_ = other.num_audio_ins_;
  num_audio_outs_ = other.num_audio_outs_;
  num_midi_ins_ = other.num_midi_ins_;
  num_midi_outs_ = other.num_midi_outs_;
  num_ctrl_ins_ = other.num_ctrl_ins_;
  num_ctrl_outs_ = other.num_ctrl_outs_;
  num_cv_ins_ = other.num_cv_ins_;
  num_cv_outs_ = other.num_cv_outs_;
  arch_ = other.arch_;
  protocol_ = other.protocol_;
  path_ = other.path_;
  uri_ = other.uri_;
  unique_id_ = other.unique_id_;
  min_bridge_mode_ = other.min_bridge_mode_;
  has_custom_ui_ = other.has_custom_ui_;
  ghash_ = other.ghash_;
  sha1_ = other.sha1_;
}

#define IS_CAT(x) (category_ == ZPluginCategory::x)

bool
PluginDescriptor::is_instrument () const
{
  if (this->num_midi_ins_ == 0 || this->num_audio_outs_ == 0)
    {
      return false;
    }

  if (this->category_ == ZPluginCategory::INSTRUMENT)
    {
      return true;
    }
  else
    {
      return
        /* if VSTs are instruments their category must be INSTRUMENT, otherwise
           they are not */
        this->protocol_ != Protocol::ProtocolType::VST
        && this->category_ == ZPluginCategory::NONE && this->num_midi_ins_ > 0
        && this->num_audio_outs_ > 0;
    }
}

bool
PluginDescriptor::is_effect () const
{
  return (this->category_ > ZPluginCategory::NONE
          && (IS_CAT (DELAY) || IS_CAT (REVERB) || IS_CAT (DISTORTION) || IS_CAT (WAVESHAPER) || IS_CAT (DYNAMICS) || IS_CAT (AMPLIFIER) || IS_CAT (COMPRESSOR) || IS_CAT (ENVELOPE) || IS_CAT (EXPANDER) || IS_CAT (GATE) || IS_CAT (LIMITER) || IS_CAT (FILTER) || IS_CAT (ALLPASS_FILTER) || IS_CAT (BANDPASS_FILTER) || IS_CAT (COMB_FILTER) || IS_CAT (EQ) || IS_CAT (MULTI_EQ) || IS_CAT (PARA_EQ) || IS_CAT (HIGHPASS_FILTER) || IS_CAT (LOWPASS_FILTER) || IS_CAT (GENERATOR) || IS_CAT (CONSTANT) || IS_CAT (OSCILLATOR) || IS_CAT (MODULATOR) || IS_CAT (CHORUS) || IS_CAT (FLANGER) || IS_CAT (PHASER) || IS_CAT (SIMULATOR) || IS_CAT (SIMULATOR_REVERB) || IS_CAT (SPATIAL) || IS_CAT (SPECTRAL) || IS_CAT (PITCH) || IS_CAT (UTILITY) || IS_CAT (ANALYZER) || IS_CAT (CONVERTER) || IS_CAT (FUNCTION) || IS_CAT (MIXER)))
         || (this->category_ == ZPluginCategory::NONE && this->num_audio_ins_ > 0 && this->num_audio_outs_ > 0);
}

bool
PluginDescriptor::is_modulator () const
{
  return (this->category_ == ZPluginCategory::NONE
          || (this->category_ > ZPluginCategory::NONE && (IS_CAT (ENVELOPE) || IS_CAT (GENERATOR) || IS_CAT (CONSTANT) || IS_CAT (OSCILLATOR) || IS_CAT (MODULATOR) || IS_CAT (UTILITY) || IS_CAT (CONVERTER) || IS_CAT (FUNCTION))))
         && this->num_cv_outs_ > 0;
}

bool
PluginDescriptor::is_midi_modifier () const
{
  return (this->category_ > ZPluginCategory::NONE
          && this->category_ == ZPluginCategory::MIDI)
         || (this->category_ == ZPluginCategory::NONE && this->num_midi_ins_ > 0 && this->num_midi_outs_ > 0 && this->protocol_ != Protocol::ProtocolType::VST);
}

#undef IS_CAT

ZPluginCategory
PluginDescriptor::string_to_category (const std::string &str)
{
  ZPluginCategory category = ZPluginCategory::NONE;

#define CHECK_CAT(term, cat) \
  if (g_strrstr (str.c_str (), term)) \
  category = ZPluginCategory::cat

  /* add category */
  CHECK_CAT ("Delay", DELAY);
  CHECK_CAT ("Reverb", REVERB);
  CHECK_CAT ("Distortion", DISTORTION);
  CHECK_CAT ("Waveshaper", WAVESHAPER);
  CHECK_CAT ("Dynamics", DYNAMICS);
  CHECK_CAT ("Amplifier", AMPLIFIER);
  CHECK_CAT ("Compressor", COMPRESSOR);
  CHECK_CAT ("Envelope", ENVELOPE);
  CHECK_CAT ("Expander", EXPANDER);
  CHECK_CAT ("Gate", GATE);
  CHECK_CAT ("Limiter", LIMITER);
  CHECK_CAT ("Filter", FILTER);
  CHECK_CAT ("Allpass", ALLPASS_FILTER);
  CHECK_CAT ("Bandpass", BANDPASS_FILTER);
  CHECK_CAT ("Comb", COMB_FILTER);
  CHECK_CAT ("Equaliser", EQ);
  CHECK_CAT ("Equalizer", EQ);
  CHECK_CAT ("Multiband", MULTI_EQ);
  CHECK_CAT ("Para", PARA_EQ);
  CHECK_CAT ("Highpass", HIGHPASS_FILTER);
  CHECK_CAT ("Lowpass", LOWPASS_FILTER);
  CHECK_CAT ("Generator", GENERATOR);
  CHECK_CAT ("Constant", CONSTANT);
  CHECK_CAT ("Instrument", INSTRUMENT);
  CHECK_CAT ("Oscillator", OSCILLATOR);
  CHECK_CAT ("MIDI", MIDI);
  CHECK_CAT ("Modulator", MODULATOR);
  CHECK_CAT ("Chorus", CHORUS);
  CHECK_CAT ("Flanger", FLANGER);
  CHECK_CAT ("Phaser", PHASER);
  CHECK_CAT ("Simulator", SIMULATOR);
  CHECK_CAT ("SimulatorReverb", SIMULATOR_REVERB);
  CHECK_CAT ("Spatial", SPATIAL);
  CHECK_CAT ("Spectral", SPECTRAL);
  CHECK_CAT ("Pitch", PITCH);
  CHECK_CAT ("Utility", UTILITY);
  CHECK_CAT ("Analyser", ANALYZER);
  CHECK_CAT ("Analyzer", ANALYZER);
  CHECK_CAT ("Converter", CONVERTER);
  CHECK_CAT ("Function", FUNCTION);
  CHECK_CAT ("Mixer", MIXER);

#undef CHECK_CAT

  return category;
}

std::string
PluginDescriptor::category_to_string (ZPluginCategory category)
{

#define RET_STRING(term, cat) \
  if (category == ZPluginCategory::cat) \
  return term

  /* add category */
  RET_STRING ("Delay", DELAY);
  RET_STRING ("Reverb", REVERB);
  RET_STRING ("Distortion", DISTORTION);
  RET_STRING ("Waveshaper", WAVESHAPER);
  RET_STRING ("Dynamics", DYNAMICS);
  RET_STRING ("Amplifier", AMPLIFIER);
  RET_STRING ("Compressor", COMPRESSOR);
  RET_STRING ("Envelope", ENVELOPE);
  RET_STRING ("Expander", EXPANDER);
  RET_STRING ("Gate", GATE);
  RET_STRING ("Limiter", LIMITER);
  RET_STRING ("Filter", FILTER);
  RET_STRING ("Allpass", ALLPASS_FILTER);
  RET_STRING ("Bandpass", BANDPASS_FILTER);
  RET_STRING ("Comb", COMB_FILTER);
  RET_STRING ("Equaliser", EQ);
  RET_STRING ("Equalizer", EQ);
  RET_STRING ("Multiband", MULTI_EQ);
  RET_STRING ("Para", PARA_EQ);
  RET_STRING ("Highpass", HIGHPASS_FILTER);
  RET_STRING ("Lowpass", LOWPASS_FILTER);
  RET_STRING ("Generator", GENERATOR);
  RET_STRING ("Constant", CONSTANT);
  RET_STRING ("Instrument", INSTRUMENT);
  RET_STRING ("Oscillator", OSCILLATOR);
  RET_STRING ("MIDI", MIDI);
  RET_STRING ("Modulator", MODULATOR);
  RET_STRING ("Chorus", CHORUS);
  RET_STRING ("Flanger", FLANGER);
  RET_STRING ("Phaser", PHASER);
  RET_STRING ("Simulator", SIMULATOR);
  RET_STRING ("SimulatorReverb", SIMULATOR_REVERB);
  RET_STRING ("Spatial", SPATIAL);
  RET_STRING ("Spectral", SPECTRAL);
  RET_STRING ("Pitch", PITCH);
  RET_STRING ("Utility", UTILITY);
  RET_STRING ("Analyser", ANALYZER);
  RET_STRING ("Analyzer", ANALYZER);
  RET_STRING ("Converter", CONVERTER);
  RET_STRING ("Function", FUNCTION);
  RET_STRING ("Mixer", MIXER);

#undef RET_STRING

  return "Plugin";
}

bool
PluginDescriptor::is_valid_for_slot_type (
  zrythm::plugins::PluginSlotType slot_type,
  Track::Type                     track_type) const
{
  switch (slot_type)
    {
    case zrythm::plugins::PluginSlotType::Insert:
      if (track_type == Track::Type::Midi)
        {
          return num_midi_outs_ > 0;
        }
      else
        {
          return num_audio_outs_ > 0;
        }
    case zrythm::plugins::PluginSlotType::MidiFx:
      return num_midi_outs_ > 0;
      break;
    case zrythm::plugins::PluginSlotType::Instrument:
      return track_type == Track::Type::Instrument && is_instrument ();
    default:
      break;
    }

  z_return_val_if_reached (false);
}

bool
PluginDescriptor::has_custom_ui () const
{
  switch (protocol_)
    {
    case Protocol::ProtocolType::LV2:
    case Protocol::ProtocolType::VST:
    case Protocol::ProtocolType::VST3:
    case Protocol::ProtocolType::AudioUnit:
    case Protocol::ProtocolType::CLAP:
    case Protocol::ProtocolType::JSFX:
#if HAVE_CARLA
      return zrythm::plugins::CarlaNativePlugin::has_custom_ui (*this);
#else
      return false;
#endif
      break;
    default:
      return false;
      break;
    }

  z_return_val_if_reached (false);
}

CarlaBridgeMode
PluginDescriptor::get_min_bridge_mode () const
{
  zrythm::plugins::CarlaBridgeMode mode = zrythm::plugins::CarlaBridgeMode::None;

  if (arch_ == PluginArchitecture::ARCH_32_BIT)
    {
      mode = zrythm::plugins::CarlaBridgeMode::Full;
    }

  return mode;
}

bool
PluginDescriptor::is_whitelisted () const
{
  return false;
#if 0
  /* on wayland nothing is whitelisted */
  if (z_gtk_is_wayland ())
    {
      return false;
    }

  static const char * authors[] = {
    "Alexandros Theodotou",
    "Andrew Deryabin",
    "AnnieShin",
    "Artican",
    "Aurelien Leblond",
    "Automatl",
    "Breakfast Quay",
    "brummer",
    "Clearly Broken Software",
    "Creative Intent",
    "Damien Zammit",
    "Datsounds",
    "David Robillard",
    "Digital Suburban",
    "DISTRHO",
    "dRowAudio",
    "DrumGizmo Team",
    "falkTX",
    "Filipe Coelho",
    "Guitarix team",
    "Hanspeter Portner",
    "Hermann Meyer",
    "IEM",
    "Iurie Nistor",
    "Jean Pierre Cimalando",
    "Klangfreund",
    "kRAkEn/gORe",
    "Lkjb",
    "LSP LADSPA",
    "LSP LV2",
    "LSP VST",
    "Luciano Dato",
    "Martin Eastwood, falkTX",
    "Matt Tytel",
    "Michael Willis",
    "Michael Willis and Rob vd Berg",
    "ndc Plugs",
    "OpenAV",
    "Patrick Desaulniers",
    "Paul Ferrand",
    "Plainweave Software",
    "Punk Labs LLC",
    "Resonant DSP",
    "Robin Gareus",
    "RockHardbuns",
    "SFZTools",
    "Spencer Jackson",
    "Stefan Westerfeld",
    "Surge Synth Team",
    "Sven Jaehnichen",
    "TAL-Togu Audio Line",
    "TheWaveWarden",
    "Tom Szilagyi",
    "tumbetoene",
    "Zrythm DAW",
  };

  if (author_.empty ())
    {
      return false;
    }

  for (size_t i = 0; i < G_N_ELEMENTS (authors); i++)
    {
      if (author_ == authors[i])
        {
#  if 0
          z_debug (
            "author '%s' is whitelisted", this->author);
#  endif
          return true;
        }
    }

  return false;
#endif
}

std::string
PluginDescriptor::get_icon_name () const
{
  if (is_instrument ())
    {
      return "instrument";
    }
  else if (is_modulator ())
    {
      return "modulator";
    }
  else if (is_midi_modifier ())
    {
      return "signal-midi";
    }
  else if (is_effect ())
    {
      return "bars";
    }
  else
    {
      return "plug";
    }
}

#if 0
GMenuModel *
PluginDescriptor::generate_context_menu () const
{
  GMenu * menu = g_menu_new ();

  GMenuItem * menuitem;
  char        tmp[600];

  /* TODO */
#  if 0
  /* add option for native generic LV2 UI */
  if (this->protocol == PluginProtocol::LV2
      &&
      this->min_bridge_mode_ == zrythm::plugins::CarlaBridgeMode::None)
    {
      menuitem =
        z_gtk_create_menu_item (
          _("Add to project (native generic UI)"),
          nullptr,
          "app.plugin-browser-add-to-project");
      g_menu_append_item (menu, menuitem);
    }
#  endif

#  if HAVE_CARLA
  sprintf (tmp, "app.plugin-browser-add-to-project-carla::%p", this);
  menuitem = z_gtk_create_menu_item (_ ("Add to project"), nullptr, tmp);
  g_menu_append_item (menu, menuitem);

  PluginSetting new_setting (*this);
  if (
    has_custom_ui_
    && this->min_bridge_mode_ == zrythm::plugins::CarlaBridgeMode::None
    && !new_setting.force_generic_ui_)
    {
      sprintf (
        tmp,
        "app.plugin-browser-add-to-project-"
        "bridged-ui::%p",
        this);
      menuitem = z_gtk_create_menu_item (
        _ ("Add to project (bridged UI)"), nullptr, tmp);
      g_menu_append_item (menu, menuitem);
    }

  sprintf (
    tmp,
    "app.plugin-browser-add-to-project-bridged-"
    "full::%p",
    this);
  menuitem =
    z_gtk_create_menu_item (_ ("Add to project (bridged full)"), nullptr, tmp);
  g_menu_append_item (menu, menuitem);
#  endif

#  if 0
  menuitem =
    GTK_MENU_ITEM (
      gtk_check_menu_item_new_with_mnemonic (
        _("Use _Generic UI")));
  APPEND;
  gtk_check_menu_item_set_active (
    GTK_CHECK_MENU_ITEM (menuitem),
    new_setting->force_generic_ui);
  g_signal_connect (
    G_OBJECT (menuitem), "toggled",
    G_CALLBACK (on_use_generic_ui_toggled), this);
#  endif

  /* add to collection */
  GMenu * add_collections_submenu = g_menu_new ();
  int     num_added = 0;
  for (auto &coll : zrythm::plugins::PluginManager::get_active_instance ()->collections_->collections_)
    {
      if (coll->contains_descriptor (*this))
        {
          continue;
        }

      sprintf (tmp, "app.plugin-browser-add-to-collection::%p,%p", &coll, this);
      menuitem = z_gtk_create_menu_item (coll->name_.c_str (), nullptr, tmp);
      g_menu_append_item (add_collections_submenu, menuitem);
      num_added++;
    }
  if (num_added > 0)
    {
      g_menu_append_section (
        menu, _ ("Add to collection"), G_MENU_MODEL (add_collections_submenu));
    }
  else
    {
      g_object_unref (add_collections_submenu);
    }

  /* remove from collection */
  GMenu * remove_collections_submenu = g_menu_new ();
  num_added = 0;
  for (auto &coll : zrythm::plugins::PluginManager::get_active_instance ()->collections_->collections_)
    {
      if (!coll->contains_descriptor (*this))
        {
          continue;
        }

      sprintf (
        tmp, "app.plugin-browser-remove-from-collection::%p,%p", &coll, this);
      menuitem = z_gtk_create_menu_item (coll->name_.c_str (), nullptr, tmp);
      g_menu_append_item (remove_collections_submenu, menuitem);
      num_added++;
    }
  if (num_added > 0)
    {
      g_menu_append_section (
        menu, _ ("Remove from collection"),
        G_MENU_MODEL (remove_collections_submenu));
    }
  else
    {
      g_object_unref (remove_collections_submenu);
    }

  return G_MENU_MODEL (menu);
}
#endif

bool
PluginDescriptor::is_same_plugin (
  const zrythm::plugins::PluginDescriptor &other) const
{
  return *this == other;
}