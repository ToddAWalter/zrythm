// SPDX-FileCopyrightText: © 2022 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "zrythm-test-config.h"

#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include "dsp/track.h"
#include "project.h"
#include "utils/flags.h"
#include "zrythm.h"

#include "tests/helpers/exporter.h"
#include "tests/helpers/plugin_manager.h"
#include "tests/helpers/project_helper.h"

static void
test_midi_fx_routing (void)
{
  test_helper_zrythm_init ();

  /* create an instrument */
  PluginSetting * setting = test_plugin_manager_get_plugin_setting (
    TEST_INSTRUMENT_BUNDLE_URI, TEST_INSTRUMENT_URI, true);
  g_assert_nonnull (setting);
  Track * track = track_create_for_plugin_at_idx_w_action (
    Track::Type::Instrument, setting, TRACKLIST->tracks.size (), nullptr);
  g_assert_nonnull (track);

  int num_dests = port_connections_manager_get_sources_or_dests (
    PORT_CONNECTIONS_MGR, nullptr, &track->processor->midi_out->id_, false);
  g_assert_cmpint (num_dests, ==, 1);

  /* add MIDI file */
  char ** midi_files = io_get_files_in_dir_ending_in (
    MIDILIB_TEST_MIDI_FILES_PATH, F_RECURSIVE, ".MID", false);
  g_assert_nonnull (midi_files);
  FileDescriptor file = FileDescriptor (midi_files[0]);
  bool           success = tracklist_import_files (
    TRACKLIST, nullptr, &file, track, track->lanes[0], -1, PLAYHEAD, nullptr,
    nullptr);
  g_assert_true (success);
  g_strfreev (midi_files);

  /* export loop and check that there is audio */
  char * audio_file = test_exporter_export_audio (
    ExportTimeRange::TIME_RANGE_LOOP, ExportMode::EXPORT_MODE_FULL);
  z_return_if_fail (audio_file);
  g_assert_false (audio_file_is_silent (audio_file));
  io_remove (audio_file);
  g_free (audio_file);

  /* create MIDI eat plugin and add to MIDI FX */
  setting = test_plugin_manager_get_plugin_setting (
    PLUMBING_BUNDLE_URI, "http://gareus.org/oss/lv2/plumbing#eat1", true);
  g_assert_nonnull (setting);
  GError * err = NULL;
  bool     ret = mixer_selections_action_perform_create (
    ZPluginSlotType::Z_PLUGIN_SLOT_MIDI_FX, track->name_hash, 0, setting, 1,
    &err);
  g_assert_true (ret);

  num_dests = port_connections_manager_get_sources_or_dests (
    PORT_CONNECTIONS_MGR, NULL, &track->processor->midi_out->id_, false);
  g_assert_cmpint (num_dests, ==, 1);

  /* export loop and check that there is no audio */
  audio_file = test_exporter_export_audio (
    ExportTimeRange::TIME_RANGE_LOOP, ExportMode::EXPORT_MODE_FULL);
  g_return_if_fail (audio_file);
  g_assert_true (audio_file_is_silent (audio_file));
  io_remove (audio_file);
  g_free (audio_file);

  /* bypass MIDI FX and check that there is audio */
  Plugin * midieat = track->channel->midi_fx[0];
  plugin_set_enabled (midieat, F_NOT_ENABLED, F_NO_PUBLISH_EVENTS);
  g_assert_false (plugin_is_enabled (midieat, false));

  /* export loop and check that there is audio
   * again */
  audio_file = test_exporter_export_audio (
    ExportTimeRange::TIME_RANGE_LOOP, ExportMode::EXPORT_MODE_FULL);
  g_return_if_fail (audio_file);
  g_assert_false (audio_file_is_silent (audio_file));
  io_remove (audio_file);
  g_free (audio_file);

  test_helper_zrythm_cleanup ();
}

int
main (int argc, char * argv[])
{
  g_test_init (&argc, &argv, NULL);

#define TEST_PREFIX "/audio/channel/"

  g_test_add_func (
    TEST_PREFIX "test midi fx routing", (GTestFunc) test_midi_fx_routing);

  return g_test_run ();
}
