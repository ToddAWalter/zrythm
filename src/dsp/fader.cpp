// SPDX-FileCopyrightText: © 2019-2022, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "actions/tracklist_selections.h"
#include "dsp/audio_port.h"
#include "dsp/balance_control.h"
#include "dsp/channel.h"
#include "dsp/control_port.h"
#include "dsp/control_room.h"
#include "dsp/engine.h"
#include "dsp/fader.h"
#include "dsp/group_target_track.h"
#include "dsp/master_track.h"
#include "dsp/midi_event.h"
#include "dsp/port.h"
#include "dsp/track.h"
#include "dsp/tracklist.h"
#include "gui/backend/event.h"
#include "gui/backend/event_manager.h"
#include "project.h"
#include "settings/g_settings_manager.h"
#include "utils/debug.h"
#include "utils/dsp.h"
#include "utils/math.h"
#include "utils/midi.h"
#include "utils/rt_thread_id.h"
#include "zrythm.h"
#include "zrythm_app.h"

#include <glib/gi18n.h>

void
Fader::init_loaded (
  Track *           track,
  ControlRoom *     control_room,
  SampleProcessor * sample_processor)
{
  track_ = track;
  control_room_ = control_room;
  sample_processor_ = sample_processor;

  std::vector<Port *> ports;
  append_ports (ports);
  for (auto * port : ports)
    {
      port->init_loaded (this);
    }

  set_amp (amp_->control_);
}

void
Fader::append_ports (std::vector<Port *> &ports) const
{
  auto add_port = [&ports] (Port * port) {
    if (port)
      ports.push_back (port);
  };

  add_port (amp_.get ());
  add_port (balance_.get ());
  add_port (mute_.get ());
  add_port (solo_.get ());
  add_port (listen_.get ());
  add_port (mono_compat_enabled_.get ());
  add_port (swap_phase_.get ());
  if (stereo_in_)
    {
      add_port (&stereo_in_->get_l ());
      add_port (&stereo_in_->get_r ());
    }
  if (stereo_out_)
    {
      add_port (&stereo_out_->get_l ());
      add_port (&stereo_out_->get_r ());
    }
  add_port (midi_in_.get ());
  add_port (midi_out_.get ());
}

std::unique_ptr<ControlPort>
Fader::create_swap_phase_port (bool passthrough)
{
  auto swap_phase = std::make_unique<ControlPort> (
    passthrough ? _ ("Prefader Swap Phase") : _ ("Fader Swap Phase"));
  swap_phase->id_.sym_ =
    passthrough ? "prefader_swap_phase" : "fader_swap_phase";
  swap_phase->id_.flags2_ |= PortIdentifier::Flags2::FaderSwapPhase;
  swap_phase->id_.flags_ |= PortIdentifier::Flags::Toggle;
  return swap_phase;
}

