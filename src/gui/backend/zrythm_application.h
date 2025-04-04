// SPDX-FileCopyrightText: © 2019-2024 Alexandros Theodotou <alex@zrythm.org>
/* SPDX-License-Identifier: LicenseRef-ZrythmLicense */

#ifndef ZRYTHM_APPLICATION_H_INCLUDED
#define ZRYTHM_APPLICATION_H_INCLUDED

#include "zrythm-config.h"

#include "gui/backend/alert_manager.h"
#include "gui/backend/backend/settings_manager.h"
#include "gui/backend/backend/theme_manager.h"
#include "gui/backend/project_manager.h"
#include "gui/backend/translation_manager.h"
#include "utils/directory_manager.h"
#include "utils/rt_thread_id.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QLocalSocket>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QTranslator>

namespace zrythm::gui
{

class ZrythmJuceApplicationWrapper : public juce::JUCEApplicationBase
{
  void               initialise (const juce::String &command_line) override { }
  void               shutdown () override { }
  const juce::String getApplicationName () override { return "Zrythm"; }
  const juce::String getApplicationVersion () override { return "2.0"; }
  bool               moreThanOneInstanceAllowed () override { return true; }
  void anotherInstanceStarted (const juce::String &commandLine) override { }
  void systemRequestedQuit () override { }
  void suspended () override { }
  void resumed () override { }
  void unhandledException (
    const std::exception *,
    const juce::String &sourceFilename,
    int                 lineNumber) override
  {
  }
};

class ZrythmApplication final : public QApplication
{
  Q_OBJECT

public:
  ZrythmApplication (int &argc, char ** argv);

  ~ZrythmApplication () override;

  void setup_ui ();
  void setup_ipc ();
  void launch_engine_process ();

  DirectoryManager    &get_directory_manager () const { return *dir_manager_; }
  AlertManager *    get_alert_manager () const;
  SettingsManager * get_settings_manager () const;
  ThemeManager *    get_theme_manager () const;
  ProjectManager *  get_project_manager () const;
  TranslationManager * get_translation_manager () const;

  QQmlApplicationEngine * get_qml_engine () const { return qml_engine_; }

  bool notify (QObject * receiver, QEvent * event) override;

private:
  void setup_command_line_options ();

  void post_exec_initialization ();

private Q_SLOTS:
  void onEngineOutput ();
  void onAboutToQuit ();

public:
  RTThreadId::IdType qt_thread_id_;
  QCommandLineParser cmd_line_parser_;

private:
  /**
   * @brief Socket for communicating with the engine process.
   */
  QLocalSocket * socket_ = nullptr;

  std::unique_ptr<DirectoryManager> dir_manager_;
  AlertManager *    alert_manager_ = nullptr;
  SettingsManager * settings_manager_ = nullptr;
  ThemeManager *    theme_manager_ = nullptr;
  ProjectManager *  project_manager_ = nullptr;
  TranslationManager * translation_manager_ = nullptr;

  /**
   * @brief Engine process handle.
   */
  QProcess * engine_process_ = nullptr;

  QQmlApplicationEngine * qml_engine_ = nullptr;

  QTranslator * translator_ = nullptr;

  std::unique_ptr<ZrythmJuceApplicationWrapper> juce_app_wrapper_;
};

} // namespace zrythm::gui

/**
 * @}
 */

#endif /* ZRYTHM_APPLICATION_H_INCLUDED */
