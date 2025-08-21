// SPDX-FileCopyrightText: © 2025 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense
/*
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 * ---
 *
 * SPDX-FileCopyrightText: Copyright (c) 2021 Alexandre BIQUE
 * SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2021 Alexandre BIQUE
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * ---
 *
 */

#include "zrythm-config.h"

#include <utility>

#include "plugins/clap_plugin.h"
#include "plugins/clap_plugin_param.h"

#include <QLibrary>

#include <clap/helpers/event-list.hh>
#include <clap/helpers/host.hxx>
#include <clap/helpers/plugin-proxy.hh>
#include <clap/helpers/plugin-proxy.hxx>
#include <clap/helpers/reducing-param-queue.hh>
#include <clap/helpers/reducing-param-queue.hxx>

namespace zrythm::plugins
{
thread_local bool is_main_thread = false;

using ClapPluginProxy = clap::helpers::PluginProxy<
  clap::helpers::MisbehaviourHandler::Terminate,
  clap::helpers::CheckingLevel::Maximal>;

class ClapPlugin::ClapPluginImpl
{
  friend class ClapPlugin;

  enum PluginState
  {
    // The plugin is inactive, only the main thread uses it
    Inactive,

    // Activation failed
    InactiveWithError,

    // The plugin is active and sleeping, the audio engine can call
    // set_processing()
    ActiveAndSleeping,

    // The plugin is processing
    ActiveAndProcessing,

    // The plugin did process but is in error
    ActiveWithError,

    // The plugin is not used anymore by the audio engine and can be
    // deactivated on the main thread
    ActiveAndReadyToDeactivate,
  };
  bool isPluginActive () const;
  bool isPluginProcessing () const;
  bool isPluginSleeping () const;
  void setPluginState (PluginState state);

  /* clap host callbacks */
  void             scanParam (int32_t index);
  ClapPluginParam &checkValidParamId (
    const std::string_view &function,
    const std::string_view &param_name,
    clap_id                 param_id);
  void        checkValidParamValue (const ClapPluginParam &param, double value);
  double      getParamValue (const clap_param_info &info);
  static bool clapParamsRescanMayValueChange (uint32_t flags)
  {
    return (flags & (CLAP_PARAM_RESCAN_ALL | CLAP_PARAM_RESCAN_VALUES)) != 0u;
  }
  static bool clapParamsRescanMayInfoChange (uint32_t flags)
  {
    return (flags & (CLAP_PARAM_RESCAN_ALL | CLAP_PARAM_RESCAN_INFO)) != 0u;
  }

  void paramFlushOnMainThread ();
  void handlePluginOutputEvents ();
  void generatePluginInputEvents ();

  void setup_audio_ports_for_processing (
    int       num_audio_ins,
    int       num_audio_outs,
    nframes_t block_size);

  void setPluginWindowVisibility (bool isVisible);

private:
  AudioThreadChecker audio_thread_checker_;

  QLibrary library_;

  const clap_plugin_entry *        pluginEntry_ = nullptr;
  const clap_plugin_factory *      pluginFactory_ = nullptr;
  std::unique_ptr<ClapPluginProxy> plugin_;

  /* timers */
  clap_id                                              nextTimerId_ = 0;
  std::unordered_map<clap_id, std::unique_ptr<QTimer>> timers_;

  /* fd events */
  struct Notifiers
  {
    std::unique_ptr<QSocketNotifier> rd;
    std::unique_ptr<QSocketNotifier> wr;
  };
  std::unordered_map<int, std::unique_ptr<Notifiers>> fds_;

  /* process stuff */
  clap_audio_buffer        audioIn_{};
  clap_audio_buffer        audioOut_{};
  juce::AudioSampleBuffer  audio_in_buf_;
  std::vector<float *>     audio_in_channel_ptrs_;
  juce::AudioSampleBuffer  audio_out_buf_;
  std::vector<float *>     audio_out_channel_ptrs_;
  clap::helpers::EventList evIn_;
  clap::helpers::EventList evOut_;
  clap_process             process_{};

  /* param update queues */
  std::unordered_map<clap_id, std::unique_ptr<ClapPluginParam>> params_;

  struct AppToEngineParamQueueValue
  {
    void * cookie;
    double value;
  };

  struct EngineToAppParamQueueValue
  {
    void update (const EngineToAppParamQueueValue &v) noexcept
    {
      if (v.has_value)
        {
          has_value = true;
          value = v.value;
        }

      if (v.has_gesture)
        {
          has_gesture = true;
          is_begin = v.is_begin;
        }
    }

    bool   has_value = false;
    bool   has_gesture = false;
    bool   is_begin = false;
    double value = 0;
  };

