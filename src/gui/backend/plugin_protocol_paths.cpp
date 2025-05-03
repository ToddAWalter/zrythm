// SPDX-FileCopyrightText: © 2018-2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "gui/backend/backend/settings_manager.h"
#include "gui/backend/backend/zrythm.h"
#include "gui/backend/plugin_protocol_paths.h"
#include "gui/backend/zrythm_application.h"
#include "utils/directory_manager.h"
#include "utils/gtest_wrapper.h"
#include "utils/io.h"

using namespace zrythm::gui::old_dsp::plugins;
using namespace zrythm;

static void
add_expanded_paths (auto &arr, const QStringList &paths_from_settings)
{
  for (const auto &path : paths_from_settings)
    {
      auto expanded_cur_path =
        utils::string::expand_env_vars (utils::qstring_to_std_string (path));
      /* split because the env might contain multiple paths */
      auto expanded_paths = utils::io::split_paths (
        utils::std_string_to_qstring (expanded_cur_path));
      for (auto &expanded_path : expanded_paths)
        {
          arr->add_path (utils::io::qstring_to_fs_path (expanded_path));
        }
    }
}

std::unique_ptr<utils::FilePathList>
PluginProtocolPaths::get_for_protocol (const Protocol::ProtocolType protocol)
{
  switch (protocol)
    {
    case Protocol::ProtocolType::VST:
      return get_vst2_paths ();
    case Protocol::ProtocolType::VST3:
      return get_vst3_paths ();
    case Protocol::ProtocolType::DSSI:
      return get_dssi_paths ();
    case Protocol::ProtocolType::LADSPA:
      return get_ladspa_paths ();
    case Protocol::ProtocolType::SFZ:
      return get_sf_paths (false);
    case Protocol::ProtocolType::SF2:
      return get_sf_paths (true);
    case Protocol::ProtocolType::CLAP:
      return get_clap_paths ();
    case Protocol::ProtocolType::JSFX:
      return get_jsfx_paths ();
    case Protocol::ProtocolType::LV2:
      return get_lv2_paths ();
    case Protocol::ProtocolType::AudioUnit:
      return get_au_paths ();
    default:
      z_return_val_if_reached (
        {}); // Return empty container for unknown protocol types
    }
}

std::string
PluginProtocolPaths::get_for_protocol_separated (
  const Protocol::ProtocolType protocol)
{
  auto paths = get_for_protocol (protocol);
  if (!paths->empty ())
    {
      std::vector<std::string> path_strings =
        std::views::transform (paths->getPaths (), utils::qstring_to_std_string)
        | std::ranges::to<std::vector> ();
      auto paths_separated = utils::string::join (
        path_strings, utils::io::get_path_separator_string ());
      return paths_separated;
    }

  return "";
}

