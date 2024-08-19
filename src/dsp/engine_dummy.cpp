// SPDX-License-Identifier: LicenseRef-ZrythmLicense
// SPDX-FileCopyrightText: © 2019-2021, 2024 Alexandros Theodotou <alex@zrythm.org>

#include "dsp/engine.h"
#include "dsp/engine_dummy.h"
#include "dsp/port.h"
#include "dsp/tempo_track.h"
#include "dsp/tracklist.h"
#include "project.h"
#include "utils/dsp.h"
#include "zrythm.h"
#include "zrythm_app.h"

#include "gtk_wrapper.h"

class DummyEngineThread : public juce::Thread
{
public:
  DummyEngineThread (AudioEngine &engine)
      : juce::Thread ("DummyEngineThread"), engine_ (engine)
  {
  }

  void run () override
  {
    double secs_per_block =
      (double) engine_.block_length_ / engine_.sample_rate_;
    auto sleep_time = (gulong) (secs_per_block * 1000.0 * 1000);

    z_info ("Running dummy audio engine for first time");

#ifdef HAVE_LSP_DSP
    LspDspContextRAII lsp_dsp_context_raii;
#endif

    while (!threadShouldExit ())
      {
        engine_.process (engine_.block_length_);
        std::this_thread::sleep_for (std::chrono::microseconds (sleep_time));
      }
  }

public:
  AudioEngine &engine_;
};

int
engine_dummy_setup (AudioEngine * self)
{
  /* Set audio engine properties */
  self->midi_buf_size_ = 4096;

  if (ZRYTHM_HAVE_UI && zrythm_app->buf_size > 0)
    {
      self->block_length_ = (nframes_t) zrythm_app->buf_size;
    }
  else
    {
      self->block_length_ = 256;
    }

  if (zrythm_app->samplerate > 0)
    {
      self->sample_rate_ = (nframes_t) zrythm_app->samplerate;
    }
  else
    {
      self->sample_rate_ = 44100;
    }

  int beats_per_bar = P_TEMPO_TRACK->get_beats_per_bar ();
  z_warn_if_fail (beats_per_bar >= 1);

  z_info ("Dummy Engine set up [samplerate: {}]", self->sample_rate_);

  return 0;
}

int
engine_dummy_midi_setup (AudioEngine * self)
{
  z_info ("Setting up dummy MIDI engine");

  self->midi_buf_size_ = 4096;

  return 0;
}

int
engine_dummy_activate (AudioEngine * self, bool activate)
{
  if (activate)
    {
      z_info ("activating...");

      int beats_per_bar = P_TEMPO_TRACK->get_beats_per_bar ();
      self->update_frames_per_tick (
        beats_per_bar, P_TEMPO_TRACK->get_current_bpm (), self->sample_rate_,
        true, true, false);

      self->dummy_audio_thread_ = std::make_unique<DummyEngineThread> (*self);
    }
  else
    {
      z_info ("deactivating...");
      self->dummy_audio_thread_->signalThreadShouldExit ();
      self->dummy_audio_thread_->waitForThreadToExit (1'000);
    }

  z_info ("done");

  return 0;
}

void
engine_dummy_tear_down (AudioEngine * self)
{
}
