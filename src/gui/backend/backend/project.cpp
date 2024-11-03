// SPDX-FileCopyrightText: © 2018-2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "zrythm-config.h"

#include <filesystem>

#include "common/dsp/audio_region.h"
#include "common/dsp/audio_track.h"
#include "common/dsp/chord_track.h"
#include "common/dsp/engine.h"
#include "common/dsp/marker_track.h"
#include "common/dsp/master_track.h"
#include "common/dsp/modulator_track.h"
#include "common/dsp/port_connections_manager.h"
#include "common/dsp/router.h"
#include "common/dsp/tempo_track.h"
#include "common/dsp/tracklist.h"
#include "common/dsp/transport.h"
#include "common/utils/datetime.h"
#include "common/utils/exceptions.h"
#include "common/utils/file.h"
#include "common/utils/gtest_wrapper.h"
#include "common/utils/io.h"
#include "common/utils/logger.h"
#include "common/utils/objects.h"
#include "common/utils/progress_info.h"
#include "common/utils/ui.h"
#include "gui/backend/backend/project.h"
#include "gui/backend/backend/settings/g_settings_manager.h"
#include "gui/backend/backend/tracklist_selections.h"
#include "gui/backend/backend/zrythm.h"
#include "gui/backend/project_manager.h"

#include <glib/gi18n.h>

#include "juce_wrapper.h"
#include <fmt/printf.h>
#include <glibmm.h>
#include <zstd.h>

Project::Project (QObject * parent)
    : QObject (parent), version_ (Zrythm::get_version (false)),
      port_connections_manager_ (new PortConnectionsManager (this)),
      audio_engine_ (std::make_unique<AudioEngine> (this)),
      transport_ (new Transport (this)),
      quantize_opts_editor_ (
        std::make_unique<QuantizeOptions> (NoteLength::NOTE_LENGTH_1_8)),
      quantize_opts_timeline_ (
        std::make_unique<QuantizeOptions> (NoteLength::NOTE_LENGTH_1_1)),
      snap_grid_editor_ (
        std::make_unique<
          SnapGrid> (SnapGrid::Type::Editor, NoteLength::NOTE_LENGTH_1_8, true)),
      snap_grid_timeline_ (std::make_unique<SnapGrid> (
        SnapGrid::Type::Timeline,
        NoteLength::NOTE_LENGTH_BAR,
        true)),
      timeline_ (new Timeline (this)),
      midi_mappings_ (std::make_unique<MidiMappings> ()),
      tracklist_ (new Tracklist (*this)),
      undo_manager_ (std::make_unique<UndoManager> ())
{
  init_selections ();
  tracklist_selections_ =
    std::make_unique<SimpleTracklistSelections> (*tracklist_);
  // audio_engine_ = std::make_unique<AudioEngine> (this);
}

Project::Project (const std::string &title, QObject * parent) : Project (parent)
{
  title_ = title;
}

Project::~Project ()
{
  loaded_ = false;
}

std::string
Project::get_newer_backup ()
{
  std::string filepath = get_path (ProjectPath::ProjectFile, false);
  z_return_val_if_fail (!filepath.empty (), "");

  std::filesystem::file_time_type original_time;
  if (std::filesystem::exists (filepath))
    {
      original_time = std::filesystem::last_write_time (filepath);
    }
  else
    {
      z_warning ("Failed to get last modified for {}", filepath);
      return {};
    }

  std::string result;
  std::string backups_dir = get_path (ProjectPath::BACKUPS, false);

  try
    {
      for (const auto &entry : std::filesystem::directory_iterator (backups_dir))
        {
          auto full_path = entry.path () / PROJECT_FILE;
          z_debug ("{}", full_path.string ());

          if (std::filesystem::exists (full_path))
            {
              auto backup_time = std::filesystem::last_write_time (full_path);
              if (backup_time > original_time)
                {
                  result = entry.path ().string ();
                  original_time = backup_time;
                }
            }
          else
            {
              z_warning (
                "Failed to get last modified for {}", full_path.string ());
              return {};
            }
        }
    }
  catch (const std::filesystem::filesystem_error &e)
    {
      z_warning ("Error accessing backup directory: {}", e.what ());
      return {};
    }

  return result;
}