  clap::helpers::ReducingParamQueue<clap_id, AppToEngineParamQueueValue>
    appToEngineValueQueue_;
  clap::helpers::ReducingParamQueue<clap_id, AppToEngineParamQueueValue>
    appToEngineModQueue_;
  clap::helpers::ReducingParamQueue<clap_id, EngineToAppParamQueueValue>
    engineToAppValueQueue_;

  PluginState state_{ Inactive };
  bool        stateIsDirty_ = false;

  bool scheduleRestart_ = false;
  bool scheduleDeactivate_ = false;

  bool scheduleProcess_ = true;

  bool scheduleParamFlush_ = false;

  std::unordered_map<clap_id, bool> isAdjustingParameter_;

  const char * guiApi_ = nullptr;
  bool         isGuiCreated_ = false;
  bool         isGuiVisible_ = false;
  bool         isGuiFloating_ = false;

  bool scheduleMainThreadCallback_ = false;

  // work-around the fact that stopProcessing() requires being called by an
  // audio thread for whatever reason
  std::atomic_bool force_audio_thread_check_{ false };

  PluginHostWindowFactory            host_window_factory_;
  std::unique_ptr<IPluginHostWindow> editor_;
};

ClapPlugin::ClapPlugin (
  dsp::ProcessorBase::ProcessorBaseDependencies dependencies,
  StateDirectoryParentPathProvider              state_path_provider,
  AudioThreadChecker                            audio_thread_checker,
  PluginHostWindowFactory                       host_window_factory,
  QObject *                                     parent)
    : Plugin (dependencies, std::move (state_path_provider), parent),
      ClapHostBase (
        "Zrythm",
        "Alexandros Theodotou",
        "https://www.zrythm.org",
        PACKAGE_VERSION),
      pimpl_ (std::make_unique<ClapPluginImpl> ())
{
  is_main_thread = true;

  pimpl_->audio_thread_checker_ = std::move (audio_thread_checker);
  pimpl_->host_window_factory_ = std::move (host_window_factory);

  // Connect to configuration changes
  connect (
    this, &Plugin::configurationChanged, this,
    &ClapPlugin::on_configuration_changed);

  // Connect to UI visibility changes
  connect (
    this, &Plugin::uiVisibleChanged, this,
    &ClapPlugin::on_ui_visibility_changed);

  auto bypass_ref = generate_default_bypass_param ();
  add_parameter (bypass_ref);
  bypass_id_ = bypass_ref.id ();
  auto gain_ref = generate_default_gain_param ();
  add_parameter (gain_ref);
  gain_id_ = gain_ref.id ();
}

ClapPlugin::~ClapPlugin () { }

void
ClapPlugin::on_configuration_changed ()
{
  z_debug ("configuration changed");
  auto success = load_plugin (
    std::get<fs::path> (configuration ()->descriptor ()->path_or_id_), 0);
  Q_EMIT instantiationFinished (success, {});
}

void
ClapPlugin::on_ui_visibility_changed ()
{
  if (uiVisible () && !pimpl_->isGuiVisible_)
    {
      show_editor ();
    }
  else if (!uiVisible () && pimpl_->isGuiVisible_)
    {
      hide_editor ();
    }
}

static clap_window
makeClapWindow (WId window)
{
  clap_window w{};
#ifdef Q_OS_LINUX
  w.api = CLAP_WINDOW_API_X11;
  w.x11 = window;
#elifdef Q_OS_MACX
  w.api = CLAP_WINDOW_API_COCOA;
  w.cocoa = reinterpret_cast<clap_nsview> (window);
#elifdef Q_OS_WIN
  w.api = CLAP_WINDOW_API_WIN32;
  w.win32 = reinterpret_cast<clap_hwnd> (window);
#endif

  return w;
}

void
ClapPlugin::show_editor ()
{
  assert (is_main_thread);

  if (!pimpl_->plugin_->canUseGui ())
    return;

  if (pimpl_->isGuiCreated_)
    {
      pimpl_->plugin_->guiDestroy ();
      pimpl_->isGuiCreated_ = false;
      pimpl_->isGuiVisible_ = false;
    }

  const auto getCurrentClapGuiApi = [] () -> const char * {
#if defined(Q_OS_LINUX)
    return CLAP_WINDOW_API_X11;
#elif defined(Q_OS_WIN)
    return CLAP_WINDOW_API_WIN32;
#elif defined(Q_OS_MACOS)
    return CLAP_WINDOW_API_COCOA;
#else
#  error "unsupported platform"
#endif
  };
  pimpl_->guiApi_ = getCurrentClapGuiApi ();

  pimpl_->isGuiFloating_ = false;
  if (!pimpl_->plugin_->guiIsApiSupported (pimpl_->guiApi_, false))
    {
      if (!pimpl_->plugin_->guiIsApiSupported (pimpl_->guiApi_, true))
        {
          z_warning ("could not find a suitable gui api");
          return;
        }
      pimpl_->isGuiFloating_ = true;
    }

  pimpl_->editor_ = pimpl_->host_window_factory_ (*this);

  const auto embed_id = pimpl_->editor_->getEmbedWindowId ();
  auto       w = makeClapWindow (embed_id);
  if (!pimpl_->plugin_->guiCreate (w.api, pimpl_->isGuiFloating_))
    {
      z_warning ("could not create the plugin gui");
      return;
    }

  pimpl_->isGuiCreated_ = true;
  assert (pimpl_->isGuiVisible_ == false);

  if (pimpl_->isGuiFloating_)
    {
      pimpl_->plugin_->guiSetTransient (&w);
      pimpl_->plugin_->guiSuggestTitle ("using clap-host suggested title");
    }
  else
    {
      uint32_t width = 0;
      uint32_t height = 0;

      if (!pimpl_->plugin_->guiGetSize (&width, &height))
        {
          z_warning ("could not get the size of the plugin gui");
          pimpl_->isGuiCreated_ = false;
          pimpl_->plugin_->guiDestroy ();
          return;
        }

      pimpl_->editor_->setSizeAndCenter (
        static_cast<int> (width), static_cast<int> (height));

      if (!pimpl_->plugin_->guiSetParent (&w))
        {
          z_warning ("could embbed the plugin gui");
          pimpl_->isGuiCreated_ = false;
          pimpl_->plugin_->guiDestroy ();
          return;
        }
    }

  pimpl_->setPluginWindowVisibility (true);
}

void
ClapPlugin::hide_editor ()
{
  pimpl_->setPluginWindowVisibility (false);
}

void
ClapPlugin::ClapPluginImpl::setPluginWindowVisibility (bool isVisible)
{
  assert (is_main_thread);

  if (!isGuiCreated_)
    return;

  if (isVisible && !isGuiVisible_)
    {
      plugin_->guiShow ();
      isGuiVisible_ = true;
    }
  else if (!isVisible && isGuiVisible_)
    {
      plugin_->guiHide ();
      editor_->setVisible (false);
      isGuiVisible_ = false;
    }
}

void
ClapPlugin::guiResizeHintsChanged () noexcept
{
  // TODO
}

bool
ClapPlugin::guiRequestResize (uint32_t width, uint32_t height) noexcept
{
  pimpl_->editor_->setSize (static_cast<int> (width), static_cast<int> (height));

  return true;
}

bool
ClapPlugin::guiRequestShow () noexcept
{
  setUiVisible (true);

  return true;
}

bool
ClapPlugin::guiRequestHide () noexcept
{
  setUiVisible (false);

  return true;
}

void
ClapPlugin::guiClosed (bool wasDestroyed) noexcept
{
  assert (is_main_thread);
}

bool
ClapPlugin::timerSupportRegisterTimer (
  uint32_t  periodMs,
  clap_id * timerId) noexcept
{
  assert (is_main_thread);

  // Dexed fails this check even though it uses timer so make it a warning...
  z_warn_if_fail (pimpl_->plugin_->canUseTimerSupport ());

  auto id = pimpl_->nextTimerId_++;
  *timerId = id;
  auto timer = std::make_unique<QTimer> ();

  QObject::connect (timer.get (), &QTimer::timeout, this, [this, id] {
    assert (is_main_thread);
    pimpl_->plugin_->timerSupportOnTimer (id);
  });

  auto t = timer.get ();
  pimpl_->timers_.insert_or_assign (*timerId, std::move (timer));
  t->start (static_cast<int> (periodMs));
  return true;
}

bool
ClapPlugin::timerSupportUnregisterTimer (clap_id timerId) noexcept
{
  assert (is_main_thread);

  z_warn_if_fail (pimpl_->plugin_->canUseTimerSupport ());

  auto it = pimpl_->timers_.find (timerId);
  assert (it != pimpl_->timers_.end ());

  pimpl_->timers_.erase (it);
  return true;
}

void
ClapPlugin::prepare_for_processing_impl (
  sample_rate_t sample_rate,
  nframes_t     max_block_length)
{
  assert (is_main_thread);

  if (!pimpl_->plugin_)
    return;

  pimpl_->setup_audio_ports_for_processing (
    static_cast<int> (audio_in_ports_.size ()),
    static_cast<int> (audio_out_ports_.size ()), max_block_length);

  assert (!pimpl_->isPluginActive ());
  if (!pimpl_->plugin_->activate (sample_rate, 1, max_block_length))
    {
      pimpl_->setPluginState (ClapPluginImpl::InactiveWithError);
      return;
    }

  pimpl_->scheduleProcess_ = true;
  pimpl_->setPluginState (ClapPluginImpl::ActiveAndSleeping);
}

void
ClapPlugin::release_resources_impl ()
{
  assert (is_main_thread);

  if (!pimpl_->isPluginActive ())
    return;

  if (pimpl_->state_ == ClapPluginImpl::ActiveAndProcessing)
    {
      pimpl_->force_audio_thread_check_.store (true);
      pimpl_->plugin_->stopProcessing ();
      pimpl_->force_audio_thread_check_.store (false);
    }
  pimpl_->setPluginState (ClapPluginImpl::ActiveAndReadyToDeactivate);
  pimpl_->scheduleDeactivate_ = false;

  pimpl_->plugin_->deactivate ();
  pimpl_->setPluginState (ClapPluginImpl::Inactive);
}

void
ClapPlugin::process_impl (EngineProcessTimeInfo time_info) noexcept
{
  pimpl_->process_.frames_count = time_info.nframes_;
  pimpl_->process_.steady_time = -1;

  assert (threadCheckIsAudioThread ());

  if (!pimpl_->plugin_)
    return;

  // Can't process a plugin that is not active
  if (!pimpl_->isPluginActive ())
    return;

  // Do we want to deactivate the plugin?
  if (pimpl_->scheduleDeactivate_)
    {
      pimpl_->scheduleDeactivate_ = false;
      if (pimpl_->state_ == ClapPluginImpl::ActiveAndProcessing)
        pimpl_->plugin_->stopProcessing ();
      pimpl_->setPluginState (ClapPluginImpl::ActiveAndReadyToDeactivate);
      return;
    }

  // We can't process a plugin which failed to start processing
  if (pimpl_->state_ == ClapPluginImpl::ActiveWithError)
    return;

  pimpl_->process_.transport = nullptr;

  pimpl_->process_.in_events = pimpl_->evIn_.clapInputEvents ();
  pimpl_->process_.out_events = pimpl_->evOut_.clapOutputEvents ();

  pimpl_->process_.audio_inputs = &pimpl_->audioIn_;
  pimpl_->process_.audio_inputs_count = 1;
  pimpl_->process_.audio_outputs = &pimpl_->audioOut_;
  pimpl_->process_.audio_outputs_count = 1;

  pimpl_->evOut_.clear ();
  pimpl_->generatePluginInputEvents ();

  if (pimpl_->isPluginSleeping ())
    {
      if (!pimpl_->scheduleProcess_ && pimpl_->evIn_.empty ())
        // The plugin is sleeping, there is no request to wake it up and there
        // are no events to process
        return;

      pimpl_->scheduleProcess_ = false;
      if (!pimpl_->plugin_->startProcessing ())
        {
          // the plugin failed to start processing
          pimpl_->setPluginState (ClapPluginImpl::ActiveWithError);
          return;
        }

      pimpl_->setPluginState (ClapPluginImpl::ActiveAndProcessing);
    }

  [[maybe_unused]] int32_t status = CLAP_PROCESS_SLEEP;
  if (pimpl_->isPluginProcessing ())
    {
      const auto local_offset = time_info.local_offset_;
      const auto nframes = time_info.nframes_;

      // Copy input audio to JUCE buffer
      for (
        const auto &[i, channel_ptr] :
        utils::views::enumerate (pimpl_->audio_in_channel_ptrs_))
        {
          utils::float_ranges::copy (
            &channel_ptr[local_offset], &audio_in_ports_[i]->buf_[local_offset],
            nframes);
        }

      // Run plugin processing
      status = pimpl_->plugin_->process (&pimpl_->process_);

      // Copy output audio from JUCE buffer
      for (
        const auto &[i, channel_ptr] :
        utils::views::enumerate (pimpl_->audio_out_channel_ptrs_))
        {
          utils::float_ranges::copy (
            &audio_out_ports_[i]->buf_[local_offset],
            &channel_ptr[local_offset], nframes);
        }
    }

  pimpl_->handlePluginOutputEvents ();

  pimpl_->evOut_.clear ();
  pimpl_->evIn_.clear ();

  pimpl_->engineToAppValueQueue_.producerDone ();

  // TODO: send plugin to sleep if possible
}

void
ClapPlugin::save_state (std::optional<fs::path> abs_state_dir)
{
}

void
ClapPlugin::load_state (std::optional<fs::path> abs_state_dir)
{
}

bool
ClapPlugin::load_plugin (const fs::path &path, int plugin_index)
{
  assert (is_main_thread);

  if (pimpl_->library_.isLoaded ())
    unload_current_plugin ();

  pimpl_->library_.setFileName (
    utils::Utf8String::from_path (path).to_qstring ());
  pimpl_->library_.setLoadHints (
    QLibrary::ResolveAllSymbolsHint
#if !defined(__has_feature) || !__has_feature(address_sanitizer)
    | QLibrary::DeepBindHint
#endif
  );
  if (!pimpl_->library_.load ())
    {
      z_warning (
        "Failed to load plugin '{}': {}", path, pimpl_->library_.errorString ());
      return false;
    }

  pimpl_->pluginEntry_ = reinterpret_cast<const struct clap_plugin_entry *> (
    pimpl_->library_.resolve ("clap_entry"));
  if (pimpl_->pluginEntry_ == nullptr)
    {
      z_warning ("Unable to resolve entry point 'clap_entry' in '{}'", path);
      pimpl_->library_.unload ();
      return false;
    }

  if (!pimpl_->pluginEntry_->init (utils::Utf8String::from_path (path).c_str ()))
    {
      z_warning ("clap_entry->init() failed for '{}'", path);
    }

  pimpl_->pluginFactory_ = static_cast<const clap_plugin_factory *> (
    pimpl_->pluginEntry_->get_factory (CLAP_PLUGIN_FACTORY_ID));

  auto count = pimpl_->pluginFactory_->get_plugin_count (pimpl_->pluginFactory_);
  if (plugin_index >= static_cast<int> (count))
    {
      z_warning (
        "plugin index {} is invalid, expected at most {}", plugin_index,
        count - 1);
      return false;
    }

  const auto * desc = pimpl_->pluginFactory_->get_plugin_descriptor (
    pimpl_->pluginFactory_, plugin_index);
  if (desc == nullptr)
    {
      z_warning ("no plugin descriptor");
      return false;
    }

  if (!clap_version_is_compatible (desc->clap_version))
    {
      z_warning (
        "Incompatible clap version: Plugin is: {}.{}.{} Host is: {}.{}.{}",
        desc->clap_version.major, desc->clap_version.minor,
        desc->clap_version.revision, CLAP_VERSION.major, CLAP_VERSION.minor,
        CLAP_VERSION.revision);
      return false;
    }

  z_info ("Loading plugin with id: {}, index: {}", desc->id, plugin_index);

  const auto * const plugin = pimpl_->pluginFactory_->create_plugin (
    pimpl_->pluginFactory_, clapHost (), desc->id);
  if (plugin == nullptr)
    {
      z_warning ("could not create the plugin with id: {}", desc->id);
      return false;
    }

  pimpl_->plugin_ = std::make_unique<ClapPluginProxy> (*plugin, *this);

  if (!pimpl_->plugin_->init ())
    {
      z_warning ("could not init the plugin with id: {}", desc->id);
      return false;
    }

  create_ports_from_clap_plugin ();
  scanParams ();
  // scanQuickControls ();

  pluginLoadedChanged (true);

  return true;
}

void
ClapPlugin::unload_current_plugin ()
{
  assert (is_main_thread);

  pluginLoadedChanged (false);

  if (!pimpl_->library_.isLoaded ())
    return;

  if (pimpl_->isGuiCreated_)
    {
      pimpl_->plugin_->guiDestroy ();
      pimpl_->isGuiCreated_ = false;
      pimpl_->isGuiVisible_ = false;
    }

  release_resources_impl ();

  if (pimpl_->plugin_)
    {
      pimpl_->plugin_->destroy ();
    }

  pimpl_->pluginEntry_->deinit ();
  pimpl_->pluginEntry_ = nullptr;

  pimpl_->library_.unload ();
}

bool
ClapPlugin::ClapPluginImpl::isPluginActive () const
{
  switch (state_)
    {
    case Inactive:
    case InactiveWithError:
      return false;
    default:
      return true;
    }
}

bool
ClapPlugin::ClapPluginImpl::isPluginProcessing () const
{
  return state_ == ActiveAndProcessing;
}

bool
ClapPlugin::ClapPluginImpl::isPluginSleeping () const
{
  return state_ == ActiveAndSleeping;
}

bool
ClapPlugin::threadCheckIsAudioThread () const noexcept
{
  if (pimpl_->force_audio_thread_check_.load ())
    {
      return true;
    }

  return pimpl_->audio_thread_checker_ ();
}
bool
ClapPlugin::threadCheckIsMainThread () const noexcept
{
  return is_main_thread;
}

void
ClapPlugin::ClapPluginImpl::generatePluginInputEvents ()
{
  appToEngineValueQueue_.consume (
    [this] (clap_id param_id, const AppToEngineParamQueueValue &value) {
      clap_event_param_value ev{};
      ev.header.time = 0;
      ev.header.type = CLAP_EVENT_PARAM_VALUE;
      ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
      ev.header.flags = 0;
      ev.header.size = sizeof (ev);
      ev.param_id = param_id;
      ev.cookie = value.cookie;
      ev.port_index = 0;
      ev.key = -1;
      ev.channel = -1;
      ev.note_id = -1;
      ev.value = value.value;
      evIn_.push (&ev.header);
    });

  appToEngineModQueue_.consume (
    [this] (clap_id param_id, const AppToEngineParamQueueValue &value) {
      clap_event_param_mod ev{};
      ev.header.time = 0;
      ev.header.type = CLAP_EVENT_PARAM_MOD;
      ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
      ev.header.flags = 0;
      ev.header.size = sizeof (ev);
      ev.param_id = param_id;
      ev.cookie = value.cookie;
      ev.port_index = 0;
      ev.key = -1;
      ev.channel = -1;
      ev.note_id = -1;
      ev.amount = value.value;
      evIn_.push (&ev.header);
    });
}

void
ClapPlugin::ClapPluginImpl::handlePluginOutputEvents ()
{
  for (uint32_t i = 0; i < evOut_.size (); ++i)
    {
      auto * h = evOut_.get (i);
      switch (h->type)
        {
        case CLAP_EVENT_PARAM_GESTURE_BEGIN:
          {
            const auto * ev =
              reinterpret_cast<const clap_event_param_gesture *> (h); // NOLINT
            bool &isAdj = isAdjustingParameter_[ev->param_id];

            if (isAdj)
              throw std::logic_error ("The plugin sent BEGIN_ADJUST twice");
            isAdj = true;

            EngineToAppParamQueueValue v;
            v.has_gesture = true;
            v.is_begin = true;
            engineToAppValueQueue_.setOrUpdate (ev->param_id, v);
            break;
          }

        case CLAP_EVENT_PARAM_GESTURE_END:
          {
            const auto * ev =
              reinterpret_cast<const clap_event_param_gesture *> (h); // NOLINT
            bool &isAdj = isAdjustingParameter_[ev->param_id];

            if (!isAdj)
              throw std::logic_error (
                "The plugin sent END_ADJUST without a preceding BEGIN_ADJUST");
            isAdj = false;
            EngineToAppParamQueueValue v;
            v.has_gesture = true;
            v.is_begin = false;
            engineToAppValueQueue_.setOrUpdate (ev->param_id, v);
            break;
          }

        case CLAP_EVENT_PARAM_VALUE:
          {
            const auto * ev =
              reinterpret_cast<const clap_event_param_value *> (h); // NOLINT
            EngineToAppParamQueueValue v;
            v.has_value = true;
            v.value = ev->value;
            engineToAppValueQueue_.setOrUpdate (ev->param_id, v);
            break;
          }
        default:
          z_debug ("unhandled plugin output event {}", h->type);
          break;
        }
    }
}

void
ClapPlugin::requestRestart () noexcept
{
  pimpl_->scheduleRestart_ = true;
}

void
ClapPlugin::requestProcess () noexcept
{
  pimpl_->scheduleProcess_ = true;
}

void
ClapPlugin::requestCallback () noexcept
{
  pimpl_->scheduleMainThreadCallback_ = true;
}

void
ClapPlugin::logLog (clap_log_severity severity, const char * message)
  const noexcept
{
  switch (severity)
    {
    case CLAP_LOG_DEBUG:
      z_debug ("{}", message);
      break;
    case CLAP_LOG_INFO:
      z_info ("{}", message);
      break;
    case CLAP_LOG_WARNING:
      z_warning ("{}", message);
      break;
    case CLAP_LOG_FATAL:
      z_error ("[fatal CLAP error] {}", message);
      break;
    case CLAP_LOG_HOST_MISBEHAVING:
      z_error ("[CLAP host misbehaving] {}", message);
      break;
    case CLAP_LOG_PLUGIN_MISBEHAVING:
      z_warning ("[CLAP plugin misbehaving] {}", message);
    case CLAP_LOG_ERROR:
    default:
      z_error ("{}", message);
    }
}

void
ClapPlugin::create_ports_from_clap_plugin ()
{
  assert (is_main_thread);
  assert (!pimpl_->isPluginActive ());

  if (pimpl_->plugin_->canUseNotePorts ())
    {
      const auto midi_in_ports = pimpl_->plugin_->notePortsCount (true);
      const auto midi_out_ports = pimpl_->plugin_->notePortsCount (false);
      for (const auto i : std::views::iota (0u, midi_in_ports))
        {
          auto port_ref =
            dependencies ().port_registry_.create_object<dsp::MidiPort> (
              utils::Utf8String::from_utf8_encoded_string (
                fmt::format ("midi_in_{}", i + 1)),
              dsp::PortFlow::Input);
          add_input_port (port_ref);
        }
      for (const auto i : std::views::iota (0u, midi_out_ports))
        {
          auto port_ref =
            dependencies ().port_registry_.create_object<dsp::MidiPort> (
              utils::Utf8String::from_utf8_encoded_string (
                fmt::format ("midi_out_{}", i + 1)),
              dsp::PortFlow::Output);
          add_output_port (port_ref);
        }
    }

  if (pimpl_->plugin_->canUseAudioPorts ())
    {
      const auto audio_in_ports = pimpl_->plugin_->audioPortsCount (true);
      const auto audio_out_ports = pimpl_->plugin_->audioPortsCount (false);
      for (const auto i : std::views::iota (0u, audio_in_ports))
        {
          clap_audio_port_info_t nfo{};
          pimpl_->plugin_->audioPortsGet (i, true, &nfo);
          for (const auto ch : std::views::iota (0u, nfo.channel_count))
            {
              auto port_ref =
                dependencies ().port_registry_.create_object<dsp::AudioPort> (
                  utils::Utf8String::from_utf8_encoded_string (
                    fmt::format (
                      "{}_ch_{}",
                      std::strlen (nfo.name) > 0
                        ? nfo.name
                        : fmt::format ("audio_in_{}", i + 1),
                      ch + 1)),
                  dsp::PortFlow::Input);
              add_input_port (port_ref);
            }
        }
      for (const auto i : std::views::iota (0u, audio_out_ports))
        {
          clap_audio_port_info_t nfo{};
          pimpl_->plugin_->audioPortsGet (i, false, &nfo);
          for (const auto ch : std::views::iota (0u, nfo.channel_count))
            {
              auto port_ref =
                dependencies ().port_registry_.create_object<dsp::AudioPort> (
                  utils::Utf8String::from_utf8_encoded_string (
                    fmt::format (
                      "{}_ch_{}",
                      std::strlen (nfo.name) > 0
                        ? nfo.name
                        : fmt::format ("audio_out_{}", i + 1),
                      ch + 1)),
                  dsp::PortFlow::Output);
              add_output_port (port_ref);
            }
        }
    }
}

void
ClapPlugin::ClapPluginImpl::setup_audio_ports_for_processing (
  int       num_audio_ins,
  int       num_audio_outs,
  nframes_t block_size)
{
  // Resize audio buffers
  audio_in_buf_.setSize (num_audio_ins, static_cast<int> (block_size));
  audio_out_buf_.setSize (num_audio_outs, static_cast<int> (block_size));

  // Setup channel pointers
  audio_in_channel_ptrs_.resize (num_audio_ins);
  audio_out_channel_ptrs_.resize (num_audio_outs);

  for (int i = 0; i < num_audio_ins; ++i)
    {
      audio_in_channel_ptrs_[i] = audio_in_buf_.getWritePointer (i);
    }

  for (int i = 0; i < num_audio_outs; ++i)
    {
      audio_out_channel_ptrs_[i] = audio_out_buf_.getWritePointer (i);
    }

  audioIn_.channel_count = num_audio_ins;
  audioIn_.data32 = audio_in_channel_ptrs_.data ();
  audioIn_.data64 = nullptr;
  audioIn_.constant_mask = 0;
  audioIn_.latency = 0;

  audioOut_.channel_count = num_audio_outs;
  audioOut_.data32 = audio_out_channel_ptrs_.data ();
  audioOut_.data64 = nullptr;
  audioOut_.constant_mask = 0;
  audioOut_.latency = 0;
}

void
ClapPlugin::ClapPluginImpl::checkValidParamValue (
  const ClapPluginParam &param,
  double                 value)
{
  assert (is_main_thread);

  if (!param.isValueValid (value))
    {
      std::ostringstream msg;
      msg << "Invalid value for param. ";
      param.printInfo (msg);
      msg << "; value: " << value;
      // std::cerr << msg.str() << std::endl;
      throw std::invalid_argument (msg.str ());
    }
}

void
ClapPlugin::scanParams ()
{
  paramsRescan (CLAP_PARAM_RESCAN_ALL);
}

void
ClapPlugin::paramsRescan (uint32_t flags) noexcept
{
  assert (is_main_thread);

  if (!pimpl_->plugin_->canUseParams ())
    return;

  // 1. it is forbidden to use CLAP_PARAM_RESCAN_ALL if the plugin is active
  assert (!pimpl_->isPluginActive () || !(flags & CLAP_PARAM_RESCAN_ALL));

  // 2. scan the params.
  auto                        count = pimpl_->plugin_->paramsCount ();
  std::unordered_set<clap_id> paramIds (count * 2);

  for (const auto i : std::views::iota (0u, count))
    {
      clap_param_info info{};
      z_return_if_fail (pimpl_->plugin_->paramsGetInfo (i, &info));

      assert (info.id != CLAP_INVALID_ID);

      auto it = pimpl_->params_.find (info.id);

      // check that the parameter is not declared twice
      assert (!paramIds.contains (info.id));
      paramIds.insert (info.id);

      if (it == pimpl_->params_.end ())
        {
          assert ((flags & CLAP_PARAM_RESCAN_ALL) != 0u);

          double value = pimpl_->getParamValue (info);
          auto   param = std::make_unique<ClapPluginParam> (info, value, this);
          pimpl_->checkValidParamValue (*param, value);
          pimpl_->params_.insert_or_assign (info.id, std::move (param));
        }
      else
        {
          // update param info
          if (!it->second->isInfoEqualTo (info))
            {
              assert (pimpl_->clapParamsRescanMayInfoChange (flags));
              assert (
                ((flags & CLAP_PARAM_RESCAN_ALL) != 0u)
                || it->second->isInfoCriticallyDifferentTo (info));
              it->second->setInfo (info);
            }

          double value = pimpl_->getParamValue (info);
          if (it->second->value () != value)
            {
              assert (pimpl_->clapParamsRescanMayValueChange (flags));

              // update param value
              pimpl_->checkValidParamValue (*it->second, value);
              it->second->setValue (value);
              it->second->setModulation (value);
            }
        }
    }

  // remove parameters which are gone
  for (auto it = pimpl_->params_.begin (); it != pimpl_->params_.end ();)
    {
      if (paramIds.contains (it->first))
        ++it;
      else
        {
          assert ((flags & CLAP_PARAM_RESCAN_ALL) != 0u);
          it = pimpl_->params_.erase (it);
        }
    }

  if ((flags & CLAP_PARAM_RESCAN_ALL) != 0u)
    paramsChanged ();
}

void
ClapPlugin::paramsClear (clap_id paramId, clap_param_clear_flags flags) noexcept
{
  assert (is_main_thread);
}

void
ClapPlugin::ClapPluginImpl::paramFlushOnMainThread ()
{
  assert (is_main_thread);

  assert (!isPluginActive ());

  scheduleParamFlush_ = false;

  evIn_.clear ();
  evOut_.clear ();

  generatePluginInputEvents ();

  if (plugin_->canUseParams ())
    plugin_->paramsFlush (evIn_.clapInputEvents (), evOut_.clapOutputEvents ());
  handlePluginOutputEvents ();

  evOut_.clear ();
  engineToAppValueQueue_.producerDone ();
}

void
ClapPlugin::paramsRequestFlush () noexcept
{
  if (!pimpl_->isPluginActive () && threadCheckIsMainThread ())
    {
      // Perform the flush immediately
      pimpl_->paramFlushOnMainThread ();
      return;
    }

  pimpl_->scheduleParamFlush_ = true;
}

double
ClapPlugin::ClapPluginImpl::getParamValue (const clap_param_info &info)
{
  assert (is_main_thread);

  if (!plugin_->canUseParams ())
    return 0;

  double value{};
  if (plugin_->paramsGetValue (info.id, &value))
    return value;

  throw std::logic_error (
    fmt::format (
      "Failed to get the param value, id: {}, name: {}, module: {}", info.id,
      info.name, info.module));
}

void
ClapPlugin::ClapPluginImpl::setPluginState (PluginState state)
{
  switch (state)
    {
    case Inactive:
      Q_ASSERT (state_ == ActiveAndReadyToDeactivate);
      break;

    case InactiveWithError:
      Q_ASSERT (state_ == Inactive);
      break;

    case ActiveAndSleeping:
      Q_ASSERT (state_ == Inactive || state_ == ActiveAndProcessing);
      break;

    case ActiveAndProcessing:
      Q_ASSERT (state_ == ActiveAndSleeping);
      break;

    case ActiveWithError:
      Q_ASSERT (state_ == ActiveAndProcessing);
      break;

    case ActiveAndReadyToDeactivate:
      Q_ASSERT (
        state_ == ActiveAndProcessing || state_ == ActiveAndSleeping
        || state_ == ActiveWithError);
      break;

    default:
      std::terminate ();
    }

  state_ = state;
}

} // namespace zrythm::plugins