Fader::Fader (
  Type              type,
  bool              passthrough,
  Track *           track,
  ControlRoom *     control_room,
  SampleProcessor * sample_processor)
    : type_ (type), midi_mode_ (MidiFaderMode::MIDI_FADER_MODE_VEL_MULTIPLIER),
      passthrough_ (passthrough), track_ (track), control_room_ (control_room),
      sample_processor_ (sample_processor)
{
  /* set volume */
  float amp = 1.f;
  amp_ = std::make_unique<ControlPort> (
    passthrough ? _ ("Prefader Volume") : _ ("Fader Volume"));
  amp_->set_owner (this);
  amp_->id_.sym_ = passthrough ? "prefader_volume" : "fader_volume";
  amp_->deff_ = amp;
  amp_->minf_ = 0.f;
  amp_->maxf_ = 2.f;
  amp_->set_control_value (amp, false, false);
  fader_val_ = math_get_fader_val_from_amp (amp);
  amp_->id_.flags_ |= PortIdentifier::Flags::Amplitude;
  if ((type == Type::AudioChannel || type == Type::MidiChannel) && !passthrough)
    {
      amp_->id_.flags_ |= PortIdentifier::Flags::Automatable;
      amp_->id_.flags_ |= PortIdentifier::Flags::ChannelFader;
    }

  /* set pan */
  float balance = 0.5f;
  balance_ = std::make_unique<ControlPort> (
    passthrough ? _ ("Prefader Balance") : _ ("Fader Balance"));
  balance_->set_owner (this);
  balance_->id_.sym_ = passthrough ? "prefader_balance" : "fader_balance";
  balance_->set_control_value (balance, 0, 0);
  balance_->id_.flags_ |= PortIdentifier::Flags::StereoBalance;
  if ((type == Type::AudioChannel || type == Type::MidiChannel) && !passthrough)
    {
      balance_->id_.flags_ |= PortIdentifier::Flags::Automatable;
    }

  /* set mute */
  mute_ = std::make_unique<ControlPort> (
    passthrough ? _ ("Prefader Mute") : _ ("Fader Mute"));
  mute_->set_owner (this);
  mute_->id_.sym_ = passthrough ? "prefader_mute" : "fader_mute";
  mute_->set_toggled (false, false);
  mute_->id_.flags_ |= PortIdentifier::Flags::FaderMute;
  mute_->id_.flags_ |= PortIdentifier::Flags::Toggle;
  if ((type == Type::AudioChannel || type == Type::MidiChannel) && !passthrough)
    {
      mute_->id_.flags_ |= PortIdentifier::Flags::Automatable;
    }

  /* set solo */
  solo_ = std::make_unique<ControlPort> (
    passthrough ? _ ("Prefader Solo") : _ ("Fader Solo"));
  solo_->set_owner (this);
  solo_->id_.sym_ = passthrough ? "prefader_solo" : "fader_solo";
  solo_->set_toggled (false, false);
  solo_->id_.flags2_ |= PortIdentifier::Flags2::FaderSolo;
  solo_->id_.flags_ |= PortIdentifier::Flags::Toggle;

  /* set listen */
  listen_ = std::make_unique<ControlPort> (
    passthrough ? _ ("Prefader Listen") : _ ("Fader Listen"));
  listen_->set_owner (this);
  listen_->id_.sym_ = passthrough ? "prefader_listen" : "fader_listen";
  listen_->set_toggled (false, false);
  listen_->id_.flags2_ |= PortIdentifier::Flags2::FaderListen;
  listen_->id_.flags_ |= PortIdentifier::Flags::Toggle;

  /* set mono compat */
  mono_compat_enabled_ = std::make_unique<ControlPort> (
    passthrough ? _ ("Prefader Mono Compat") : _ ("Fader Mono Compat"));
  mono_compat_enabled_->set_owner (this);
  mono_compat_enabled_->id_.sym_ =
    passthrough ? "prefader_mono_compat_enabled" : "fader_mono_compat_enabled";
  mono_compat_enabled_->set_toggled (false, false);
  mono_compat_enabled_->id_.flags2_ |= PortIdentifier::Flags2::FaderMonoCompat;
  mono_compat_enabled_->id_.flags_ |= PortIdentifier::Flags::Toggle;

  /* set swap phase */
  swap_phase_ = create_swap_phase_port (passthrough);
  swap_phase_->set_toggled (false, false);
  swap_phase_->set_owner (this);

  if (
    type == Type::AudioChannel || type == Type::Monitor
    || type == Type::SampleProcessor)
    {
      const char * name = nullptr;
      const char * sym = nullptr;
      if (type == Type::AudioChannel)
        {
          if (passthrough)
            {
              name = _ ("Ch Pre-Fader in");
              sym = "ch_prefader_in";
            }
          else
            {
              name = _ ("Ch Fader in");
              sym = "ch_fader_in";
            }
        }
      else if (type == Type::SampleProcessor)
        {
          name = _ ("Sample Processor Fader in");
          sym = "sample_processor_fader_in";
        }
      else
        {
          name = _ ("Monitor Fader in");
          sym = "monitor_fader_in";
        }

      /* stereo in */
      stereo_in_ = std::make_unique<StereoPorts> (true, name, sym);

      /* set proper owner */
      stereo_in_->set_owner (this);

      if (type == Type::AudioChannel)
        {
          if (passthrough)
            {
              name = _ ("Ch Pre-Fader out");
              sym = "ch_prefader_out";
            }
          else
            {
              name = _ ("Ch Fader out");
              sym = "ch_fader_out";
            }
        }
      else if (type == Type::SampleProcessor)
        {
          name = _ ("Sample Processor Fader out");
          sym = "sample_processor_fader_out";
        }
      else
        {
          name = _ ("Monitor Fader out");
          sym = "monitor_fader_out";
        }

      /* stereo out */
      stereo_out_ = std::make_unique<StereoPorts> (false, name, sym);

      /* set proper owner */
      stereo_out_->set_owner (this);
    }

  if (type == Type::MidiChannel)
    {
      /* MIDI in */
      const char * name = nullptr;
      const char * sym = nullptr;
      if (passthrough)
        {
          name = _ ("Ch MIDI Pre-Fader in");
          sym = "ch_midi_prefader_in";
        }
      else
        {
          name = _ ("Ch MIDI Fader in");
          sym = "ch_midi_fader_in";
        }
      midi_in_ = std::make_unique<MidiPort> (name, PortFlow::Input);
      midi_in_->set_owner (this);
      midi_in_->id_.sym_ = sym;

      /* MIDI out */
      if (passthrough)
        {
          name = _ ("Ch MIDI Pre-Fader out");
          sym = "ch_midi_prefader_out";
        }
      else
        {
          name = _ ("Ch MIDI Fader out");
          sym = "ch_midi_fader_out";
        }
      midi_out_ = std::make_unique<MidiPort> (name, PortFlow::Output);
      midi_out_->set_owner (this);
      midi_out_->id_.sym_ = sym;
    }
}