void
Project::make_project_dirs (bool is_backup)
{
  for (
    auto type :
    { ProjectPath::BACKUPS, ProjectPath::EXPORTS, ProjectPath::EXPORTS_STEMS,
      ProjectPath::POOL, ProjectPath::PluginStates,
      ProjectPath::PLUGIN_EXT_COPIES, ProjectPath::PLUGIN_EXT_LINKS })
    {
      std::string dir = get_path (type, is_backup);
      z_return_if_fail (dir.length () > 0);
      try
        {
          io_mkdir (dir);
        }
      catch (ZrythmException &e)
        {
          throw ZrythmException (
            fmt::format ("Failed to create directory {}", dir));
        }
    }
}

void
Project::compress_or_decompress (
  bool                   compress,
  char **                _dest,
  size_t *               _dest_size,
  ProjectCompressionFlag dest_type,
  const char *           _src,
  const size_t           _src_size,
  ProjectCompressionFlag src_type)
{
  z_info (
    "using zstd v{}.{}.{}", ZSTD_VERSION_MAJOR, ZSTD_VERSION_MINOR,
    ZSTD_VERSION_RELEASE);

  char * src = NULL;
  size_t src_size = 0;
  switch (src_type)
    {
    case ProjectCompressionFlag::PROJECT_COMPRESS_DATA:
      src = (char *) _src;
      src_size = _src_size;
      break;
    case ProjectCompressionFlag::PROJECT_COMPRESS_FILE:
      {
        GError * err = NULL;
        gsize    file_contents_sz = 0;
        bool success = g_file_get_contents (_src, &src, &file_contents_sz, &err);
        src_size = static_cast<size_t> (file_contents_sz);
        if (!success)
          {
            std::string msg = err->message;
            g_error_free_and_null (err);
            throw ZrythmException (fmt::format (
              "Failed to get contents from file '{}': {}", _src, msg));
          }
      }
      break;
    }

  char * dest = NULL;
  size_t dest_size = 0;
  if (compress)
    {
      z_info ("compressing project...");
      size_t compress_bound = ZSTD_compressBound (src_size);
      dest = (char *) malloc (compress_bound);
      dest_size = ZSTD_compress (dest, compress_bound, src, src_size, 1);
      if (ZSTD_isError (dest_size))
        {
          free (dest);
          throw ZrythmException (format_str (
            "Failed to compress project file: {}",
            ZSTD_getErrorName (dest_size)));
        }
    }
  else /* decompress */
    {
#if (ZSTD_VERSION_MAJOR == 1 && ZSTD_VERSION_MINOR < 3)
      unsigned long long const frame_content_size =
        ZSTD_getDecompressedSize (src, src_size);
      if (frame_content_size == 0)
#else
      unsigned long long const frame_content_size =
        ZSTD_getFrameContentSize (src, src_size);
      if (frame_content_size == ZSTD_CONTENTSIZE_ERROR)
#endif
        {
          throw ZrythmException ("Project not compressed by zstd");
        }
      dest = (char *) malloc ((size_t) frame_content_size);
      dest_size = ZSTD_decompress (dest, frame_content_size, src, src_size);
      if (ZSTD_isError (dest_size))
        {
          free (dest);
          throw ZrythmException (format_str (
            "Failed to decompress project file: {}",
            ZSTD_getErrorName (dest_size)));
        }
      if (dest_size != frame_content_size)
        {
          free (dest);
          /* impossible because zstd will check this condition */
          throw ZrythmException ("uncompressed_size != frame_content_size");
        }
    }

  z_debug (
    "{} : {} bytes -> {} bytes", compress ? "Compression" : "Decompression",
    (unsigned) src_size, (unsigned) dest_size);

  switch (dest_type)
    {
    case ProjectCompressionFlag::PROJECT_COMPRESS_DATA:
      *_dest = dest;
      *_dest_size = dest_size;
      break;
    case ProjectCompressionFlag::PROJECT_COMPRESS_FILE:
      {
        GError * err = NULL;
        bool     success =
          g_file_set_contents (*_dest, dest, (gssize) dest_size, &err);
        if (!success)
          {
            std::string msg = err->message;
            g_error_free_and_null (err);
            throw ZrythmException (
              fmt::sprintf ("Failed to write project file", msg));
          }
      }
      break;
    }
}

void
Project::set_and_create_next_available_backup_dir ()
{
  std::string backups_dir = get_path (ProjectPath::BACKUPS, false);

  int i = 0;
  do
    {
      if (i > 0)
        {
          std::string bak_title = fmt::format ("{}.bak{}", title_, i);
          backup_dir_ = Glib::build_filename (backups_dir, bak_title);
        }
      else
        {
          std::string bak_title = fmt::format ("{}.bak", title_);
          backup_dir_ = Glib::build_filename (backups_dir, bak_title);
        }
      i++;
    }
  while (file_path_exists (backup_dir_));

  try
    {
      io_mkdir (backup_dir_);
    }
  catch (ZrythmException &e)
    {
      throw ZrythmException (
        format_str (_ ("Failed to create backup directory {}"), backup_dir_));
    }
}

