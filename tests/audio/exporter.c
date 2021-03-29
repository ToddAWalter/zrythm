/*
 * Copyright (C) 2020-2021 Alexandros Theodotou <alex at zrythm dot org>
 *
 * This file is part of Zrythm
 *
 * Zrythm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version approved by
 * Alexandros Theodotou in a public statement of acceptance.
 *
 * Zrythm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Zrythm.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "zrythm-test-config.h"

#include "helpers/plugin_manager.h"
#include "helpers/zrythm.h"

#include "actions/tracklist_selections.h"
#include "audio/encoder.h"
#include "audio/exporter.h"
#include "audio/supported_file.h"
#include "project.h"
#include "utils/math.h"
#include "zrythm.h"

#include <glib.h>

#include <chromaprint.h>
#include <sndfile.h>

/**
 * Chroma fingerprint info for a specific file.
 */
typedef struct ChromaprintFingerprint
{
  uint32_t * fp;
  int        size;
  char *     compressed_str;
} ChromaprintFingerprint;

static void
chromaprint_fingerprint_free (
  ChromaprintFingerprint * self)
{
  chromaprint_dealloc (self->fp);
  chromaprint_dealloc (self->compressed_str);

}

static long
get_num_frames (
  const char * file)
{
  SF_INFO sfinfo;
  memset (&sfinfo, 0, sizeof (sfinfo));
  sfinfo.format =
    sfinfo.format | SF_FORMAT_PCM_16;
  SNDFILE * sndfile =
    sf_open (file, SFM_READ, &sfinfo);
  if (!sndfile)
    {
      const char * err_str =
        sf_strerror (sndfile);
      g_critical ("sndfile null: %s", err_str);
    }
  g_assert_nonnull (sndfile);
  g_assert_cmpint (sfinfo.frames, >, 0);
  long frames = sfinfo.frames;

  int ret = sf_close (sndfile);
  g_assert_cmpint (ret, ==, 0);

  return frames;
}

static ChromaprintFingerprint *
get_fingerprint (
  const char * file1,
  long         max_frames)
{
  int ret;

  SF_INFO sfinfo;
  memset (&sfinfo, 0, sizeof (sfinfo));
  sfinfo.format =
    sfinfo.format | SF_FORMAT_PCM_16;
  SNDFILE * sndfile =
    sf_open (file1, SFM_READ, &sfinfo);
  g_assert_nonnull (sndfile);
  g_assert_cmpint (sfinfo.frames, >, 0);

  ChromaprintContext * ctx =
    chromaprint_new (CHROMAPRINT_ALGORITHM_DEFAULT);
  g_assert_nonnull (ctx);
  ret = chromaprint_start (
    ctx, sfinfo.samplerate, sfinfo.channels);
  g_assert_cmpint (ret, ==, 1);
  int buf_size = sfinfo.frames * sfinfo.channels;
  short data[buf_size];
  sf_count_t frames_read =
    sf_readf_short (sndfile, data, sfinfo.frames);
  g_assert_cmpint (frames_read, ==, sfinfo.frames);
  g_message (
    "read %ld frames for %s", frames_read, file1);

  ret = chromaprint_feed (ctx, data, buf_size);
  g_assert_cmpint (ret, ==, 1);

  ret = chromaprint_finish (ctx);
  g_assert_cmpint (ret, ==, 1);

  ChromaprintFingerprint * fp =
    calloc (1, sizeof (ChromaprintFingerprint));
  ret = chromaprint_get_fingerprint (
    ctx, &fp->compressed_str);
  g_assert_cmpint (ret, ==, 1);
  ret = chromaprint_get_raw_fingerprint (
    ctx, &fp->fp, &fp->size);
  g_assert_cmpint (ret, ==, 1);

  g_message (
    "fingerprint %s [%d]",
    fp->compressed_str, fp->size);

  chromaprint_free (ctx);

  ret = sf_close (sndfile);
  g_assert_cmpint (ret, ==, 0);

  return fp;
}

/**
 * @param perc Minimum percentage of equal
 *   fingerprints required.
 */