std::unique_ptr<utils::FilePathList>
PluginProtocolPaths::get_lv2_paths ()
{
  auto ret = std::make_unique<utils::FilePathList> ();

  if (ZRYTHM_TESTING || ZRYTHM_BENCHMARKING)
    {
      /* add test plugins if testing */
      auto tests_builddir = qEnvironmentVariable ("G_TEST_BUILDDIR");
      auto root_builddir = qEnvironmentVariable ("G_TEST_BUILD_ROOT_DIR");
      z_return_val_if_fail (!tests_builddir.isEmpty (), nullptr);
      z_return_val_if_fail (!root_builddir.isEmpty (), nullptr);

      auto test_lv2_plugins =
        utils::io::qstring_to_fs_path (tests_builddir) / "lv2plugins";
      auto test_root_plugins =
        utils::io::qstring_to_fs_path (root_builddir) / "data" / "plugins";
      ret->add_path (test_lv2_plugins.string ());
      ret->add_path (test_root_plugins.string ());

      QStringList paths_from_settings = { u"${LV2_PATH}"_s, u"/usr/lib/lv2"_s };
      add_expanded_paths (ret, paths_from_settings);

      ret->print ("LV2 paths");

      return ret;
    }

  auto paths_from_settings =
    zrythm::gui::SettingsManager::get_instance ()->get_lv2_search_paths ();
  if (paths_from_settings.empty ())
    {
      /* no paths given - use default */
#ifdef _WIN32
      ret->add_path ("C:\\Program Files\\Common Files\\LV2");
#elifdef __APPLE__
      ret->add_path ("/Library/Audio/Plug-ins/LV2");
#elifdef FLATPAK_BUILD
      ret->add_path ("/app/lib/lv2");
      ret->add_path ("/app/extensions/Plugins/lv2");
#else /* non-flatpak UNIX */
      {
        auto home_lv2 =
          utils::io::qstring_to_fs_path (QDir::homePath ()) / ".lv2";
        ret->add_path (home_lv2);
      }
      ret->add_path ("/usr/lib/lv2");
      ret->add_path ("/usr/local/lib/lv2");
#  if ZRYTHM_IS_INSTALLER_VER
      ret->add_path ("/usr/lib64/lv2");
      ret->add_path ("/usr/local/lib64/lv2");
#  else  /* else if unix and not installer ver */
      if (std::string (LIBDIR_NAME) != "lib")
        {
          ret->add_path ("/usr/" LIBDIR_NAME "/lv2");
          ret->add_path ("/usr/local/" LIBDIR_NAME "/lv2");
        }
#  endif /* endif non-flatpak UNIX */
#endif   /* endif per-platform code */
    }
  else
    {
      /* use paths given */
      add_expanded_paths (ret, paths_from_settings);
    }

  /* add special paths */
  auto &dir_mgr =
    dynamic_cast<ZrythmApplication *> (qApp)->get_directory_manager ();
  auto builtin_plugins_path = dir_mgr.get_dir (
    DirectoryManager::DirectoryType::SYSTEM_BUNDLED_PLUGINSDIR);
  auto special_plugins_path = dir_mgr.get_dir (
    DirectoryManager::DirectoryType::SYSTEM_SPECIAL_LV2_PLUGINS_DIR);
  ret->add_path (builtin_plugins_path);
  ret->add_path (special_plugins_path);

  ret->print ("LV2 paths");

  return ret;
}

std::unique_ptr<utils::FilePathList>
PluginProtocolPaths::get_vst2_paths ()
{
  auto ret = std::make_unique<utils::FilePathList> ();

  if (ZRYTHM_TESTING || ZRYTHM_BENCHMARKING)
    {
      QStringList paths_from_settings = { u"${VST_PATH}"_s };
      add_expanded_paths (ret, paths_from_settings);

      ret->print ("VST2 paths");
      return ret;
    }

  auto paths_from_settings =
    gui::SettingsManager::get_instance ()->get_vst2_search_paths ();
  if (paths_from_settings.empty ())
    {
      /* no paths given - use default */
#ifdef _WIN32
      ret->add_path ("C:\\Program Files\\Common Files\\VST2");
      ret->add_path ("C:\\Program Files\\VSTPlugins");
      ret->add_path ("C:\\Program Files\\Steinberg\\VSTPlugins");
      ret->add_path ("C:\\Program Files\\Common Files\\VST2");
      ret->add_path ("C:\\Program Files\\Common Files\\Steinberg\\VST2");
#elifdef __APPLE__
      ret->add_path ("/Library/Audio/Plug-ins/VST");
#elifdef FLATPAK_BUILD
      ret->add_path ("/app/extensions/Plugins/vst");
#else /* non-flatpak UNIX */
      {
        auto home_vst = utils::io::get_home_path () / ".vst";
        ret->add_path (home_vst);
      }
      ret->add_path ("/usr/lib/vst");
      ret->add_path ("/usr/local/lib/vst");
#  if ZRYTHM_IS_INSTALLER_VER
      ret->add_path ("/usr/lib64/vst");
      ret->add_path ("/usr/local/lib64/vst");
#  else  /* else if unix and not installer ver */
      if (std::string (LIBDIR_NAME) != "lib")
        {
          ret->add_path ("/usr/" LIBDIR_NAME "/vst");
          ret->add_path ("/usr/local/" LIBDIR_NAME "/vst");
        }
#  endif /* endif non-flatpak UNIX */
#endif   /* endif per-platform code */
    }
  else
    {
      /* use paths given */
      add_expanded_paths (ret, paths_from_settings);
    }

  ret->print ("VST2 paths");

  return ret;
}