void
Project::activate ()
{
  z_debug ("Activating project {} ({:p})...", title_, fmt::ptr (this));

  last_saved_action_ = undo_manager_->get_last_action ();

  audio_engine_->activate (true);

  /* pause engine */
  AudioEngine::State state{};
  audio_engine_->wait_for_pause (state, true, false);

  /* connect channel inputs to hardware and re-expose ports to
   * backend. has to be done after engine activation */
  for (auto * track : tracklist_->tracks_ | type_is<ChannelTrack> ())
    {
      track->channel_->reconnect_ext_input_ports (*audio_engine_);
    }
  tracklist_->expose_ports_to_backend (*audio_engine_);

  /* reconnect graph */
  audio_engine_->router_->recalc_graph (false);

  /* fix audio regions in case running under a new sample rate */
  fix_audio_regions ();

  /* resume engine */
  audio_engine_->resume (state);

  z_debug ("Project {} ({:p}) activated", title_, fmt::ptr (this));
}

void
Project::add_default_tracks ()
{
  z_return_if_fail (tracklist_);

  /* init pinned tracks */

  auto add_track = [&]<typename T> () {
    static_assert (std::derived_from<T, Track>, "T must be derived from Track");

    z_debug ("adding {} track...", typeid (T).name ());
    return tracklist_->append_track (
      *T::create_unique (tracklist_->tracks_.size ()), false, false);
  };

  /* chord */
  add_track.operator()<ChordTrack> ();

  /* add a scale */
  auto scale = std::make_unique<ScaleObject> (
    MusicalScale (MusicalScale::Type::Aeolian, MusicalNote::A));
  tracklist_->chord_track_->add_scale (std::move (scale));

  /* tempo */
  add_track.operator()<TempoTrack> ();
  int   beats_per_bar = tracklist_->tempo_track_->get_beats_per_bar ();
  int   beat_unit = tracklist_->tempo_track_->get_beat_unit ();
  bpm_t bpm = tracklist_->tempo_track_->get_current_bpm ();
  transport_->update_caches (beats_per_bar, beat_unit);
  audio_engine_->update_frames_per_tick (
    beats_per_bar, bpm, audio_engine_->sample_rate_, true, true, false);

  /* modulator */
  add_track.operator()<ModulatorTrack> ();

  /* marker */
  add_track.operator()<MarkerTrack> ()->add_default_markers (*transport_);

  tracklist_->pinned_tracks_cutoff_ = tracklist_->tracks_.size ();

  /* add master channel to mixer and tracklist */
  add_track.operator()<MasterTrack> ();
  tracklist_selections_->add_track (*tracklist_->master_track_, false);
  last_selection_ = SelectionType::Tracklist;
}

bool
Project::validate () const
{
  z_debug ("validating project...");

#if 0
  size_t max_size = 20;
  Port ** ports =
    object_new_n (max_size, Port *);
  int num_ports = 0;
  port_get_all (
    &ports, &max_size, true, &num_ports);
  for (int i = 0; i < num_ports; i++)
    {
      Port * port = ports[i];
      port_update_identifier (
        port, &port->id_, port->track,
        F_UPDATE_AUTOMATION_TRACK);
    }
  free (ports);
#endif

  if (!tracklist_->validate ())
    return false;

  region_link_group_manager_.validate ();

  /* TODO add arranger_object_get_all and check
   * positions (arranger_object_validate) */

  z_debug ("project validation passed");

  return true;
}

bool
Project::fix_audio_regions ()
{
  z_debug ("fixing audio region positions...");

  int num_fixed = 0;
  for (const auto &track : tracklist_->tracks_ | type_is<AudioTrack> ())
    {
      for (const auto &lane_var : track->lanes_)
        {
          auto * lane = std::get<AudioLane *> (lane_var);
          for (const auto &region : lane->regions_)
            {
              if (region->fix_positions (0))
                num_fixed++;
            }
        }
    }

  z_debug ("done fixing {} audio region positions", num_fixed);

  return num_fixed > 0;
}