Fader *
Fader::find_from_port_identifier (const PortIdentifier &id)
{
  auto flag2 = id.flags2_;
  auto tr = TRACKLIST->find_track_by_name_hash (id.track_name_hash_);
  if (
    !tr
    && ENUM_BITSET_TEST (
      PortIdentifier::Flags2, flag2,
      PortIdentifier::Flags2::SampleProcessorTrack))
    {
      tr = SAMPLE_PROCESSOR->tracklist_->find_track_by_name_hash (
        id.track_name_hash_);
    }
  if (ENUM_BITSET_TEST (
        PortIdentifier::Flags2, flag2, PortIdentifier::Flags2::MonitorFader))
    return MONITOR_FADER.get ();
  else if (
    ENUM_BITSET_TEST (
      PortIdentifier::Flags2, flag2,
      PortIdentifier::Flags2::SampleProcessorFader))
    return SAMPLE_PROCESSOR->fader_.get ();
  else if (
    ENUM_BITSET_TEST (
      PortIdentifier::Flags2, flag2, PortIdentifier::Flags2::Prefader))
    {
      auto channel_track = dynamic_cast<ChannelTrack *> (tr);
      z_return_val_if_fail (channel_track, nullptr);
      return channel_track->channel_->prefader_.get ();
    }
  else if (
    ENUM_BITSET_TEST (
      PortIdentifier::Flags2, flag2, PortIdentifier::Flags2::Postfader))
    {
      auto channel_track = dynamic_cast<ChannelTrack *> (tr);
      z_return_val_if_fail (channel_track, nullptr);
      return channel_track->channel_->fader_.get ();
    }

  z_return_val_if_reached (nullptr);
}

void
Fader::set_muted (bool mute, bool fire_events)
{
  auto track = get_track ();
  z_return_if_fail (track);

  mute_->set_toggled (mute, fire_events);

  if (fire_events)
    {
      EVENTS_PUSH (EventType::ET_TRACK_STATE_CHANGED, track);
    }
}

bool
Fader::get_implied_soloed () const
{
  /* only check channel faders */
  if (
    (type_ != Type::AudioChannel && type_ != Type::MidiChannel) || passthrough_
    || solo_->is_toggled ())
    {
      return false;
    }

  auto track = get_track ();
  z_return_val_if_fail (track, false);

  /* check parents */
  auto out_track = track;
  do
    {
      if (out_track->has_channel ())
        {
          auto channel_track = dynamic_cast<ChannelTrack *> (out_track);
          out_track = channel_track->channel_->get_output_track ();
          if (out_track && out_track->get_soloed ())
            {
              return true;
            }
        }
      else
        {
          out_track = nullptr;
        }
    }
  while (out_track);

  /* check children */
  if (track->can_be_group_target ())
    {
      auto group_target = dynamic_cast<GroupTargetTrack *> (track);
      for (const auto &child_hash : group_target->children_)
        {
          auto child_track = TRACKLIST->find_track_by_name_hash (child_hash);
          if (
            child_track
            && (child_track->get_soloed () || child_track->get_implied_soloed ()))
            {
              return true;
            }
        }
    }

  return false;
}

void
Fader::set_soloed (bool solo, bool fire_events)
{
  auto track = get_track ();
  z_return_if_fail (track);

  solo_->set_toggled (solo, fire_events);

  if (fire_events)
    {
      EVENTS_PUSH (EventType::ET_TRACK_STATE_CHANGED, track);
    }
}

