// SPDX-FileCopyrightText: © 2020-2022 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "zrythm-test-config.h"

#include "actions/tracklist_selections.h"
#include "dsp/engine.h"
#include "dsp/fader.h"
#include "dsp/master_track.h"
#include "dsp/midi_event.h"
#include "dsp/router.h"
#include "dsp/tracklist.h"
#include "project.h"

#include "helpers/project_helper.h"
#include "tests/helpers/plugin_manager.h"
#include "tests/helpers/zrythm_helper.h"

static void
test_fader_process_with_instrument (
  const char * pl_bundle,
  const char * pl_uri,
  bool         with_carla)
{
  test_plugin_manager_create_tracks_from_plugin (
    pl_bundle, pl_uri, true, with_carla, 1);

  auto track = TRACKLIST->get_last_track<InstrumentTrack> ();

  /* send a note then wait for playback */
  track->processor_->midi_in_->midi_events_.queued_events_.add_note_on (
    1, 82, 74, 2);

  /* stop dummy audio engine processing so we can process manually */
  test_project_stop_dummy_engine ();

  /* run engine twice (running one is not enough to
   * make the note make sound) */
  for (int i = 0; i < 2; i++)
    {
      AUDIO_ENGINE->process (AUDIO_ENGINE->block_length_);
    }

  /* test fader */
  SemaphoreRAII<> sem{ ROUTER->graph_access_sem_, true };
  bool            has_signal = false;
  auto           &l = track->channel_->fader_->stereo_out_->get_l ();
  for (nframes_t i = 0; i < AUDIO_ENGINE->block_length_; i++)
    {
      if (l.buf_[i] > 0.0001f)
        {
          has_signal = true;
          break;
        }
    }
  REQUIRE (has_signal);
}

TEST_CASE_FIXTURE (ZrythmFixture, "fader process")
{
  test_fader_process_with_instrument (
    TEST_INSTRUMENT_BUNDLE_URI, TEST_INSTRUMENT_URI, true);
}

static bool
track_has_sound (ChannelTrack * track)
{
  for (nframes_t i = 0; i < AUDIO_ENGINE->block_length_; i++)
    {
      if (
        std::abs (track->channel_->fader_->stereo_out_->get_l ().buf_[i])
        > 0.0001f)
        {
          return true;
        }
    }
  return false;
}

static void
test_track_has_sound (ChannelTrack * track, bool expect_sound)
{
  Position pos;
  pos.set_to_bar (1);
  TRANSPORT->set_playhead_pos (pos);
  TRANSPORT->request_roll (true);

  AUDIO_ENGINE->process (AUDIO_ENGINE->block_length_);
  AUDIO_ENGINE->process (AUDIO_ENGINE->block_length_);
  AUDIO_ENGINE->process (AUDIO_ENGINE->block_length_);

  REQUIRE (track_has_sound (track) == expect_sound);

  TRANSPORT->request_pause (true);
(AUDIO_ENGINE->process( AUDIO_ENGINE->block_length_);
}

TEST_CASE_FIXTURE (ZrythmFixture, "solo")
{
  /* create audio track */
  FileDescriptor file (fs::path (TESTS_SRCDIR) / "test.wav");
  Track::create_with_action (
    Track::Type::Audio, nullptr, &file, &PLAYHEAD, TRACKLIST->get_num_tracks (),
    1, -1, nullptr);
  auto audio_track = TRACKLIST->get_last_track<AudioTrack> ();

  /* create audio track 2 */
  Track::create_with_action (
    Track::Type::Audio, nullptr, &file, &PLAYHEAD, TRACKLIST->get_num_tracks (),
    1, -1, nullptr);
  auto audio_track2 = TRACKLIST->get_last_track<AudioTrack> ();

  /* create group track */
  auto group_track = Track::create_empty_with_action<AudioGroupTrack> ();

  /* route audio tracks to group track */
  track_select (audio_track, true, true, false);
  tracklist_selections_action_perform_set_direct_out (
    TRACKLIST_SELECTIONS, PORT_CONNECTIONS_MGR, group_track, nullptr);
  track_select (audio_track2, true, true, false);
  tracklist_selections_action_perform_set_direct_out (
    TRACKLIST_SELECTIONS, PORT_CONNECTIONS_MGR, group_track, nullptr);

  /* stop dummy audio engine processing so we can
   * process manually */
  AUDIO_ENGINE->stop_dummy_audio_thread = true;
  g_usleep (1000000);

  /* test solo group makes sound */
  track_set_soloed (group_track, F_SOLO, F_TRIGGER_UNDO, F_AUTO_SELECT, false);
  test_track_has_sound (P_MASTER_TRACK, true);
  test_track_has_sound (group_track, true);
  test_track_has_sound (audio_track, true);
  test_track_has_sound (audio_track2, true);
  UNDO_MANAGER->undo ();

  /* test solo audio track makes sound */
  track_set_soloed (audio_track, F_SOLO, F_TRIGGER_UNDO, F_AUTO_SELECT, false);
  test_track_has_sound (P_MASTER_TRACK, true);
  test_track_has_sound (group_track, true);
  test_track_has_sound (audio_track, true);
  test_track_has_sound (audio_track2, false);
  UNDO_MANAGER->undo ();

  /* test solo both audio tracks */
  track_select (audio_track, true, true, false);
  track_select (audio_track2, true, false, false);
  tracklist_selections_action_perform_edit_solo (
    TRACKLIST_SELECTIONS, F_SOLO, nullptr);
  test_track_has_sound (P_MASTER_TRACK, true);
  test_track_has_sound (group_track, true);
  test_track_has_sound (audio_track, true);
  test_track_has_sound (audio_track2, true);
  UNDO_MANAGER->undo ();

  /* test undo/redo */
  track_select (audio_track, true, true, false);
  tracklist_selections_action_perform_edit_solo (
    TRACKLIST_SELECTIONS, F_SOLO, nullptr);
  g_assert_true (track_get_soloed (audio_track));
  g_assert_false (track_get_soloed (audio_track2));
  track_select (audio_track, true, true, false);
  track_select (audio_track2, true, false, false);
  tracklist_selections_action_perform_edit_solo (
    TRACKLIST_SELECTIONS, F_SOLO, nullptr);
  g_assert_true (track_get_soloed (audio_track));
  g_assert_true (track_get_soloed (audio_track2));
  UNDO_MANAGER->undo ();
  g_assert_true (track_get_soloed (audio_track));
  g_assert_false (track_get_soloed (audio_track2));
  UNDO_MANAGER->redo ();
  g_assert_true (track_get_soloed (audio_track));
  g_assert_true (track_get_soloed (audio_track2));
}

TEST_SUITE_END;