// SPDX-FileCopyrightText: © 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "zrythm-test-config.h"

#include "actions/arranger_selections.h"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "dsp/midi_function.h"
#include "dsp/tracklist.h"

#include "tests/helpers/zrythm_helper.h"

TEST_SUITE_BEGIN ("dsp/midi function");

TEST_CASE_FIXTURE (ZrythmFixture, "crescendo")
{
  auto     midi_track = Track::create_empty_with_action<MidiTrack> ();
  Position pos, end_pos;
  pos.set_to_bar (1);
  end_pos.set_to_bar (4);
  auto r1 = std::make_shared<MidiRegion> (
    pos, end_pos, midi_track->get_name_hash (), 0, 0);
  midi_track->add_region (r1, nullptr, 0, true, false);
  r1->select (true, false, true);
  UNDO_MANAGER->perform (
    std::make_unique<CreateArrangerSelectionsAction> (*TL_SELECTIONS));

  pos.set_to_bar (1);
  end_pos.set_to_bar (2);
  auto mn1 = std::make_shared<MidiNote> (r1->id_, pos, end_pos, 34, 50);
  r1->append_object (mn1);
  mn1->select (true, false, false);
  UNDO_MANAGER->perform (
    std::make_unique<CreateArrangerSelectionsAction> (*MIDI_SELECTIONS));

  pos.set_to_bar (2);
  end_pos.set_to_bar (3);
  auto mn2 = std::make_shared<MidiNote> (r1->id_, pos, end_pos, 34, 50);
  r1->append_object (mn2);
  mn2->select (true, false, false);
  UNDO_MANAGER->perform (
    std::make_unique<CreateArrangerSelectionsAction> (*MIDI_SELECTIONS));

  mn1->select (true, false, false);
  mn2->select (true, true, false);

  MidiFunctionOpts opts = { 0 };
  opts.curve_algo_ = CurveOptions::Algorithm::Exponent;
  opts.curviness_ = 0.5;

  /* test crescendo */
  opts.start_vel_ = 30;
  opts.end_vel_ = 90;
  UNDO_MANAGER->perform (std::make_unique<ArrangerSelectionsAction::EditAction> (
    *MIDI_SELECTIONS, MidiFunctionType::Crescendo, opts));
  REQUIRE_EQ (mn1->vel_->vel_, 30);
  REQUIRE_EQ (mn2->vel_->vel_, 90);

  /* test diminuendo */
  opts.start_vel_ = 90;
  opts.end_vel_ = 30;
  UNDO_MANAGER->perform (std::make_unique<ArrangerSelectionsAction::EditAction> (
    *MIDI_SELECTIONS, MidiFunctionType::Crescendo, opts));
  REQUIRE_EQ (mn1->vel_->vel_, 90);
  REQUIRE_EQ (mn2->vel_->vel_, 30);
}

TEST_SUITE_END;