bool
Fader::is_in_active_project () const
{
  return (track_ && track_->is_in_active_project ())
         || (sample_processor_ && sample_processor_->is_in_active_project ())
         || (control_room_ && control_room_->is_in_active_project ());
}

void
Fader::set_listened (bool listen, bool fire_events)
{
  auto track = get_track ();
  z_return_if_fail (track);

  listen_->set_toggled (listen, fire_events);

  if (fire_events)
    {
      EVENTS_PUSH (EventType::ET_TRACK_STATE_CHANGED, track);
    }
}

void
Fader::update_volume_and_fader_val ()
{
  volume_ = math_amp_to_dbfs (amp_->control_);
  fader_val_ = math_get_fader_val_from_amp (amp_->control_);
}

void
Fader::set_amp (float amp)
{
  amp_->set_control_value (amp, false, false);
  update_volume_and_fader_val ();
}

void
Fader::set_amp_with_action (float amp_from, float amp_to, bool skip_if_equal)
{
  auto track = get_track ();
  bool is_equal = math_floats_equal_epsilon (amp_from, amp_to, 0.0001f);
  if (!skip_if_equal || !is_equal)
    {
      try
        {
          UNDO_MANAGER->perform (std::make_unique<SingleTrackFloatAction> (
            TracklistSelectionsAction::EditType::Volume, track, amp_from,
            amp_to, false));
        }
      catch (const ZrythmException &e)
        {
          e.handle (_ ("Failed to change volume"));
        }
    }
}

void
Fader::add_amp (sample_t amp)
{
  float fader_amp = get_amp ();
  fader_amp = std::clamp<float> (fader_amp + amp, amp_->minf_, amp_->maxf_);
  set_amp (fader_amp);
  update_volume_and_fader_val ();
}

void
Fader::set_midi_mode (MidiFaderMode mode, bool with_action, bool fire_events)
{
  if (with_action)
    {
      auto track = get_track ();
      z_return_if_fail (track);

      try
        {
          UNDO_MANAGER->perform (std::make_unique<SingleTrackIntAction> (
            TracklistSelectionsAction::EditType::MidiFaderMode, track,
            static_cast<int> (mode), false));
        }
      catch (const ZrythmException &e)
        {
          e.handle (_ ("Failed to set MIDI mode"));
        }
    }
  else
    {
      midi_mode_ = mode;
    }

  if (fire_events)
    {
      // TODO: Implement event firing
    }
}

void
Fader::set_mono_compat_enabled (bool enabled, bool fire_events)
{
  mono_compat_enabled_->set_toggled (enabled, fire_events);

  if (type_ == Type::AudioChannel || type_ == Type::MidiChannel)
    {
      auto track = get_track ();
      z_return_if_fail (track);
      if (fire_events)
        {
          EVENTS_PUSH (EventType::ET_TRACK_STATE_CHANGED, track);
        }
    }
}

void
Fader::set_swap_phase (bool enabled, bool fire_events)
{
  swap_phase_->set_toggled (enabled, fire_events);

  if (type_ == Type::AudioChannel || type_ == Type::MidiChannel)
    {
      auto track = get_track ();
      z_return_if_fail (track);
      if (fire_events)
        {
          EVENTS_PUSH (EventType::ET_TRACK_STATE_CHANGED, track);
        }
    }
}

void
Fader::set_fader_val (float fader_val)
{
  fader_val_ = fader_val;
  float fader_amp = math_get_amp_val_from_fader (fader_val);
  fader_amp = std::clamp<float> (fader_amp, amp_->minf_, amp_->maxf_);
  set_amp (fader_amp);
  volume_ = math_amp_to_dbfs (fader_amp);

  if (this == MONITOR_FADER.get ())
    {
      Glib::RefPtr<Gio::Settings> settings =
        Gio::Settings::create (S_MONITOR_SCHEMA);
      settings->set_double ("monitor-vol", static_cast<double> (fader_amp));
    }
  else if (this == CONTROL_ROOM->mute_fader_.get ())
    {
      Glib::RefPtr<Gio::Settings> settings =
        Gio::Settings::create (S_MONITOR_SCHEMA);
      settings->set_double ("mute-vol", static_cast<double> (fader_amp));
    }
  else if (this == CONTROL_ROOM->listen_fader_.get ())
    {
      Glib::RefPtr<Gio::Settings> settings =
        Gio::Settings::create (S_MONITOR_SCHEMA);
      settings->set_double ("listen-vol", static_cast<double> (fader_amp));
    }
  else if (this == CONTROL_ROOM->dim_fader_.get ())
    {
      Glib::RefPtr<Gio::Settings> settings =
        Gio::Settings::create (S_MONITOR_SCHEMA);
      settings->set_double ("dim-vol", static_cast<double> (fader_amp));
    }
}