#if 0
ArrangerWidget *
project_get_arranger_for_last_selection (
  Project * self)
{
  Region * r =
    CLIP_EDITOR->get_region ();
  switch (self->last_selection)
    {
    case Project::SelectionType::Timeline:
      return TL_SELECTIONS;
      break;
    case Project::SelectionType::Editor:
      if (r)
        {
          switch (r->id.type)
            {
            case RegionType::REGION_TYPE_AUDIO:
              return AUDIO_SELECTIONS;
            case RegionType::REGION_TYPE_AUTOMATION:
              return AUTOMATION_SELECTIONS;
            case RegionType::REGION_TYPE_MIDI:
              return MIDI_SELECTIONS;
            case RegionType::REGION_TYPE_CHORD:
              return CHORD_SELECTIONS;
            }
        }
      break;
    default:
      return NULL;
    }

  return NULL;
}
#endif

ArrangerSelections *
Project::get_arranger_selections_for_last_selection ()
{
  auto r = CLIP_EDITOR->get_region ();
  switch (last_selection_)
    {
    case Project::SelectionType::Timeline:
      return TL_SELECTIONS.get ();
      break;
    case Project::SelectionType::Editor:
      if (r)
        {
          return r->get_arranger_selections ();
        }
      break;
    default:
      return NULL;
    }

  return NULL;
}

void
Project::init_selections (bool including_arranger_selections)
{
  if (including_arranger_selections)
    {
      automation_selections_ = std::make_unique<AutomationSelections> ();
      audio_selections_ = std::make_unique<AudioSelections> ();
      chord_selections_ = std::make_unique<ChordSelections> ();
      timeline_selections_ = std::make_unique<TimelineSelections> ();
      midi_selections_ = std::make_unique<MidiSelections> ();
    }
  mixer_selections_ = std::make_unique<ProjectMixerSelections> ();
}

void
Project::get_all_ports (std::vector<Port *> &ports) const
{
  audio_engine_->append_ports (ports);

  std::ranges::for_each (tracklist_->tracks_, [&] (auto &&track_var) {
    std::visit (
      [&] (const auto &track) { track->append_ports (ports, false); },
      track_var);
  });
}

char *
Project::get_existing_uncompressed_text (bool backup)
{
  /* get file contents */
  std::string project_file_path = get_path (ProjectPath::ProjectFile, backup);
  z_debug ("getting text for project file {}", project_file_path);

  char *   compressed_pj;
  gsize    compressed_pj_size;
  GError * err = NULL;
  g_file_get_contents (
    project_file_path.c_str (), &compressed_pj, &compressed_pj_size, &err);
  if (err != nullptr)
    {
      std::string msg = err->message;
      g_error_free_and_null (err);
      throw ZrythmException (format_str (
        _ ("Unable to read file at {}: {}"), project_file_path, msg));
    }

  /* decompress */
  z_info ("{}: decompressing project...", __func__);
  char * text = NULL;
  size_t text_size;
  err = NULL;
  try
    {
      decompress (
        &text, &text_size, PROJECT_DECOMPRESS_DATA, compressed_pj,
        compressed_pj_size, PROJECT_DECOMPRESS_DATA);
    }
  catch (ZrythmException &e)
    {
      throw ZrythmException (format_str (
        _ ("Unable to decompress project file at {}"), project_file_path));
    }
  g_free (compressed_pj);

  /* make string null-terminated */
  text = (char *) g_realloc (text, text_size + sizeof (char));
  text[text_size] = '\0';

  return text;
}