std::unique_ptr<utils::FilePathList>
PluginProtocolPaths::get_vst3_paths ()
{
  auto ret = std::make_unique<utils::FilePathList> ();

  if (ZRYTHM_TESTING || ZRYTHM_BENCHMARKING)
    {
      QStringList paths_from_settings = { u"${VST3_PATH}"_s };
      add_expanded_paths (ret, paths_from_settings);

      ret->print ("VST3 paths");
      return ret;
    }

  auto paths_from_settings =
    gui::SettingsManager::get_instance ()->get_vst3_search_paths ();
  if (paths_from_settings.empty ())
    {
      /* no paths given - use default */
#ifdef _WIN32
      ret->add_path ("C:\\Program Files\\Common Files\\VST3");
#elifdef __APPLE__
      ret->add_path ("/Library/Audio/Plug-ins/VST3");
#elifdef FLATPAK_BUILD
      ret->add_path ("/app/extensions/Plugins/vst3");
#else /* non-flatpak UNIX */
      {
        auto home_vst3 = utils::io::get_home_path () / ".vst3";
        ret->add_path (home_vst3);
      }
      ret->add_path ("/usr/lib/vst3");
      ret->add_path ("/usr/local/lib/vst3");
#  if ZRYTHM_IS_INSTALLER_VER
      ret->add_path ("/usr/lib64/vst3");
      ret->add_path ("/usr/local/lib64/vst3");
#  else  /* else if unix and not installer ver */
      if (std::string (LIBDIR_NAME) != "lib")
        {
          ret->add_path ("/usr/" LIBDIR_NAME "/vst3");
          ret->add_path ("/usr/local/" LIBDIR_NAME "/vst3");
        }
#  endif /* endif non-flatpak UNIX */
#endif   /* endif per-platform code */
    }
  else
    {
      /* use paths given */
      add_expanded_paths (ret, paths_from_settings);
    }

  ret->print ("VST3 paths");

  return ret;
}

std::unique_ptr<utils::FilePathList>
PluginProtocolPaths::get_sf_paths (bool sf2)
{
  auto ret = std::make_unique<utils::FilePathList> ();

  if (ZRYTHM_TESTING || ZRYTHM_BENCHMARKING)
    {
      ret->add_path (utils::io::get_path_separator_string ());
      return ret;
    }

  auto paths_from_settings =
    sf2 ? gui::SettingsManager::get_instance ()->get_sf2_search_paths ()
        : gui::SettingsManager::get_instance ()->get_sfz_search_paths ();
  add_expanded_paths (ret, paths_from_settings);

  return ret;
}

std::unique_ptr<utils::FilePathList>
PluginProtocolPaths::get_dssi_paths ()
{
  auto ret = std::make_unique<utils::FilePathList> ();

  if (ZRYTHM_TESTING || ZRYTHM_BENCHMARKING)
    {
      QStringList paths_from_settings = { u"${DSSI_PATH}"_s };
      add_expanded_paths (ret, paths_from_settings);

      ret->print ("DSSI paths");
      return ret;
    }

  auto paths_from_settings =
    gui::SettingsManager::get_instance ()->get_dssi_search_paths ();
  if (paths_from_settings.empty ())
    {
      /* no paths given - use default */
#if defined(FLATPAK_BUILD)
      ret->add_path ("/app/extensions/Plugins/dssi");
#else /* non-flatpak UNIX */
      {
        auto home_dssi = utils::io::get_home_path () / ".dssi";
        ret->add_path (home_dssi.string ());
      }
      ret->add_path ("/usr/lib/dssi");
      ret->add_path ("/usr/local/lib/dssi");
#  if ZRYTHM_IS_INSTALLER_VER
      ret->add_path ("/usr/lib64/dssi");
      ret->add_path ("/usr/local/lib64/dssi");
#  else  /* else if unix and not installer ver */
      if (std::string (LIBDIR_NAME) != "lib")
        {
          ret->add_path ("/usr/" LIBDIR_NAME "/dssi");
          ret->add_path ("/usr/local/" LIBDIR_NAME "/dssi");
        }
#  endif /* endif non-flatpak UNIX */
#endif   /* endif per-platform code */
    }
  else
    {
      /* use paths given */
      add_expanded_paths (ret, paths_from_settings);
    }

  ret->print ("DSSI paths");

  return ret;
}