void
Fader::set_fader_val_with_action_from_db (const std::string &str)
{
  bool  is_valid = false;
  float val;
  if (math_is_string_valid_float (str.c_str (), &val))
    {
      if (val <= 6.f)
        {
          is_valid = true;
        }
    }

  if (is_valid)
    {
      set_amp_with_action (get_amp (), math_dbfs_to_amp (val), true);
    }
  else
    {
      ui_show_error_message (_ ("Invalid Value"), _ ("Invalid value"));
    }
}

Channel *
Fader::get_channel () const
{
  auto track = dynamic_cast<ChannelTrack *> (get_track ());
  z_return_val_if_fail (track, nullptr);

  return track->get_channel ().get ();
}

Track *
Fader::get_track () const
{
  z_return_val_if_fail (track_, nullptr);
  return track_;
}

void
Fader::clear_buffers ()
{
  switch (type_)
    {
    case Type::AudioChannel:
    case Type::Monitor:
    case Type::SampleProcessor:
      stereo_in_->clear_buffer (*AUDIO_ENGINE);
      stereo_out_->clear_buffer (*AUDIO_ENGINE);
      break;
    case Type::MidiChannel:
      midi_in_->clear_buffer (*AUDIO_ENGINE);
      midi_out_->clear_buffer (*AUDIO_ENGINE);
      break;
    default:
      break;
    }
}

void
Fader::disconnect_all ()
{
  switch (type_)
    {
    case Type::AudioChannel:
    case Type::Monitor:
    case Type::SampleProcessor:
      stereo_in_->get_l ().disconnect_all ();
      stereo_in_->get_r ().disconnect_all ();
      stereo_out_->get_l ().disconnect_all ();
      stereo_out_->get_r ().disconnect_all ();
      break;
    case Type::MidiChannel:
      midi_in_->disconnect_all ();
      midi_out_->disconnect_all ();
      break;
    default:
      break;
    }

  amp_->disconnect_all ();
  balance_->disconnect_all ();
  mute_->disconnect_all ();
  solo_->disconnect_all ();
  listen_->disconnect_all ();
  mono_compat_enabled_->disconnect_all ();
  swap_phase_->disconnect_all ();
}

void
Fader::copy_values (const Fader &other)
{
  volume_ = other.volume_;
  phase_ = other.phase_;
  fader_val_ = other.fader_val_;
  amp_->control_ = other.amp_->control_;
  balance_->control_ = other.balance_->control_;
  mute_->control_ = other.mute_->control_;
  solo_->control_ = other.solo_->control_;
  listen_->control_ = other.listen_->control_;
  mono_compat_enabled_->control_ = other.mono_compat_enabled_->control_;
  swap_phase_->control_ = other.swap_phase_->control_;
}

int
Fader::fade_frames_for_type (Type type)
{
  return type == Type::Monitor
           ? FADER_DEFAULT_FADE_FRAMES
           : FADER_DEFAULT_FADE_FRAMES_SHORT;
}

/**
 * Process the Fader.
 */