int
Project::autosave_cb (void * data)
{
#if 0
  if (
    !PROJECT || !PROJECT->loaded_ || PROJECT->dir_.empty ()
    || PROJECT->datetime_str_.empty () || !MAIN_WINDOW || !MAIN_WINDOW->setup)
    return G_SOURCE_CONTINUE;

  unsigned int autosave_interval_mins =
    g_settings_get_uint (S_P_PROJECTS_GENERAL, "autosave-interval");

  /* return if autosave disabled */
  if (autosave_interval_mins <= 0)
    return G_SOURCE_CONTINUE;

  auto &out_ports =
    PROJECT->tracklist_->master_track_->get_channel ()->stereo_out_;
  auto cur_time = SteadyClock::now ();
  /* subtract 4 seconds because the time this gets called is not exact (this is
   * an old comment and I don't remember why this is done) */
  auto autosave_interval =
    std::chrono::minutes (autosave_interval_mins) - std::chrono::seconds (4);

  /* skip if semaphore busy */
  SemaphoreRAII sem (PROJECT->save_sem_);
  if (!sem.is_acquired ())
    {
      z_debug ("can't acquire project lock - skipping autosave");
      return G_SOURCE_CONTINUE;
    }

  auto * last_action = PROJECT->undo_manager_->get_last_action ();

  /* skip if bad time to save or rolling */
  if (cur_time - PROJECT->last_successful_autosave_time_ < autosave_interval || TRANSPORT->is_rolling() || (TRANSPORT->play_state_ == Transport::PlayState::RollRequested && (TRANSPORT->preroll_frames_remaining_ > 0 || TRANSPORT->countin_frames_remaining_ > 0)))
    {
      return G_SOURCE_CONTINUE;
    }

  /* skip if sound is playing */
  if (
    out_ports->get_l ().get_peak () >= 0.0001f
    || out_ports->get_r ().get_peak () >= 0.0001f)
    {
      z_debug ("sound is playing, skipping autosave");
      return G_SOURCE_CONTINUE;
    }

  /* skip if currently performing action */
  if (arranger_widget_any_doing_action ())
    {
      z_debug ("in the middle of an action, skipping autosave");
      return G_SOURCE_CONTINUE;
    }

  if (PROJECT->last_action_in_last_successful_autosave_ == last_action)
    {
      z_debug ("last action is same as previous backup - skipping autosave");
      return G_SOURCE_CONTINUE;
    }

  /* skip if any modal window is open */
  if (ZRYTHM_HAVE_UI)
    {
      auto toplevels = gtk_window_get_toplevels ();
      auto num_toplevels = g_list_model_get_n_items (toplevels);
      for (guint i = 0; i < num_toplevels; i++)
        {
          auto window = GTK_WINDOW (g_list_model_get_item (toplevels, i));
          if (
            gtk_widget_get_visible (GTK_WIDGET (window))
            && (gtk_window_get_modal (window) || gtk_window_get_transient_for (window) == GTK_WINDOW (MAIN_WINDOW)))
            {
              z_debug ("modal/transient windows exist - skipping autosave");
              z_gtk_widget_print_hierarchy (GTK_WIDGET (window));
              return G_SOURCE_CONTINUE;
            }
        }
    }

  /* ok to save */
  try
    {
      PROJECT->save (PROJECT->dir_, true, true, true);
      PROJECT->last_successful_autosave_time_ = cur_time;
    }
  catch (const ZrythmException &e)
    {
      if (ZRYTHM_HAVE_UI)
        {
          e.handle (_ ("Failed to save the project"));
        }
      else
        {
          z_warning ("{}", e.what ());
        }
    }

#endif
  return G_SOURCE_CONTINUE;
}

std::string
Project::get_path (ProjectPath path, bool backup)
{
  auto &dir = backup ? backup_dir_ : dir_;
  switch (path)
    {
    case ProjectPath::BACKUPS:
      return Glib::build_filename (dir, PROJECT_BACKUPS_DIR);
    case ProjectPath::EXPORTS:
      return Glib::build_filename (dir, PROJECT_EXPORTS_DIR);
    case ProjectPath::EXPORTS_STEMS:
      return Glib::build_filename (dir, PROJECT_EXPORTS_DIR, PROJECT_STEMS_DIR);
    case ProjectPath::PLUGINS:
      return Glib::build_filename (dir, PROJECT_PLUGINS_DIR);
    case ProjectPath::PluginStates:
      {
        std::string plugins_dir = get_path (ProjectPath::PLUGINS, backup);
        return Glib::build_filename (plugins_dir, PROJECT_PLUGIN_STATES_DIR);
      }
      break;
    case ProjectPath::PLUGIN_EXT_COPIES:
      {
        std::string plugins_dir = get_path (ProjectPath::PLUGINS, backup);
        return Glib::build_filename (plugins_dir, PROJECT_PLUGIN_EXT_COPIES_DIR);
      }
      break;
    case ProjectPath::PLUGIN_EXT_LINKS:
      {
        std::string plugins_dir = get_path (ProjectPath::PLUGINS, backup);
        return Glib::build_filename (plugins_dir, PROJECT_PLUGIN_EXT_LINKS_DIR);
      }
      break;
    case ProjectPath::POOL:
      return Glib::build_filename (dir, PROJECT_POOL_DIR);
    case ProjectPath::ProjectFile:
      return Glib::build_filename (dir, PROJECT_FILE);
    case ProjectPath::FINISHED_FILE:
      return Glib::build_filename (dir, PROJECT_FINISHED_FILE);
    default:
      z_return_val_if_reached ("");
    }
  z_return_val_if_reached ("");
}

