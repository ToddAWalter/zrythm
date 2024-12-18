// SPDX-FileCopyrightText: © 2018-2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "gui/backend/backend/project.h"
#include "gui/backend/backend/zrythm.h"
#include "gui/dsp/audio_port.h"
#include "gui/dsp/channel_track.h"
#include "gui/dsp/engine.h"
#include "gui/dsp/master_track.h"
#include "gui/dsp/tracklist.h"
#include "utils/dsp.h"

#include <fmt/format.h>

AudioPort::AudioPort () : AudioPort ("", PortFlow::Input) { }

AudioPort::AudioPort (std::string label, PortFlow flow)
    : Port (label, PortType::Audio, flow, -1.f, 1.f, 0.f)
{
}

void
AudioPort::init_after_cloning (const AudioPort &other)
{
  Port::copy_members_from (other);
}

void
AudioPort::allocate_bufs ()
{
  audio_ring_ = std::make_unique<RingBuffer<float>> (AUDIO_RING_SIZE);

  size_t max = std::max (AUDIO_ENGINE->block_length_, 1u);
  buf_.resize (max);
  last_buf_sz_ = max;
}

void
AudioPort::clear_buffer (AudioEngine &engine)
{
  utils::float_ranges::fill (
    buf_.data (), DENORMAL_PREVENTION_VAL (&engine), engine.block_length_);
}

void
AudioPort::sum_data_from_dummy (
  const nframes_t start_frame,
  const nframes_t nframes)
{
  if (
    id_->owner_type_ == PortIdentifier::OwnerType::AudioEngine
    || id_->flow_ != PortFlow::Input || id_->type_ != PortType::Audio
    || AUDIO_ENGINE->audio_backend_ != AudioBackend::AUDIO_BACKEND_DUMMY
    || AUDIO_ENGINE->midi_backend_ != MidiBackend::MIDI_BACKEND_DUMMY)
    return;

  if (AUDIO_ENGINE->dummy_input_)
    {
      Port * port = nullptr;
      if (ENUM_BITSET_TEST (
            PortIdentifier::Flags, id_->flags_, PortIdentifier::Flags::StereoL))
        {
          port = &AUDIO_ENGINE->dummy_input_->get_l ();
        }
      else if (
        ENUM_BITSET_TEST (
          PortIdentifier::Flags, id_->flags_, PortIdentifier::Flags::StereoR))
        {
          port = &AUDIO_ENGINE->dummy_input_->get_r ();
        }

      if (port)
        {
          utils::float_ranges::add2 (
            &buf_[start_frame], &port->buf_[start_frame], nframes);
        }
    }
}

bool
AudioPort::has_sound () const
{
  z_return_val_if_fail (
    this->buf_.size () >= AUDIO_ENGINE->block_length_, false);
  for (nframes_t i = 0; i < AUDIO_ENGINE->block_length_; i++)
    {
      if (fabsf (this->buf_[i]) > 0.0000001f)
        {
          return true;
        }
    }
  return false;
}

