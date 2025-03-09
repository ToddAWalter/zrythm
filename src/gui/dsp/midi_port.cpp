// SPDX-FileCopyrightText: © 2018-2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "zrythm-config.h"

#include "gui/backend/backend/project.h"
#include "gui/backend/backend/zrythm.h"
#include "gui/dsp/engine.h"
#include "gui/dsp/midi_port.h"
#include "gui/dsp/piano_roll_track.h"
#include "utils/midi.h"
#include "utils/rt_thread_id.h"

MidiPort::MidiPort () = default;

MidiPort::MidiPort (std::string label, PortFlow flow)
    : Port (label, PortType::Event, flow, 0.f, 1.f, 0.f)
{
}

MidiPort::~MidiPort () = default;

void
MidiPort::init_after_cloning (
  const MidiPort &other,
  ObjectCloneType clone_type)
{
  Port::copy_members_from (other, clone_type);
}

void
MidiPort::allocate_bufs ()
{
  midi_ring_ = std::make_unique<RingBuffer<MidiEvent>> (11);
}

void
MidiPort::clear_buffer (AudioEngine &engine)
{
  midi_events_.active_events_.clear ();
  // midi_events_.queued_events_.clear ();
}

void
MidiPort::process (const EngineProcessTimeInfo time_nfo, const bool noroll)
{
  if (noroll)
    return;

  midi_events_.dequeue (time_nfo.local_offset_, time_nfo.nframes_);

  const auto &id = id_;
  const auto  owner_type = id->owner_type_;
  auto &events = midi_events_.active_events_;

  /* if piano roll keys, add the notes to the piano roll "current notes" (to
   * show pressed keys in the UI) */
  if (
    owner_type == PortIdentifier::OwnerType::TrackProcessor && is_output ()
    && events.has_any () && CLIP_EDITOR->has_region_
    && CLIP_EDITOR->region_id_.track_uuid_ == id_->get_track_id ().value ())
    [[unlikely]]
    {
      bool events_processed = false;
      for (auto &ev : events)
        {
          auto &buf = ev.raw_buffer_;
          if (midi_is_note_on (buf.data ()))
            {
              PIANO_ROLL->add_current_note (midi_get_note_number (buf.data ()));
              events_processed = true;
            }
          else if (midi_is_note_off (buf.data ()))
            {
              PIANO_ROLL->remove_current_note (
                midi_get_note_number (buf.data ()));
              events_processed = true;
            }
          else if (midi_is_all_notes_off (buf.data ()))
            {
              PIANO_ROLL->current_notes_.clear ();
              events_processed = true;
            }
        }
      if (events_processed)
        {
          // EVENTS_PUSH (EventType::ET_PIANO_ROLL_KEY_ON_OFF, nullptr);
        }
    }

  if (
    is_input () && backend_ && backend_->is_exposed ()
    && owner_->should_sum_data_from_backend ())
    {
      backend_->sum_data (
        midi_events_, { time_nfo.local_offset_, time_nfo.nframes_ },
        [this] (midi_byte_t channel) {
          return owner_->are_events_on_midi_channel_approved (channel);
        });
    }

  /* set midi capture if hardware */
  if (owner_type == PortIdentifier::OwnerType::HardwareProcessor)
    {
      if (events.has_any ())
        {
          AUDIO_ENGINE->trigger_midi_activity_.store (true);

          /* queue playback if recording and we should record on MIDI input */
          if (
            TRANSPORT->is_recording () && TRANSPORT->isPaused ()
            && TRANSPORT->start_playback_on_midi_input_)
            {
              // EVENTS_PUSH (EventType::ET_TRANSPORT_ROLL_REQUIRED, nullptr);
            }

          /* capture cc if capturing */
          if (AUDIO_ENGINE->capture_cc_.load ()) [[unlikely]]
            {
              auto last_event = events.back ();
              AUDIO_ENGINE->last_cc_captured_ = last_event.raw_buffer_;
            }

          /* send cc to mapped ports */
          for (auto &ev : events)
            {
              MIDI_MAPPINGS->apply (ev.raw_buffer_.data ());
            }
        }
    }

  /* handle MIDI clock */
  if (
    ENUM_BITSET_TEST (id_->flags2_, PortIdentifier::Flags2::MidiClock)
    && is_output ())
    {
      /* continue or start */
      bool start =
        TRANSPORT->isRolling () && !AUDIO_ENGINE->pos_nfo_before_.is_rolling_;
      if (start)
        {
          uint8_t start_msg = MIDI_CLOCK_CONTINUE;
          if (PLAYHEAD.frames_ == 0)
            {
              start_msg = MIDI_CLOCK_START;
            }
          events.add_raw (&start_msg, 1, 0);
        }
      else if (
        !TRANSPORT->isRolling () && AUDIO_ENGINE->pos_nfo_before_.is_rolling_)
        {
          uint8_t stop_msg = MIDI_CLOCK_STOP;
          events.add_raw (&stop_msg, 1, 0);
        }

      /* song position */
      int32_t sixteenth_within_song =
        PLAYHEAD.get_total_sixteenths (false, AUDIO_ENGINE->frames_per_tick_);
      if (
        AUDIO_ENGINE->pos_nfo_at_end_.sixteenth_within_song_
          != AUDIO_ENGINE->pos_nfo_current_.sixteenth_within_song_
        || start)
        {
          /* TODO interpolate */
          events.add_song_pos (sixteenth_within_song, 0);
        }

      /* clock beat */
      if (
        AUDIO_ENGINE->pos_nfo_at_end_.ninetysixth_notes_
        > AUDIO_ENGINE->pos_nfo_current_.ninetysixth_notes_)
        {
          for (
            int32_t i = AUDIO_ENGINE->pos_nfo_current_.ninetysixth_notes_ + 1;
            i <= AUDIO_ENGINE->pos_nfo_at_end_.ninetysixth_notes_; ++i)
            {
              double ninetysixth_ticks =
                i * dsp::Position::TICKS_PER_NINETYSIXTH_NOTE_DBL;
              double      ratio = (ninetysixth_ticks - AUDIO_ENGINE->pos_nfo_current_.playhead_ticks_) / (AUDIO_ENGINE->pos_nfo_at_end_.playhead_ticks_ - AUDIO_ENGINE->pos_nfo_current_.playhead_ticks_);
              auto midi_time = static_cast<midi_time_t> (
                std::floor (ratio * (double) AUDIO_ENGINE->block_length_));
              if (
                midi_time >= time_nfo.local_offset_
                && midi_time < time_nfo.local_offset_ + time_nfo.nframes_)
                {
                  uint8_t beat_msg = MIDI_CLOCK_BEAT;
                  events.add_raw (&beat_msg, 1, midi_time);
#if 0
                  z_debug ("(i = {}) time {} / {}", i, midi_time, time_nfo.local_offset + time_nfo.nframes_);
#endif
                }
            }
        }

      events.sort ();
    }

  /* append data from each source */
  for (size_t k = 0; k < srcs_.size (); k++)
    {
      const auto * src_port = srcs_[k];
      const auto  &conn = src_connections_[k];
      if (!conn->enabled_)
        continue;

      z_return_if_fail (src_port->id_->type_ == PortType::Event);
      const auto * src_midi_port = dynamic_cast<const MidiPort *> (src_port);

      /* if hardware device connected to track processor input, only allow
       * signal to pass if armed and MIDI channel is valid */
      if (
        src_port->id_->owner_type_ == PortIdentifier::OwnerType::HardwareProcessor
        && owner_type == PortIdentifier::OwnerType::TrackProcessor)
        {
          /* skip if not armed */
          if (!owner_->should_sum_data_from_backend ())
            continue;

          for (const auto &src_ev : src_midi_port->midi_events_.active_events_)
            {
              /* only copy events inside the current time range */
              if (
                src_ev.time_ < time_nfo.local_offset_
                || src_ev.time_ >= time_nfo.local_offset_ + time_nfo.nframes_)
                {
                  continue;
                }

              /* only copy events on approved MIDI channels */
              midi_byte_t channel = src_ev.raw_buffer_[0] & 0xf;
              if (owner_->are_events_on_midi_channel_approved (channel))
                {
                  events.push_back (src_ev);
                }
            }
        }
      else
        {
          events.append (
            src_midi_port->midi_events_.active_events_, time_nfo.local_offset_,
            time_nfo.nframes_);
        }
    } /* foreach source */

  if (id->is_output () && backend_ && backend_->is_exposed ())
    {
      backend_->send_data (
        midi_events_, { time_nfo.local_offset_, time_nfo.nframes_ });
    }

  /* send UI notification */
  if (events.has_any ())
    {
      owner_->on_midi_activity (*id_);
    }

  if (time_nfo.local_offset_ + time_nfo.nframes_ == AUDIO_ENGINE->block_length_)
    {
      if (write_ring_buffers_)
        {
          for (auto &ev : events | std::views::reverse)
            {
              if (midi_ring_->write_space () < 1)
                {
                  midi_ring_->skip (1);
                }

              ev.systime_ = Zrythm::getInstance ()->get_monotonic_time_usecs ();
              midi_ring_->write (ev);
              // z_warning ("writing to ring for {}", get_label ());
            }
        }
      else
        {
          if (events.has_any ())
            {
              last_midi_event_time_ =
                Zrythm::getInstance ()->get_monotonic_time_usecs ();
              // z_warning (
              //   "wrote last event time {} for '{}'", last_midi_event_time_,
              //   get_label ());
              has_midi_events_.store (true);
            }
        }
    }
}
