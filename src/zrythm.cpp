// SPDX-FileCopyrightText: © 2018-2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "zrythm-config.h"

#include "utils/gtest_wrapper.h"

#ifndef _WIN32
#  include <sys/mman.h>
#endif

#include "dsp/recording_manager.h"
#include "gui/backend/event_manager.h"
#include "gui/widgets/main_window.h"
#include "plugins/plugin_manager.h"
#include "project.h"
#include "settings/chord_preset_pack_manager.h"
#include "settings/g_settings_manager.h"
#include "settings/settings.h"
#include "utils/dsp.h"
#include "utils/env.h"
#include "utils/exceptions.h"
#include "utils/io.h"
#include "utils/networking.h"
#include "utils/string.h"
#include "zrythm.h"
#include "zrythm_app.h"

#include <glib/gi18n.h>

#include "Wrapper.h"
#include "gtk_wrapper.h"
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <giomm.h>
#include <glibmm.h>

JUCE_IMPLEMENT_SINGLETON (Zrythm);

void
Zrythm::pre_init (const char * exe_path, bool have_ui, bool optimized_dsp)
{
  if (exe_path)
    exe_path_ = exe_path;

  have_ui_ = have_ui;
  use_optimized_dsp_ = optimized_dsp;
  if (use_optimized_dsp_)
    {
      lsp_dsp_context_ = std::make_unique<LspDspContextRAII> ();
    }
  settings_ = std::make_unique<Settings> ();

  debug_ = (env_get_int ("ZRYTHM_DEBUG", 0) != 0);
  break_on_error_ = (env_get_int ("ZRYTHM_BREAK_ON_ERROR", 0) != 0);
  benchmarking_ = (env_get_int ("ZRYTHM_BENCHMARKING", 0) != 0);
}

void
Zrythm::init ()
{
  settings_->init ();
  recording_manager_ = std::make_unique<RecordingManager> ();
  plugin_manager_ = std::make_unique<PluginManager> ();
  chord_preset_pack_manager_ = std::make_unique<ChordPresetPackManager> (
    have_ui_ && !ZRYTHM_TESTING && !ZRYTHM_BENCHMARKING);

  if (have_ui_)
    {
      event_manager_ = std::make_unique<EventManager> ();
    }
}

void
Zrythm::add_to_recent_projects (const std::string &_filepath)
{
  recent_projects_.insert (0, _filepath.c_str ());

  /* if we are at max projects, remove the last one */
  if (recent_projects_.size () > MAX_RECENT_PROJECTS)
    {
      recent_projects_.remove (MAX_RECENT_PROJECTS);
    }

  char ** tmp = recent_projects_.getNullTerminated ();
  g_settings_set_strv (S_GENERAL, "recent-projects", (const char * const *) tmp);
  g_strfreev (tmp);
}

void
Zrythm::remove_recent_project (const std::string &filepath)
{
  recent_projects_.removeString (filepath.c_str ());

  char ** tmp = recent_projects_.getNullTerminated ();
  g_settings_set_strv (S_GENERAL, "recent-projects", (const char * const *) tmp);
  g_strfreev (tmp);
}

/**
 * Returns the version string.
 *
 * Must be g_free()'d.
 *
 * @param with_v Include a starting "v".
 */
char *
Zrythm::get_version (bool with_v)
{
  const char * ver = PACKAGE_VERSION;

  if (with_v)
    {
      if (ver[0] == 'v')
        return g_strdup (ver);
      else
        return g_strdup_printf ("v%s", ver);
    }
  else
    {
      if (ver[0] == 'v')
        return g_strdup (&ver[1]);
      else
        return g_strdup (ver);
    }
}

/**
 * Returns the version and the capabilities.
 *
 * @param buf Buffer to write the string to.
 * @param include_system_info Whether to include
 *   additional system info (for bug reports).
 */