Project::SerializeProjectThread::SerializeProjectThread (SaveContext &ctx)
    : juce::Thread ("SerializeProject"), ctx_ (ctx)
{
  startThread ();
}

Project::SerializeProjectThread::~SerializeProjectThread ()
{
  stopThread (-1);
}

void
Project::SerializeProjectThread::run ()
{
  char * compressed_json;
  size_t compressed_size;

  /* generate json */
  z_debug ("serializing project to json...");
  auto                       time_before = g_get_monotonic_time ();
  gint64                     time_after;
  std::optional<CStringRAII> json;
  try
    {
      json = ctx_.project_->serialize_to_json_string ();
    }
  catch (const ZrythmException &e)
    {
      e.handle ("Failed to serialize project");
      ctx_.has_error_ = true;
      goto serialize_end;
    }
  time_after = g_get_monotonic_time ();
  z_debug ("time to serialize: {}ms", (time_after - time_before) / 1000);

  /* compress */
  try
    {
      compress (
        &compressed_json, &compressed_size,
        ProjectCompressionFlag::PROJECT_COMPRESS_DATA, json->c_str (),
        strlen (json->c_str ()) * sizeof (char),
        ProjectCompressionFlag::PROJECT_COMPRESS_DATA);
    }
  catch (const ZrythmException &ex)
    {
      ex.handle (_ ("Failed to compress project file"));
      ctx_.has_error_ = true;
      goto serialize_end;
    }

  /* set file contents */
  z_debug ("saving project file at {}...", ctx_.project_file_path_);
  try
    {
      Glib::file_set_contents (
        ctx_.project_file_path_, compressed_json,
        static_cast<gssize> (compressed_size));
      g_free (compressed_json);
    }
  catch (const Glib::Error &e)
    {
      ctx_.has_error_ = true;
      z_error ("Unable to write project file: {}", e.what ());
    }

  z_debug ("successfully saved project");

serialize_end:
  ctx_.main_project_->undo_manager_->action_sem_.release ();
  ctx_.finished_.store (true);
}

int
Project::idle_saved_callback (SaveContext * ctx)
{
  if (!ctx->finished_)
    {
      return G_SOURCE_CONTINUE;
    }

  if (ctx->is_backup_)
    {

      z_debug ("Backup saved.");
      if (ZRYTHM_HAVE_UI)
        {

          ui_show_notification (_ ("Backup saved."));
        }
    }
  else
    {
      if (ZRYTHM_HAVE_UI && !ZRYTHM_TESTING && !ZRYTHM_BENCHMARKING)
        {
          zrythm::gui::ProjectManager::get_instance ()->add_to_recent_projects (
            QString::fromStdString (ctx->project_file_path_));
        }
      if (ctx->show_notification_)

        ui_show_notification (_ ("Project saved."));
    }

#if 0
  if (ZRYTHM_HAVE_UI && PROJECT->loaded_ && MAIN_WINDOW)
    {
      /* EVENTS_PUSH (EventType::ET_PROJECT_SAVED, PROJECT.get ()); */
    }
#endif

  ctx->progress_info_.mark_completed (
    ProgressInfo::CompletionType::SUCCESS, nullptr);

  return G_SOURCE_REMOVE;
}

void
Project::cleanup_plugin_state_dirs (Project &main_project, bool is_backup)
{
  z_debug ("cleaning plugin state dirs{}...", is_backup ? " for backup" : "");

  std::vector<zrythm::plugins::Plugin *> plugins;
  zrythm::plugins::Plugin::get_all (main_project, plugins, true);
  zrythm::plugins::Plugin::get_all (*this, plugins, true);

  for (size_t i = 0; i < plugins.size (); i++)
    {
      z_debug ("plugin {}: {}", i, plugins[i]->state_dir_.c_str ());
    }

  auto plugin_states_path =
    main_project.get_path (ProjectPath::PluginStates, false);

  try
    {
      Glib::Dir dir (plugin_states_path);
      for (auto filename : dir)
        {
          auto full_path = Glib::build_filename (plugin_states_path, filename);

          if (!Glib::file_test (full_path, Glib::FileTest::IS_DIR))
            {
              continue;
            }

          bool found = std::any_of (
            plugins.begin (), plugins.end (), [&filename] (const auto &pl) {
              return pl->state_dir_ == filename;
            });
          if (!found)
            {
              z_debug ("removing unused plugin state in {}", full_path);
              io_rmdir (full_path, true);
            }
        }
    }
  catch (const Glib::Error &e)
    {
      z_critical ("Failed to open directory: {}", e.what ());
      return;
    }

  z_debug ("cleaned plugin state directories");
}

