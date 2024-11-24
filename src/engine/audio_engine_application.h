// SPDX-FileCopyrightText: © 2024 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include <csignal>

#include <QCoreApplication>
#include <QLocalServer>
#include <QLocalSocket>

#include "engine/ipc_message.h"
#include "juce_wrapper.h"

namespace zrythm::engine
{
class AudioEngineJuceApplicationWrapper : public juce::JUCEApplicationBase
{
};

class AudioEngineApplication : public juce::JUCEApplication
{
public:
  AudioEngineApplication ();
  ~AudioEngineApplication () override;

  void               initialise (const juce::String &command_line) override;
  void               shutdown () override { }
  const juce::String getApplicationName () override { return "ZrythmEngine"; }
  const juce::String getApplicationVersion () override { return "1.0"; }
  bool               moreThanOneInstanceAllowed () override { return false; }
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

  static bool is_audio_engine_process ()
  {
    return qApp->applicationName () == QString::fromUtf8 ("ZrythmEngine");
  }

private:
  void setup_ipc ();

  void post_exec_initialization ();

#if 0
private Q_SLOTS:
  void onNewConnection ();
  void onDataReceived ();
#endif

private:
  QLocalServer *                    server_ = nullptr;
  QLocalSocket *                    client_connection_ = nullptr;
  std::unique_ptr<QCoreApplication> qt_app_;
};
}