void
Zrythm::get_version_with_capabilities (char * buf, bool include_system_info)
{
  char * ver = get_version (0);

  GString * gstr = g_string_new (nullptr);

  g_string_append_printf (
    gstr,
    "%s %s%s (%s)\n"
    "  built with %s %s for %s%s\n"
#ifdef HAVE_CARLA
    "    +carla\n"
#endif

#ifdef HAVE_JACK2
    "    +jack2\n"
#elif defined(HAVE_JACK)
    "    +jack1\n"
#endif

#ifdef MANUAL_PATH
    "    +manual\n"
#endif
#if HAVE_PULSEAUDIO
    "    +pulse\n"
#endif
#ifdef HAVE_RTMIDI
    "    +rtmidi\n"
#endif
#ifdef HAVE_RTAUDIO
    "    +rtaudio\n"
#endif
#ifdef HAVE_SDL
    "    +sdl2\n"
#endif

#if HAVE_LSP_DSP
    "    +lsp-dsp-lib\n"
#endif

    "",
    PROGRAM_NAME,

#if ZRYTHM_IS_TRIAL_VER
    "(trial) ",
#else
    "",
#endif

    ver,

#if 0
    "optimization " OPTIMIZATION
#  ifdef IS_DEBUG_BUILD
    " - debug"
#  endif
#endif
    BUILD_TYPE,

    COMPILER, COMPILER_VERSION, HOST_MACHINE_SYSTEM,

#ifdef APPIMAGE_BUILD
    " (appimage)"
#elif ZRYTHM_IS_INSTALLER_VER
    " (installer)"
#else
    ""
#endif

  );

  if (include_system_info)
    {
      g_string_append (gstr, "\n");
      char * system_nfo = get_system_info ();
      g_string_append (gstr, system_nfo);
      g_free (system_nfo);
    }

  char * str = g_string_free (gstr, false);
  strcpy (buf, str);

  g_free (str);
  g_free (ver);
}

/**
 * Returns system info (mainly used for bug
 * reports).
 */
char *
Zrythm::get_system_info (void)
{
  GString * gstr = g_string_new (nullptr);

  char * content = NULL;
  bool ret = g_file_get_contents ("/etc/os-release", &content, nullptr, nullptr);
  if (ret)
    {
      g_string_append_printf (gstr, "%s\n", content);
      g_free (content);
    }

  g_string_append_printf (
    gstr, "XDG_SESSION_TYPE=%s\n", g_getenv ("XDG_SESSION_TYPE"));
  g_string_append_printf (
    gstr, "XDG_CURRENT_DESKTOP=%s\n", g_getenv ("XDG_CURRENT_DESKTOP"));
  g_string_append_printf (
    gstr, "DESKTOP_SESSION=%s\n", g_getenv ("DESKTOP_SESSION"));

  g_string_append (gstr, "\n");

#ifdef HAVE_CARLA
  g_string_append_printf (gstr, "Carla version: %s\n", Z_CARLA_VERSION_STRING);
#endif
  g_string_append_printf (
    gstr, "GTK version: %u.%u.%u\n", gtk_get_major_version (),
    gtk_get_minor_version (), gtk_get_micro_version ());
  g_string_append_printf (
    gstr, "libadwaita version: %u.%u.%u\n", adw_get_major_version (),
    adw_get_minor_version (), adw_get_micro_version ());
  g_string_append_printf (
    gstr, "libpanel version: %u.%u.%u\n", panel_get_major_version (),
    panel_get_minor_version (), panel_get_micro_version ());

  char * str = g_string_free (gstr, false);

  return str;
}

/**
 * Returns whether the current Zrythm version is a
 * release version.
 *
 * @note This only does regex checking.
 */
bool
Zrythm::is_release (bool official)
{
#if !ZRYTHM_IS_INSTALLER_VER
  if (official)
    {
      return false;
    }
#endif

  return !string_contains_substr (PACKAGE_VERSION, "g");
}

/**
 * @param callback A GAsyncReadyCallback to call when the
 *   request is satisfied.
 * @param callback_data Data to pass to @p callback.
 */
void
Zrythm::fetch_latest_release_ver_async (
  networking::URL::GetContentsAsyncCallback callback)
{
  networking::URL ("https://www.zrythm.org/zrythm-version.txt")
    .get_page_contents_async (8000, callback);
}

/**
 * Returns whether the given release string is the latest
 * release.
 */
bool
Zrythm::is_latest_release (const char * remote_latest_release)
{
  return string_is_equal (remote_latest_release, PACKAGE_VERSION);
}

JUCE_IMPLEMENT_SINGLETON (ZrythmDirectoryManager)

std::string
ZrythmDirectoryManager::get_prefix (void)
{
#if defined(_WIN32) && ZRYTHM_IS_INSTALLER_VER
  return io_get_registry_string_val ("InstallPath");
#elif defined(__APPLE__) && ZRYTHM_IS_INSTALLER_VER
  char bundle_path[PATH_MAX];
  int  ret = io_get_bundle_path (bundle_path);
  z_return_val_if_fail (ret == 0, nullptr);
  return io_path_get_parent_dir (bundle_path);
#elif defined(APPIMAGE_BUILD)
  z_return_val_if_fail (zrythm_app->appimage_runtime_path, nullptr);
  return g_build_filename (zrythm_app->appimage_runtime_path, "usr", nullptr);
#else
  return ZRYTHM_PREFIX;
#endif
}