void
AudioPort::process (const EngineProcessTimeInfo time_nfo, const bool noroll)
{
  if (noroll)
    {
      utils::float_ranges::fill (
        &buf_.data ()[time_nfo.local_offset_],
        DENORMAL_PREVENTION_VAL (AUDIO_ENGINE), time_nfo.nframes_);
      return;
    }

  const auto &id = id_;
  const auto  owner_type = id->owner_type_;

  const bool is_stereo = is_stereo_port ();

  if (is_input () && owner_->should_sum_data_from_backend ())
    {
      if (backend_ && backend_->is_exposed ())
        {
          backend_->sum_data (
            buf_.data (), { time_nfo.local_offset_, time_nfo.nframes_ });
        }
      else if (AUDIO_ENGINE->audio_backend_ == AudioBackend::AUDIO_BACKEND_DUMMY)
        {
          // TODO: make this a PortBackend implementation too, then it will get
          // handled by the above code
          sum_data_from_dummy (time_nfo.local_offset_, time_nfo.nframes_);
        }
    }

  for (size_t k = 0; k < srcs_.size (); k++)
    {
      const auto * src_port = srcs_[k];
      const auto  &conn = src_connections_[k];
      if (!conn->enabled_)
        continue;

      const float multiplier = conn->multiplier_;

      /* sum the signals */
      if (utils::math::floats_near (multiplier, 1.f, 0.00001f)) [[likely]]
        {
          utils::float_ranges::add2 (
            &buf_[time_nfo.local_offset_],
            &src_port->buf_[time_nfo.local_offset_], time_nfo.nframes_);
        }
      else
        {
          utils::float_ranges::mix_product (
            &buf_[time_nfo.local_offset_],
            &src_port->buf_[time_nfo.local_offset_], multiplier,
            time_nfo.nframes_);
        }

      if (owner_type == PortIdentifier::OwnerType::Fader)
        {
          constexpr float minf = -2.f;
          constexpr float maxf = 2.f;
          float           abs_peak = utils::float_ranges::abs_max (
            &buf_[time_nfo.local_offset_], time_nfo.nframes_);
          if (abs_peak > maxf)
            {
              /* this limiting wastes around 50% of port processing so only
               * do it on CV connections and faders if they exceed maxf */
              utils::float_ranges::clip (
                &buf_[time_nfo.local_offset_], minf, maxf, time_nfo.nframes_);
            }
        }
    }

  if (is_output () && backend_ && backend_->is_exposed ())
    {
      backend_->send_data (
        buf_.data (), { time_nfo.local_offset_, time_nfo.nframes_ });
    }

  if (time_nfo.local_offset_ + time_nfo.nframes_ == AUDIO_ENGINE->block_length_)
    {
      // z_debug ("writing to ring for {}", get_label ());
      audio_ring_->force_write_multiple (
        buf_.data (), AUDIO_ENGINE->block_length_);
    }

  /* if track output (to be shown on mixer) */
  if (
    owner_type == PortIdentifier::OwnerType::Channel && is_stereo
    && is_output ())
    {
      /* calculate meter values */
      /* reset peak if needed */
      auto time_now = Zrythm::getInstance ()->get_monotonic_time_usecs ();
      if (time_now - peak_timestamp_ > TIME_TO_RESET_PEAK)
        peak_ = -1.f;

      bool changed = utils::float_ranges::abs_max_with_existing_peak (
        &buf_[time_nfo.local_offset_], &peak_, time_nfo.nframes_);
      if (changed)
        {
          peak_timestamp_ = Zrythm::getInstance ()->get_monotonic_time_usecs ();
        }
    }

  /* if bouncing tracks directly to master (e.g., when bouncing the track on
   * its own without parents), clear master input */
  if (AUDIO_ENGINE->bounce_mode_ > BounceMode::BOUNCE_OFF && !AUDIO_ENGINE->bounce_with_parents_ && (this == &P_MASTER_TRACK->processor_->stereo_in_->get_l () || this == &P_MASTER_TRACK->processor_->stereo_in_->get_r ())) [[unlikely]]
    {
      utils::float_ranges::fill (
        &buf_[time_nfo.local_offset_], AUDIO_ENGINE->denormal_prevention_val_,
        time_nfo.nframes_);
    }

  /* if bouncing directly to master (e.g., when bouncing a track on
   * its own without parents), add the buffer to master output */
  if (
    AUDIO_ENGINE->bounce_mode_ > BounceMode::BOUNCE_OFF && is_stereo
    && is_output ()
    && owner_->should_bounce_to_master (AUDIO_ENGINE->bounce_step_)) [[unlikely]]
    {
      auto &dest =
        ENUM_BITSET_TEST (
          PortIdentifier::Flags, id_->flags_, PortIdentifier::Flags::StereoL)
          ? P_MASTER_TRACK->channel_->stereo_out_->get_l ()
          : P_MASTER_TRACK->channel_->stereo_out_->get_r ();
      utils::float_ranges::add2 (
        &dest.buf_[time_nfo.local_offset_], &this->buf_[time_nfo.local_offset_],
        time_nfo.nframes_);
    }
}

void
AudioPort::apply_pan (
  float                     pan,
  zrythm::dsp::PanLaw       pan_law,
  zrythm::dsp::PanAlgorithm pan_algo,
  nframes_t                 start_frame,
  const nframes_t           nframes)
{
  auto [calc_r, calc_l] = dsp::calculate_panning (pan_law, pan_algo, pan);

  /* if stereo R */
  if (ENUM_BITSET_TEST (
        PortIdentifier::Flags, id_->flags_, PortIdentifier::Flags::StereoR))
    {
      utils::float_ranges::mul_k2 (&buf_[start_frame], calc_r, nframes);
    }
  /* else if stereo L */
  else
    {
      utils::float_ranges::mul_k2 (&buf_[start_frame], calc_l, nframes);
    }
}

void
AudioPort::apply_fader (float amp, nframes_t start_frame, const nframes_t nframes)
{
  utils::float_ranges::mul_k2 (&buf_[start_frame], amp, nframes);
}

StereoPorts::StereoPorts (bool input, std::string name, std::string symbol)
    : StereoPorts (
        AudioPort (
          fmt::format ("{} L", name),
          input ? dsp::PortFlow::Input : dsp::PortFlow::Output),
        AudioPort (
          fmt::format ("{} R", name),
          input ? dsp::PortFlow::Input : dsp::PortFlow::Output))

{
  l_->id_->flags_ |= dsp::PortIdentifier::Flags::StereoL;
  l_->id_->sym_ = fmt::format ("{}_l", symbol);
  r_->id_->flags_ |= dsp::PortIdentifier::Flags::StereoR;
  r_->id_->sym_ = fmt::format ("{}_r", symbol);
}

StereoPorts::StereoPorts (const AudioPort &l, const AudioPort &r)
{
  l_ = l.clone_raw_ptr ();
  r_ = r.clone_raw_ptr ();
  l_->setParent (this);
  r_->setParent (this);
  l_->id_->flags_ |= dsp::PortIdentifier::Flags::StereoL;
  r_->id_->flags_ |= dsp::PortIdentifier::Flags::StereoR;
}

void
StereoPorts::init_after_cloning (const StereoPorts &other)
{
  l_ = other.l_->clone_raw_ptr ();
  r_ = other.r_->clone_raw_ptr ();
  l_->setParent (this);
  r_->setParent (this);
}

void
StereoPorts::disconnect (PortConnectionsManager &mgr)
{
  l_->disconnect_all ();
  r_->disconnect_all ();
}

void
StereoPorts::
  connect_to (PortConnectionsManager &mgr, StereoPorts &dest, bool locked)
{
  mgr.ensure_connect_default (*l_->id_, *dest.l_->id_, locked);
  mgr.ensure_connect_default (*r_->id_, *dest.r_->id_, locked);
}

StereoPorts::~StereoPorts ()
{
  if (PORT_CONNECTIONS_MGR)
    {
      disconnect (*PORT_CONNECTIONS_MGR);
    }
}