void
Project::save (
  const std::string &_dir,
  const bool         is_backup,
  const bool         show_notification,
  const bool         async)
{
  z_info (
    "Saving project at {}, is backup: {}, show notification: {}, async: {}",
    _dir, is_backup, show_notification, async);

  /* pause engine */
  AudioEngine::State state{};
  bool               engine_paused = false;
  z_return_if_fail (audio_engine_);
  if (audio_engine_->activated_)
    {
      audio_engine_->wait_for_pause (state, false, true);
      engine_paused = true;
    }

  /* if async, lock the undo manager */
  if (async)
    {
      undo_manager_->action_sem_.acquire ();
    }

  validate ();

  /* set the dir and create it if it doesn't exist */
  dir_ = _dir;
  try
    {
      io_mkdir (dir_);
    }

  catch (const ZrythmException &e)
    {
      throw ZrythmException (

        fmt::format ("Failed to create project directory {}", dir_));
    }

  /* set the title */
  title_ = dir_.filename ().string ();

  /* save current datetime */
  datetime_str_ = datetime_get_current_as_string ();

  /* set the project version */
  version_ = Zrythm::get_version (false);

  /* if backup, get next available backup dir */
  if (is_backup)
    {
      try
        {
          set_and_create_next_available_backup_dir ();
        }

      catch (const ZrythmException &e)
        {
          throw ZrythmException (_ ("Failed to create backup directory"));
        }
    }

  try
    {
      make_project_dirs (is_backup);
    }

  catch (const ZrythmException &e)
    {
      throw ZrythmException (_ ("Failed to create project directories"));
    }

  if (this == get_active_instance ())
    {
      /* write the pool */
      audio_engine_->pool_->remove_unused (is_backup);
    }

  try
    {
      audio_engine_->pool_->write_to_disk (is_backup);
    }
  catch (const ZrythmException &e)
    {
      throw ZrythmException (_ ("Failed to write audio pool to disk"));
    }

  auto ctx = std::make_unique<SaveContext> ();
  ctx->main_project_ = this;
  ctx->project_file_path_ = get_path (ProjectPath::ProjectFile, is_backup);
  ctx->show_notification_ = show_notification;
  ctx->is_backup_ = is_backup;
  if (ZRYTHM_IS_QT_THREAD)
    {
      ctx->project_ = std::unique_ptr<Project> (clone (is_backup));
    }
  else
    {
      Project *  cloned_prj = nullptr;
      QThread *  currentThread = QThread::currentThread ();
      QEventLoop loop;
      QMetaObject::invokeMethod (
        QCoreApplication::instance (),
        [this, currentThread, is_backup, &cloned_prj, &loop] () {
          cloned_prj = clone (is_backup);

          // need to move the temporary cloned project to the outer scope's
          // thread, because it will be free'd on that thread too
          cloned_prj->moveToThread (currentThread);
          loop.quit ();
        },
        Qt::QueuedConnection);
      loop.exec ();
      ctx->project_ = std::unique_ptr<Project> (cloned_prj);
    }

  if (is_backup)
    {
      /* copy plugin states */
      auto prj_pl_states_dir = get_path (ProjectPath::PLUGINS, false);
      auto prj_backup_pl_states_dir = get_path (ProjectPath::PLUGINS, true);
      try
        {
          io_copy_dir (prj_backup_pl_states_dir, prj_pl_states_dir, false, true);
        }
      catch (const ZrythmException &e)
        {
          throw ZrythmException (_ ("Failed to copy plugin states"));
        }
    }

  if (!is_backup)
    {
      /* cleanup unused plugin states */
      ctx->project_->cleanup_plugin_state_dirs (*this, is_backup);
    }

  /* TODO verify all plugin states exist */

  if (async)
    {
      SerializeProjectThread save_thread (*ctx);

      /* TODO: show progress dialog */
      if (ZRYTHM_HAVE_UI && false)
        {
          g_idle_add ((GSourceFunc) idle_saved_callback, ctx.get ());

          /* show progress while saving (TODO) */
        }
      else
        {
          while (!ctx->finished_.load ())
            {
              std::this_thread::sleep_for (std::chrono::milliseconds (1));
            }
          idle_saved_callback (ctx.get ());
        }
    }
  else /* else if no async */
    {
      /* call synchronously */
      SerializeProjectThread save_thread (*ctx);
      while (save_thread.isThreadRunning ())
        {
          std::this_thread::sleep_for (std::chrono::milliseconds (1));
        }
      idle_saved_callback (ctx.get ());
    }

  /* write FINISHED file */
  {
    auto finished_file_path = get_path (ProjectPath::FINISHED_FILE, is_backup);
    io_touch_file (finished_file_path);
  }

  if (ZRYTHM_TESTING)
    tracklist_->validate ();

  auto last_action = undo_manager_->get_last_action ();
  if (is_backup)
    {
      last_action_in_last_successful_autosave_ = last_action;
    }
  else
    {
      last_saved_action_ = last_action;
    }

  if (engine_paused)
    {
      audio_engine_->resume (state);
    }

  z_info (
    "Saved project at {}, is backup: {}, show notification: {}, async: {}",
    _dir, is_backup, show_notification, async);
}