static void
check_fingerprint_similarity (
  const char * file1,
  const char * file2,
  int          perc,
  int          expected_size)
{
  const long max_frames =
    MIN (
      get_num_frames (file1),
      get_num_frames (file2));
  ChromaprintFingerprint * fp1 =
    get_fingerprint (file1, max_frames);
  g_assert_cmpint (fp1->size, ==, expected_size);
  ChromaprintFingerprint * fp2 =
    get_fingerprint (file2, max_frames);

  int min = MIN (fp1->size, fp2->size);
  g_assert_cmpint (min, !=, 0);
  int rate = 0;
  for (int i = 0; i < min; i++)
    {
      if (fp1->fp[i] == fp2->fp[i])
        rate++;
    }

  double rated = (double) rate / (double) min;
  int rate_perc =
    math_round_double_to_int (rated * 100.0);
  g_message (
    "%d out of %d (%d%%)", rate, min, rate_perc);

  g_assert_cmpint (rate_perc, >=, perc);

  chromaprint_fingerprint_free (fp1);
  chromaprint_fingerprint_free (fp2);
}

static void
test_export_wav ()
{
  test_helper_zrythm_init ();

  int ret;

  char * filepath =
    g_build_filename (
      TESTS_SRCDIR, "test.wav", NULL);
  SupportedFile * file =
    supported_file_new_from_path (filepath);
  UndoableAction * action =
    tracklist_selections_action_new_create (
      TRACK_TYPE_AUDIO, NULL, file,
      TRACKLIST->num_tracks, PLAYHEAD, 1);
  undo_manager_perform (UNDO_MANAGER, action);

  char * tmp_dir =
    g_dir_make_tmp ("test_wav_prj_XXXXXX", NULL);
  ret =
    project_save (
      PROJECT, tmp_dir, 0, 0, F_NO_ASYNC);
  g_free (tmp_dir);
  g_assert_cmpint (ret, ==, 0);


  for (int i = 0; i < 2; i++)
    {
      g_assert_false (TRANSPORT_IS_ROLLING);
      g_assert_cmpint (
        TRANSPORT->playhead_pos.frames, ==, 0);

      char * filename =
        g_strdup_printf ("test_wav%d.wav", i);

      ExportSettings settings;
      settings.has_error = false;
      settings.cancelled = false;
      settings.format = AUDIO_FORMAT_WAV;
      settings.artist = g_strdup ("Test Artist");
      settings.genre = g_strdup ("Test Genre");
      settings.depth = BIT_DEPTH_16;
      settings.mode = EXPORT_MODE_FULL;
      settings.time_range = TIME_RANGE_LOOP;
      char * exports_dir =
        project_get_path (
          PROJECT, PROJECT_PATH_EXPORTS, false);
      settings.file_uri =
        g_build_filename (
          exports_dir, filename, NULL);
      ret = exporter_export (&settings);
      g_assert_false (AUDIO_ENGINE->exporting);
      g_assert_cmpint (ret, ==, 0);

      check_fingerprint_similarity (
        filepath, settings.file_uri, 100, 6);

      g_free (filename);

      g_assert_false (TRANSPORT_IS_ROLLING);
      g_assert_cmpint (
        TRANSPORT->playhead_pos.frames, ==, 0);
    }

  g_free (filepath);

  test_helper_zrythm_cleanup ();
}

static void
bounce_region (
  bool with_bpm_automation)
{
#ifdef HAVE_HELM
  test_helper_zrythm_init ();

  Position pos, end_pos;
  position_set_to_bar (&pos, 2);
  position_set_to_bar (&end_pos, 4);

  if (with_bpm_automation)
    {
      /* create bpm automation */
      AutomationTrack * at =
        automation_track_find_from_port (
          P_TEMPO_TRACK->bpm_port, P_TEMPO_TRACK, false);
      ZRegion * r =
        automation_region_new (
          &pos, &end_pos, P_TEMPO_TRACK->pos,
          at->index, 0);
      track_add_region (
        P_TEMPO_TRACK, r, at, 0, 1, 0);
      position_set_to_bar (&pos, 1);
      AutomationPoint * ap =
        automation_point_new_float (
          168.434006f, 0.361445993f, &pos);
      automation_region_add_ap (
        r, ap, F_NO_PUBLISH_EVENTS);
      position_set_to_bar (&pos, 2);
      ap =
        automation_point_new_float (
          297.348999f, 0.791164994f, &pos);
      automation_region_add_ap (
        r, ap, F_NO_PUBLISH_EVENTS);
    }

  /* create the plugin track */
  test_plugin_manager_create_tracks_from_plugin (
    HELM_BUNDLE, HELM_URI, true, false, 1);
  Track * track =
    TRACKLIST->tracks[TRACKLIST->num_tracks - 1];
  track_select (
    track, F_SELECT, F_EXCLUSIVE,
    F_NO_PUBLISH_EVENTS);

  /* create a region and select it */
  position_set_to_bar (&pos, 2);
  position_set_to_bar (&end_pos, 4);
  ZRegion * r =
    midi_region_new (
      &pos, &end_pos, track->pos, 0, 0);
  ArrangerObject * r_obj = (ArrangerObject *) r;
  track_add_region (
    track, r, NULL, 0, F_GEN_NAME,
    F_NO_PUBLISH_EVENTS);
  arranger_object_select (
    r_obj, F_SELECT, F_NO_APPEND, F_NO_PUBLISH_EVENTS);
  UndoableAction * ua =
    arranger_selections_action_new_create (
      TL_SELECTIONS);
  undo_manager_perform (UNDO_MANAGER, ua);

  /* bounce it */
  ExportSettings settings;
  memset (&settings, 0, sizeof (ExportSettings));
  timeline_selections_mark_for_bounce (
    TL_SELECTIONS);
  settings.mode = EXPORT_MODE_REGIONS;
  export_settings_set_bounce_defaults (
    &settings, NULL, r->name);

  /* start exporting in a new thread */
  GThread * thread =
    g_thread_new (
      "bounce_thread",
      (GThreadFunc) exporter_generic_export_thread,
      &settings);

  while (settings.progress < 1.0)
    {
      g_message (
        "progress: %f.1", settings.progress * 100.0);
      g_usleep (1000);
    }

  g_thread_join (thread);

  test_helper_zrythm_cleanup ();
#endif
}