void
Fader::process (const EngineProcessTimeInfo time_nfo)
{
  if (ZRYTHM_TESTING)
    {
#if 0
      z_debug (
        "g_start %ld, start frame {}, nframes {}", time_nfo->g_start_frame_w_offset,
        time_nfo->local_offset, time_nfo->nframes);
#endif
    }

  Track * track = nullptr;
  if (type_ == Type::AudioChannel)
    {
      track = get_track ();
      z_return_if_fail (track);
    }

  const int default_fade_frames = fade_frames_for_type (type_);

  bool effectively_muted = false;
  if (!passthrough_)
    {
      /* muted if any of the following is true:
       * 1. muted
       * 2. other track(s) is soloed and this isn't
       * 3. bounce mode and the track is set to BOUNCE_OFF */
      effectively_muted =
        get_muted()
        ||
        ((type_ == Type::AudioChannel || type_ == Type::MidiChannel)
         && TRACKLIST->has_soloed() && !get_soloed() && !get_implied_soloed()
         && track != P_MASTER_TRACK)
        ||
        (AUDIO_ENGINE->bounce_mode_ == BounceMode::BOUNCE_ON
         &&
         (type_ == Type::AudioChannel
          || type_ == Type::MidiChannel)
         && track
         && !track->is_master()
         && !track->bounce_);

#if 0
      if (ZRYTHM_TESTING && track &&
          (self->type == Fader::Type::FADER_TYPE_AUDIO_CHANNEL ||
           self->type == Fader::Type::FADER_TYPE_MIDI_CHANNEL))
        {
          z_debug ("{} soloed {} implied soloed {} effectively muted {}",
            track->name, fader_get_soloed (self),
            fader_get_implied_soloed (self),
            effectively_muted);
        }
#endif
    }

  if (
    type_ == Type::AudioChannel || type_ == Type::Monitor
    || type_ == Type::SampleProcessor)
    {
      /* copy the input to output */
      dsp_copy (
        &stereo_out_->get_l ().buf_[time_nfo.local_offset_],
        &stereo_in_->get_l ().buf_[time_nfo.local_offset_], time_nfo.nframes_);
      dsp_copy (
        &stereo_out_->get_r ().buf_[time_nfo.local_offset_],
        &stereo_in_->get_r ().buf_[time_nfo.local_offset_], time_nfo.nframes_);

      /* if prefader */
      if (passthrough_)
        {

          /* if track frozen and transport is rolling */
          if (track && track->frozen_ && TRANSPORT->is_rolling ())
            {
#if 0
              /* get audio from clip */
              AudioClip * clip =
                audio_pool_get_clip (
                  AUDIO_POOL, track->pool_id_);
              /* FIXME this is wrong - need to
               * also calculate the offset in the
               * clip */
              stereo_ports_fill_from_clip (
                self->stereo_out, clip,
                time_nfo->g_start_frame_w_offset,
                time_nfo->local_offset,
                time_nfo->nframes);
#endif
            }
        }
      else /* not prefader */
        {
          /* if monitor */
          float mute_amp;
          if (type_ == Fader::Type::Monitor)
            {
              mute_amp = AUDIO_ENGINE->denormal_prevention_val_;
              float dim_amp = CONTROL_ROOM->dim_fader_->get_amp ();

              /* if have listened tracks */
              if (TRACKLIST->has_listened ())
                {
                  /* dim signal */
                  dsp_mul_k2 (
                    &stereo_out_->get_l ().buf_[time_nfo.local_offset_],
                    dim_amp, time_nfo.nframes_);
                  dsp_mul_k2 (
                    &stereo_out_->get_r ().buf_[time_nfo.local_offset_],
                    dim_amp, time_nfo.nframes_);

                  /* add listened signal */
                  /* TODO add "listen" buffer on fader struct and add listened
                   * tracks to it during processing instead of looping here */
                  float listen_amp = CONTROL_ROOM->listen_fader_->get_amp ();
                  for (auto &t : TRACKLIST->tracks_)
                    {
                      if (
                        t->has_channel ()
                        && t->out_signal_type_ == PortType::Audio
                        && t->get_listened ())
                        {
                          auto channel_track =
                            dynamic_cast<ChannelTrack *> (t.get ());
                          auto f = channel_track->get_fader (true);
                          dsp_mix2 (
                            &stereo_out_->get_l ().buf_[time_nfo.local_offset_],
                            &f->stereo_out_->get_l ()
                               .buf_[time_nfo.local_offset_],
                            1.f, listen_amp, time_nfo.nframes_);
                          dsp_mix2 (
                            &stereo_out_->get_r ().buf_[time_nfo.local_offset_],
                            &f->stereo_out_->get_r ()
                               .buf_[time_nfo.local_offset_],
                            1.f, listen_amp, time_nfo.nframes_);
                        }
                    }
                } /* endif have listened tracks */

              /* apply dim if enabled */
              if (CONTROL_ROOM->dim_output_)
                {
                  dsp_mul_k2 (
                    &stereo_out_->get_l ().buf_[time_nfo.local_offset_],
                    dim_amp, time_nfo.nframes_);
                  dsp_mul_k2 (
                    &stereo_out_->get_r ().buf_[time_nfo.local_offset_],
                    dim_amp, time_nfo.nframes_);
                }
            } /* endif monitor fader */
          else
            {
              mute_amp = CONTROL_ROOM->mute_fader_->get_amp ();

              /* add fade if changed from muted to non-muted or
               * vice versa */
              if (effectively_muted && !was_effectively_muted_)
                {
                  fade_out_samples_.store (default_fade_frames);
                  fading_out_.store (true);
                }
              else if (!effectively_muted && was_effectively_muted_)
                {

                  fading_out_.store (false);
                  fade_in_samples_.store (default_fade_frames);
                }
            }

          /* handle fade in */
          int fade_in_samples = fade_in_samples_.load ();
          if (fade_in_samples > 0) [[unlikely]]
            {
              z_return_if_fail_cmp (default_fade_frames, >=, fade_in_samples);
#if 0
              z_debug (
                "fading in %d samples", fade_in_samples);
#endif
              dsp_linear_fade_in_from (
                &stereo_out_->get_l ().buf_[time_nfo.local_offset_],
                default_fade_frames - fade_in_samples, default_fade_frames,
                time_nfo.nframes_, mute_amp);
              dsp_linear_fade_in_from (
                &stereo_out_->get_r ().buf_[time_nfo.local_offset_],
                default_fade_frames - fade_in_samples, default_fade_frames,
                time_nfo.nframes_, mute_amp);
              fade_in_samples -= (int) time_nfo.nframes_;
              fade_in_samples = MAX (fade_in_samples, 0);
              fade_in_samples_.store (fade_in_samples);
            }

          /* handle fade out */
          size_t faded_out_frames = 0;
          if (fading_out_.load ()) [[unlikely]]
            {
              int fade_out_samples = fade_out_samples_.load ();
              int samples_to_process =
                MAX (0, MIN (fade_out_samples, (int) time_nfo.nframes_));
              if (fade_out_samples > 0)
                {
                  z_return_if_fail_cmp (
                    default_fade_frames, >=, fade_out_samples);

#if 0
                  z_debug (
                    "fading out %d frames",
                    samples_to_process);
#endif
                  dsp_linear_fade_out_to (
                    &stereo_out_->get_l ().buf_[time_nfo.local_offset_],
                    default_fade_frames - fade_out_samples, default_fade_frames,
                    (size_t) samples_to_process, mute_amp);
                  dsp_linear_fade_out_to (
                    &stereo_out_->get_r ().buf_[time_nfo.local_offset_],
                    default_fade_frames - fade_out_samples, default_fade_frames,
                    (size_t) samples_to_process, mute_amp);
                  fade_out_samples -= samples_to_process;
                  faded_out_frames += (size_t) samples_to_process;
                  fade_out_samples_.store (fade_out_samples);
                }

              /* if still fading out and have no more fade out samples, silence */
              if (fade_out_samples == 0)
                {
                  size_t remaining_frames =
                    time_nfo.nframes_ - (size_t) samples_to_process;
#if 0
                  z_debug (
                    "silence for remaining %zu frames", remaining_frames);
#endif
                  if (remaining_frames > 0)
                    {
                      dsp_mul_k2 (
                        &stereo_out_->get_l ()
                           .buf_[time_nfo.local_offset_ + faded_out_frames],
                        mute_amp, remaining_frames);
                      dsp_mul_k2 (
                        &stereo_out_->get_r ()
                           .buf_[time_nfo.local_offset_ + faded_out_frames],
                        mute_amp, remaining_frames);
                      faded_out_frames += (size_t) remaining_frames;
                    }
                }
            }

          float pan = balance_->get_control_value (false);
          float amp = amp_->get_control_value (false);

          float calc_l, calc_r;
          balance_control_get_calc_lr (
            BalanceControlAlgorithm::BALANCE_CONTROL_ALGORITHM_LINEAR, pan,
            &calc_l, &calc_r);

          /* apply fader and pan */
          dsp_mul_k2 (
            &stereo_out_->get_l ().buf_[time_nfo.local_offset_], amp * calc_l,
            time_nfo.nframes_);
          dsp_mul_k2 (
            &stereo_out_->get_r ().buf_[time_nfo.local_offset_], amp * calc_r,
            time_nfo.nframes_);

          /* make mono if mono compat enabled */
          if (mono_compat_enabled_->is_toggled ())
            {
              dsp_make_mono (
                &stereo_out_->get_l ().buf_[time_nfo.local_offset_],
                &stereo_out_->get_r ().buf_[time_nfo.local_offset_],
                time_nfo.nframes_, false);
            }

          /* swap phase if need */
          if (swap_phase_->is_toggled ())
            {
              dsp_mul_k2 (
                &stereo_out_->get_l ().buf_[time_nfo.local_offset_], -1.f,
                time_nfo.nframes_);
              dsp_mul_k2 (
                &stereo_out_->get_r ().buf_[time_nfo.local_offset_], -1.f,
                time_nfo.nframes_);
            }

          int fade_out_samples = fade_out_samples_.load ();
          if (
            effectively_muted && fade_out_samples == 0
            && time_nfo.nframes_ - faded_out_frames > 0)
            {
#if 0
              z_debug (
                "muting %zu frames",
                time_nfo->nframes - faded_out_frames);
#endif
              /* apply mute level */
              if (mute_amp < 0.00001f)
                {
                  dsp_fill (
                    &stereo_out_->get_l ()
                       .buf_[time_nfo.local_offset_ + faded_out_frames],
                    AUDIO_ENGINE->denormal_prevention_val_,
                    time_nfo.nframes_ - faded_out_frames);
                  dsp_fill (
                    &stereo_out_->get_r ()
                       .buf_[time_nfo.local_offset_ + faded_out_frames],
                    AUDIO_ENGINE->denormal_prevention_val_,
                    time_nfo.nframes_ - faded_out_frames);
                }
              else
                {
                  dsp_mul_k2 (
                    &stereo_out_->get_l ().buf_[time_nfo.local_offset_],
                    mute_amp, time_nfo.nframes_ - faded_out_frames);
                  dsp_mul_k2 (
                    &stereo_out_->get_r ()
                       .buf_[time_nfo.local_offset_ + faded_out_frames],
                    mute_amp, time_nfo.nframes_ - faded_out_frames);
                }
            }

          /* if master or monitor or sample processor, hard limit the output */
          if (
            (type_ == Type::AudioChannel && track && track->is_master ())
            || type_ == Type::Monitor || type_ == Type::SampleProcessor)
            {
              dsp_limit1 (
                &stereo_out_->get_l ().buf_[time_nfo.local_offset_], -2.f, 2.f,
                time_nfo.nframes_);
              dsp_limit1 (
                &stereo_out_->get_r ().buf_[time_nfo.local_offset_], -2.f, 2.f,
                time_nfo.nframes_);
            }
        } /* fi not prefader */
    } /* fi monitor/audio fader */
  else if (type_ == Type::MidiChannel)
    {
      if (!effectively_muted)
        {
          midi_out_->midi_events_.active_events_.append (
            midi_in_->midi_events_.active_events_, time_nfo.local_offset_,
            time_nfo.nframes_);

          /* if not prefader, also apply volume changes */
          if (!passthrough_)
            {
              for (auto &ev : midi_out_->midi_events_.active_events_)
                {
                  if (
                    midi_mode_ == MidiFaderMode::MIDI_FADER_MODE_VEL_MULTIPLIER
                    && midi_is_note_on (ev.raw_buffer_.data ()))
                    {
                      midi_byte_t prev_vel =
                        midi_get_velocity (ev.raw_buffer_.data ());
                      auto new_vel =
                        (midi_byte_t) ((float) prev_vel * amp_->control_);
                      ev.set_velocity (
                        std::min (new_vel, static_cast<midi_byte_t> (127)));
                    }
                }

              if (
                midi_mode_ == MidiFaderMode::MIDI_FADER_MODE_CC_VOLUME
                && !math_floats_equal (last_cc_volume_, amp_->control_))
                {
                  /* TODO add volume event on each channel */
                }
            }
        }
    }

  was_effectively_muted_ = effectively_muted;

  if (ZRYTHM_TESTING)
    {
#if 0
      z_debug ("{}: done", __func__);
#endif
    }
}

void
Fader::init_after_cloning (const Fader &other)
{
  type_ = other.type_;
  volume_ = other.volume_;
  amp_ = other.amp_->clone_unique ();
  phase_ = other.phase_;
  balance_ = other.balance_->clone_unique ();
  mute_ = other.mute_->clone_unique ();
  solo_ = other.solo_->clone_unique ();
  listen_ = other.listen_->clone_unique ();
  mono_compat_enabled_ = other.mono_compat_enabled_->clone_unique ();
  swap_phase_ = other.swap_phase_->clone_unique ();
  if (other.midi_in_)
    midi_in_ = other.midi_in_->clone_unique ();
  if (other.midi_out_)
    midi_out_ = other.midi_out_->clone_unique ();
  if (other.stereo_in_)
    stereo_in_ = other.stereo_in_->clone_unique ();
  if (other.stereo_out_)
    stereo_out_ = other.stereo_out_->clone_unique ();
  midi_mode_ = other.midi_mode_;
  passthrough_ = other.passthrough_;
}