bool
Project::has_unsaved_changes () const
{
  /* simply check if the last performed action matches the last action when the
   * project was last saved/loaded */
  auto last_performed_action = undo_manager_->get_last_action ();
  return last_performed_action != last_saved_action_;
}

void
Project::init_after_cloning (const Project &other)
{
  z_return_if_fail (ZRYTHM_IS_QT_THREAD);
  z_debug ("cloning project...");

  title_ = other.title_;
  datetime_str_ = other.datetime_str_;
  version_ = other.version_;
  transport_ = other.transport_->clone_raw_ptr ();
  transport_->setParent (this);
  audio_engine_ = other.audio_engine_->clone_unique ();
  tracklist_ = other.tracklist_->clone_raw_ptr ();
  tracklist_->setParent (this);
  clip_editor_ = other.clip_editor_;
  timeline_ = other.timeline_->clone_raw_ptr ();
  timeline_->setParent (this);
  snap_grid_timeline_ = std::make_unique<SnapGrid> (*other.snap_grid_timeline_);
  snap_grid_editor_ = std::make_unique<SnapGrid> (*other.snap_grid_editor_);
  quantize_opts_timeline_ =
    std::make_unique<QuantizeOptions> (*other.quantize_opts_timeline_);
  quantize_opts_editor_ =
    std::make_unique<QuantizeOptions> (*other.quantize_opts_editor_);
  mixer_selections_ =
    std::make_unique<ProjectMixerSelections> (*other.mixer_selections_);
  timeline_selections_ = other.timeline_selections_->clone_unique ();
  midi_selections_ = other.midi_selections_->clone_unique ();
  chord_selections_ = other.chord_selections_->clone_unique ();
  automation_selections_ = other.automation_selections_->clone_unique ();
  audio_selections_ = other.audio_selections_->clone_unique ();
  tracklist_selections_ =
    std::make_unique<SimpleTracklistSelections> (*other.tracklist_selections_);
  tracklist_selections_->tracklist_ = tracklist_;
  region_link_group_manager_ = other.region_link_group_manager_;
  port_connections_manager_ = other.port_connections_manager_->clone_raw_ptr ();
  port_connections_manager_->setParent (this);
  midi_mappings_ = other.midi_mappings_->clone_unique ();
  undo_manager_ = other.undo_manager_->clone_unique ();

  z_debug ("finished cloning project");
}

QString
Project::getTitle () const
{
  return QString::fromStdString (title_);
}

void
Project::setTitle (const QString &title)
{
  if (title_ == title.toStdString ())
    return;

  title_ = title.toStdString ();
  Q_EMIT titleChanged (title);
}

QString
Project::getDirectory () const
{
  return QString::fromStdString (dir_);
}

void
Project::setDirectory (const QString &directory)
{
  if (dir_ == directory.toStdString ())
    return;

  dir_ = directory.toStdString ();
  Q_EMIT directoryChanged (directory);
}

Tracklist *
Project::getTracklist () const
{
  return tracklist_;
}

Timeline *
Project::getTimeline () const
{
  return timeline_;
}

Transport *
Project::getTransport () const
{
  return transport_;
}

Project *
Project::get_active_instance ()
{
  return zrythm::gui::ProjectManager::get_instance ()->getActiveProject ();
}

Project *
Project::clone (bool for_backup) const
{
  auto ret = clone_raw_ptr ();
  if (for_backup)
    {
      /* no undo history in backups */
      ret->undo_manager_.reset ();
    }
  return ret;
}