static void
test_bounce_region ()
{
  bounce_region (false);
}

static void
test_bounce_with_bpm_automation ()
{
  bounce_region (true);
}

/**
 * Export the audio mixdown when a MIDI track with
 * data is routed to an instrument track.
 */
static void
test_export_midi_routed_to_instrument_track ()
{
#ifdef HAVE_HELM
  test_helper_zrythm_init ();

  /* create the instrument track */
  test_plugin_manager_create_tracks_from_plugin (
    HELM_BUNDLE, HELM_URI, true, false, 1);
  Track * ins_track =
    TRACKLIST->tracks[TRACKLIST->num_tracks - 1];
  track_select (
    ins_track, F_SELECT, F_EXCLUSIVE,
    F_NO_PUBLISH_EVENTS);

  char * midi_file =
    g_build_filename (
      MIDILIB_TEST_MIDI_FILES_PATH,
      "M71.MID", NULL);

  /* create the MIDI track from a MIDI file */
  SupportedFile * file =
    supported_file_new_from_path (midi_file);
  UndoableAction * ua =
    tracklist_selections_action_new_create (
      TRACK_TYPE_MIDI, NULL, file,
      TRACKLIST->num_tracks, PLAYHEAD, 1);
  undo_manager_perform (UNDO_MANAGER, ua);
  Track * midi_track =
    TRACKLIST->tracks[TRACKLIST->num_tracks - 1];
  track_select (
    midi_track, F_SELECT, F_EXCLUSIVE,
    F_NO_PUBLISH_EVENTS);

  /* route the MIDI track to the instrument track */
  ua =
    tracklist_selections_action_new_edit_direct_out (
      TRACKLIST_SELECTIONS, ins_track);
  undo_manager_perform (UNDO_MANAGER, ua);

  /* bounce it */
  ExportSettings settings;
  memset (&settings, 0, sizeof (ExportSettings));
  settings.mode = EXPORT_MODE_FULL;
  export_settings_set_bounce_defaults (
    &settings, NULL, __func__);
  settings.time_range = TIME_RANGE_LOOP;

  /* start exporting in a new thread */
  GThread * thread =
    g_thread_new (
      "bounce_thread",
      (GThreadFunc) exporter_generic_export_thread,
      &settings);

  while (settings.progress < 1.0)
    {
      g_message (
        "progress: %f.1", settings.progress * 100.0);
      g_usleep (1000);
    }

  g_thread_join (thread);

  char * filepath =
    g_build_filename (
      TESTS_SRCDIR,
      "test_export_midi_routed_to_instrument_track.ogg",
      NULL);
  check_fingerprint_similarity (
    filepath, settings.file_uri, 97, 34);
  g_free (filepath);

  test_helper_zrythm_cleanup ();
#endif
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

#define TEST_PREFIX "/audio/exporter/"

  g_test_add_func (
    TEST_PREFIX "test export midi routed to instrument track",
    (GTestFunc) test_export_midi_routed_to_instrument_track);
  g_test_add_func (
    TEST_PREFIX "test export wav",
    (GTestFunc) test_export_wav);
  g_test_add_func (
    TEST_PREFIX "test bounce region",
    (GTestFunc) test_bounce_region);
  g_test_add_func (
    TEST_PREFIX "test bounce with bpm automation",
    (GTestFunc) test_bounce_with_bpm_automation);

  return g_test_run ();
}
