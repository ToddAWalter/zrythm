// SPDX-FileCopyrightText: © 2020-2022, 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "zrythm-test-config.h"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "dsp/master_track.h"
#include "dsp/midi_mapping.h"
#include "project.h"
#include "utils/math.h"
#include "utils/objects.h"
#include "zrythm.h"

#include "tests/helpers/project_helper.h"
#include "tests/helpers/zrythm_helper.h"

TEST_SUITE_BEGIN ("dsp/midi mapping");

TEST_CASE_FIXTURE (ZrythmFixture, "midi mapping")
{
  MidiMappings mappings;

  ExtPort ext_port;
  ext_port.type_ = ExtPort::Type::RtAudio;
  ext_port.full_name_ = "ext port1";
  ext_port.short_name_ = "extport1";

  std::array<midi_byte_t, 3> buf = { 0xB0, 0x07, 121 };
  MIDI_MAPPINGS->bind_device (
    buf, &ext_port, *P_MASTER_TRACK->channel_->fader_->amp_, false);
  ASSERT_SIZE_EQ (MIDI_MAPPINGS->mappings_, 1);

  int size = MIDI_MAPPINGS->get_for_port (
    *P_MASTER_TRACK->channel_->fader_->amp_, nullptr);
  ASSERT_EQ (size, 1);

  MIDI_MAPPINGS->apply (buf.data ());

  test_project_save_and_reload ();

  ASSERT_EQ (
    P_MASTER_TRACK->channel_->fader_->amp_.get (),
    MIDI_MAPPINGS->mappings_[0]->dest_);
}

TEST_SUITE_END;