std::unique_ptr<utils::FilePathList>
PluginProtocolPaths::get_ladspa_paths ()
{
  auto ret = std::make_unique<utils::FilePathList> ();

  if (ZRYTHM_TESTING || ZRYTHM_BENCHMARKING)
    {
      QStringList paths_from_settings = { u"${LADSPA_PATH}"_s };
      add_expanded_paths (ret, paths_from_settings);

      ret->print ("LADSPA paths");
      return ret;
    }

  auto paths_from_settings =
    gui::SettingsManager::get_instance ()->get_ladspa_search_paths ();
  if (paths_from_settings.empty ())
    {
      /* no paths given - use default */
#if defined(FLATPAK_BUILD)
      ret->add_path ("/app/extensions/Plugins/ladspa");
#else /* non-flatpak UNIX */
      ret->add_path ("/usr/lib/ladspa");
      ret->add_path ("/usr/local/lib/ladspa");
#  if ZRYTHM_IS_INSTALLER_VER
      ret->add_path ("/usr/lib64/ladspa");
      ret->add_path ("/usr/local/lib64/ladspa");
#  else  /* else if unix and not installer ver */
      if (std::string (LIBDIR_NAME) != "lib")
        {
          ret->add_path ("/usr/" LIBDIR_NAME "/ladspa");
          ret->add_path ("/usr/local/" LIBDIR_NAME "/ladspa");
        }
#  endif /* endif non-flatpak UNIX */
#endif   /* endif per-platform code */
    }
  else
    {
      /* use paths given */
      add_expanded_paths (ret, paths_from_settings);
    }

  ret->print ("LADSPA paths");

  return ret;
}

std::unique_ptr<utils::FilePathList>
PluginProtocolPaths::get_clap_paths ()
{
  auto ret = std::make_unique<utils::FilePathList> ();

#ifndef CARLA_HAVE_CLAP_SUPPORT
  return ret;
#endif

  if (ZRYTHM_TESTING || ZRYTHM_BENCHMARKING)
    {
      QStringList paths_from_settings = { u"${CLAP_PATH}"_s };
      add_expanded_paths (ret, paths_from_settings);

      ret->print ("CLAP paths");
      return ret;
    }

  auto paths_from_settings =
    gui::SettingsManager::get_instance ()->get_clap_search_paths ();
  if (paths_from_settings.empty ())
    {
      /* no paths given - use default */
#ifdef _WIN32
      ret->add_path ("C:\\Program Files\\Common Files\\CLAP");
      ret->add_path ("C:\\Program Files (x86)\\Common Files\\CLAP");
#elifdef __APPLE__
      ret->add_path ("/Library/Audio/Plug-ins/CLAP");
#elifdef FLATPAK_BUILD
      ret->add_path ("/app/extensions/Plugins/clap");
#else /* non-flatpak UNIX */
      {
        auto home_clap = utils::io::get_home_path () / ".clap";
        ret->add_path (home_clap);
      }
      ret->add_path ("/usr/lib/clap");
      ret->add_path ("/usr/local/lib/clap");
#  if ZRYTHM_IS_INSTALLER_VER
      ret->add_path ("/usr/lib64/clap");
      ret->add_path ("/usr/local/lib64/clap");
#  else  /* else if unix and not installer ver */
      if (std::string (LIBDIR_NAME) != "lib")
        {
          ret->add_path ("/usr/" LIBDIR_NAME "/clap");
          ret->add_path ("/usr/local/" LIBDIR_NAME "/clap");
        }
#  endif /* endif non-flatpak UNIX */
#endif   /* endif per-platform code */
    }
  else
    {
      /* use paths given */
      add_expanded_paths (ret, paths_from_settings);
    }

  ret->print ("CLAP paths");

  return ret;
}

std::unique_ptr<utils::FilePathList>
PluginProtocolPaths::get_jsfx_paths ()
{
  auto ret = std::make_unique<utils::FilePathList> ();

  if (ZRYTHM_TESTING || ZRYTHM_BENCHMARKING)
    {
      QStringList paths_from_settings = { u"${JSFX_PATH}"_s };
      add_expanded_paths (ret, paths_from_settings);

      ret->print ("JSFX paths");
      return ret;
    }

  auto paths_from_settings =
    gui::SettingsManager::get_instance ()->get_jsfx_search_paths ();
  if (!paths_from_settings.empty ())
    {
      /* use paths given */
      add_expanded_paths (ret, paths_from_settings);
    }

  ret->print ("JSFX paths");

  return ret;
}

std::unique_ptr<utils::FilePathList>
PluginProtocolPaths::get_au_paths ()
{
  auto ret = std::make_unique<utils::FilePathList> ();

  ret->add_path ("/Library/Audio/Plug-ins/Components");
  auto user_components =
    utils::io::get_home_path () / "Library" / "Audio" / "Plug-ins"
    / "Components";
  ret->add_path (user_components.string ());

  ret->print ("AU paths");

  return ret;
}