void
ZrythmDirectoryManager::remove_testing_dir ()
{
  if (testing_dir_.empty ())
    return;

  io_rmdir (testing_dir_.c_str (), true);
  testing_dir_.clear ();
}

const std::string &
ZrythmDirectoryManager::get_testing_dir ()
{
  if (testing_dir_.empty ())
    {
      char * new_testing_dir =
        g_dir_make_tmp ("zrythm_test_dir_XXXXXX", nullptr);
      testing_dir_ = new_testing_dir;
      g_free (new_testing_dir);
    }
  return testing_dir_;
}

std::string
ZrythmDirectoryManager::get_user_dir (bool force_default)
{
  if (ZRYTHM_TESTING || ZRYTHM_BENCHMARKING)
    {
      return get_testing_dir ();
    }

  std::string dir =
    Gio::Settings::create (GSETTINGS_ZRYTHM_PREFIX ".preferences.general.paths")
      ->get_string ("zrythm-dir");

  if (force_default || dir.length () == 0)
    {
      dir = Glib::build_filename (g_get_user_data_dir (), "zrythm");
    }

  return dir;
}

std::string
ZrythmDirectoryManager::get_default_user_dir (void)
{
  return get_user_dir (true);
}

std::string
ZrythmDirectoryManager::get_dir (ZrythmDirType type)
{
  /* handle system dirs */
  if (type < ZrythmDirType::USER_TOP)
    {
      std::string prefix = get_prefix ();

      switch (type)
        {
        case ZrythmDirType::SYSTEM_PREFIX:
          return prefix;
        case ZrythmDirType::SYSTEM_BINDIR:
          return Glib::build_filename (prefix, "bin");
        case ZrythmDirType::SYSTEM_PARENT_DATADIR:
          return Glib::build_filename (prefix, "share");
        case ZrythmDirType::SYSTEM_PARENT_LIBDIR:
          return Glib::build_filename (prefix, LIBDIR_NAME);
        case ZrythmDirType::SYSTEM_ZRYTHM_LIBDIR:
          {
            std::string parent_path =
              get_dir (ZrythmDirType::SYSTEM_PARENT_LIBDIR);
            return Glib::build_filename (parent_path, "zrythm");
          }
        case ZrythmDirType::SYSTEM_BUNDLED_PLUGINSDIR:
          {
            std::string parent_path =
              get_dir (ZrythmDirType::SYSTEM_ZRYTHM_LIBDIR);
            return Glib::build_filename (parent_path, "lv2");
          }
        case ZrythmDirType::SYSTEM_LOCALEDIR:
          return Glib::build_filename (prefix, "share", "locale");
        case ZrythmDirType::SYSTEM_SOURCEVIEW_LANGUAGE_SPECS_DIR:
          return Glib::build_filename (
            prefix, "share", "gtksourceview-5", "language-specs");
        case ZrythmDirType::SYSTEM_BUNDLED_SOURCEVIEW_LANGUAGE_SPECS_DIR:
          return Glib::build_filename (
            prefix, "share", "zrythm", "gtksourceview-5", "language-specs");
        case ZrythmDirType::SYSTEM_ZRYTHM_DATADIR:
          return Glib::build_filename (prefix, "share", "zrythm");
        case ZrythmDirType::SYSTEM_SAMPLESDIR:
          return Glib::build_filename (prefix, "share", "zrythm", "samples");
        case ZrythmDirType::SYSTEM_SCRIPTSDIR:
          return Glib::build_filename (prefix, "share", "zrythm", "scripts");
        case ZrythmDirType::SYSTEM_THEMESDIR:
          return Glib::build_filename (prefix, "share", "zrythm", "themes");
        case ZrythmDirType::SYSTEM_THEMES_CSS_DIR:
          {
            std::string parent_path = get_dir (ZrythmDirType::SYSTEM_THEMESDIR);
            return Glib::build_filename (parent_path, "css");
          }
        case ZrythmDirType::SYSTEM_THEMES_ICONS_DIR:
          {
            std::string parent_path = get_dir (ZrythmDirType::SYSTEM_THEMESDIR);
            return Glib::build_filename (parent_path, "icons");
          }
        case ZrythmDirType::SYSTEM_SPECIAL_LV2_PLUGINS_DIR:
          return Glib::build_filename (prefix, "share", "zrythm", "lv2");
        case ZrythmDirType::SYSTEM_FONTSDIR:
          return Glib::build_filename (prefix, "share", "fonts", "zrythm");
        case ZrythmDirType::SYSTEM_TEMPLATES:
          return Glib::build_filename (prefix, "share", "zrythm", "templates");
        default:
          break;
        }
    }
  /* handle user dirs */
  else
    {
      std::string user_dir = get_user_dir (false);

      switch (type)
        {
        case ZrythmDirType::USER_TOP:
          return user_dir;
        case ZrythmDirType::USER_PROJECTS:
          return Glib::build_filename (user_dir, ZRYTHM_PROJECTS_DIR);
        case ZrythmDirType::USER_TEMPLATES:
          return Glib::build_filename (user_dir, "templates");
        case ZrythmDirType::USER_LOG:
          return Glib::build_filename (user_dir, "log");
        case ZrythmDirType::USER_SCRIPTS:
          return Glib::build_filename (user_dir, "scripts");
        case ZrythmDirType::USER_THEMES:
          return Glib::build_filename (user_dir, "themes");
        case ZrythmDirType::USER_THEMES_CSS:
          return Glib::build_filename (user_dir, "themes", "css");
        case ZrythmDirType::USER_THEMES_ICONS:
          return Glib::build_filename (user_dir, "themes", "icons");
        case ZrythmDirType::USER_PROFILING:
          return Glib::build_filename (user_dir, "profiling");
        case ZrythmDirType::USER_GDB:
          return Glib::build_filename (user_dir, "gdb");
        case ZrythmDirType::USER_BACKTRACE:
          return Glib::build_filename (user_dir, "backtraces");
        default:
          break;
        }
    }

  z_return_val_if_reached ("");
}

void
Zrythm::init_user_dirs_and_files ()
{
  z_info ("initing dirs and files");

  auto * dir_mgr = ZrythmDirectoryManager::getInstance ();
  for (
    auto dir_type :
    { ZrythmDirType::USER_TOP, ZrythmDirType::USER_PROJECTS,
      ZrythmDirType::USER_TEMPLATES, ZrythmDirType::USER_LOG,
      ZrythmDirType::USER_SCRIPTS, ZrythmDirType::USER_THEMES,
      ZrythmDirType::USER_THEMES_CSS, ZrythmDirType::USER_PROFILING,
      ZrythmDirType::USER_GDB, ZrythmDirType::USER_BACKTRACE })
    {
      std::string dir = dir_mgr->get_dir (dir_type);
      z_return_if_fail (!dir.empty ());
      try
        {
          io_mkdir (dir);
        }
      catch (const ZrythmException &e)
        {
          throw ZrythmException ("Failed to init user dires and files");
        }
    }
}

void
Zrythm::init_templates ()
{
  z_info ("Initializing templates...");

  auto *      dir_mgr = ZrythmDirectoryManager::getInstance ();
  std::string user_templates_dir =
    dir_mgr->get_dir (ZrythmDirType::USER_TEMPLATES);
  try
    {
      templates_ = io_get_files_in_dir (user_templates_dir);
    }
  catch (const ZrythmException &e)
    {
      z_warning ("Failed to init user templates from {}", user_templates_dir);
    }
  if (!ZRYTHM_TESTING && !ZRYTHM_BENCHMARKING)
    {
      std::string system_templates_dir =
        dir_mgr->get_dir (ZrythmDirType::SYSTEM_TEMPLATES);
      try
        {
          templates_.addArray (io_get_files_in_dir (system_templates_dir));
        }
      catch (const ZrythmException &e)
        {
          z_warning (
            "Failed to init system templates from {}", system_templates_dir);
        }
    }

  for (auto &tmpl : this->templates_)
    {
      z_info ("Template found: {}", tmpl.toRawUTF8 ());
      if (tmpl.contains ("demo_zsong01"))
        {
          demo_template_ = tmpl.toStdString ();
        }
    }

  z_info ("done");
}

ZrythmDirectoryManager::~ZrythmDirectoryManager ()
{
  z_info ("Destroying ZrythmDirectoryManager");
  remove_testing_dir ();
  clearSingletonInstance ();
}

Zrythm::~Zrythm ()
{
  z_info ("Destroying Zrythm instance");
  clearSingletonInstance ();
}