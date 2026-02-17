// Copyright 2015 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_BROWSER_APPLICATION_H_
#define COBALT_BROWSER_APPLICATION_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "cobalt/base/event_dispatcher.h"
#include "cobalt/browser/browser_module.h"
#include "cobalt/browser/metrics/cobalt_metrics_services_manager.h"
#include "cobalt/network/network_module.h"
#include "cobalt/persistent_storage/persistent_settings.h"
#include "cobalt/system_window/system_window.h"
#if SB_IS(EVERGREEN)
#include "chrome/updater/updater_module.h"
#endif
#include "starboard/event.h"

#if defined(ENABLE_WEBDRIVER)
#include "cobalt/webdriver/web_driver_module.h"
#endif

#if defined(ENABLE_DEBUGGER)
#include "cobalt/debug/console/command_manager.h"  // nogncheck
#include "cobalt/debug/remote/debug_web_server.h"  // nogncheck
#endif                                             // ENABLE_DEBUGGER

namespace cobalt {
namespace browser {

// The Application class is meant to manage the main thread's UI task
// runner. This class is not designed to be thread safe.
class Application {
 public:
  // The passed in |quit_closure| can be called internally by the Application
  // to signal that it would like to quit.
  Application(const base::Closure& quit_closure, bool should_preload,
              int64_t timestamp);
  virtual ~Application();

  // Start from a preloaded state.
  void Start(int64_t timestamp);
  void Quit();
  void HandleStarboardEvent(const SbEvent* event);

 protected:
  base::SequencedTaskRunner* task_runner() { return task_runner_; }

  // Called to handle a network event.
  void OnNetworkEvent(const base::Event* event);

  // Called to handle an application event.
  void OnApplicationEvent(SbEventType event_type, int64_t timestamp);

  // Called to handle a window size change event.
  void OnWindowSizeChangedEvent(const base::Event* event);

  void OnOnScreenKeyboardShownEvent(const base::Event* event);
  void OnOnScreenKeyboardHiddenEvent(const base::Event* event);
  void OnOnScreenKeyboardFocusedEvent(const base::Event* event);
  void OnOnScreenKeyboardBlurredEvent(const base::Event* event);
  void OnCaptionSettingsChangedEvent(const base::Event* event);
  void OnWindowOnOnlineEvent(const base::Event* event);
  void OnWindowOnOfflineEvent(const base::Event* event);

  void OnDateTimeConfigurationChangedEvent(const base::Event* event);

  // Called when a navigation occurs in the BrowserModule.
  void MainWebModuleCreated(WebModule* web_module);

  void CollectUnloadEventTimingInfo(base::TimeTicks start_time,
                                    base::TimeTicks end_time);

  // A conduit for system events.
  base::EventDispatcher event_dispatcher_;

  // Sets up the network component for requesting internet resources.
  std::unique_ptr<network::NetworkModule> network_module_;

#if SB_IS(EVERGREEN)
  // Cobalt Updater.
  std::unique_ptr<updater::UpdaterModule> updater_module_;
#endif

  // Main components of the Cobalt browser application.
  std::unique_ptr<BrowserModule> browser_module_;

  // Event callbacks.
  base::EventCallback window_size_change_event_callback_;
  base::EventCallback on_screen_keyboard_shown_event_callback_;
  base::EventCallback on_screen_keyboard_hidden_event_callback_;
  base::EventCallback on_screen_keyboard_focused_event_callback_;
  base::EventCallback on_screen_keyboard_blurred_event_callback_;
  base::EventCallback on_caption_settings_changed_event_callback_;
  base::EventCallback on_window_on_online_event_callback_;
  base::EventCallback on_window_on_offline_event_callback_;
  base::EventCallback on_date_time_configuration_changed_event_callback_;

  // Thread checkers to ensure that callbacks for network and application events
  // always occur on the same thread.
  THREAD_CHECKER(network_event_thread_checker_);
  THREAD_CHECKER(application_event_thread_checker_);

#if defined(ENABLE_WEBDRIVER)
  // WebDriver implementation with embedded HTTP server.
  std::unique_ptr<webdriver::WebDriverModule> web_driver_module_;
#endif

#if defined(ENABLE_DEBUGGER)
  // Web server to serve devtools front end. Debugging messages are sent and
  // received via a WebSocket and communicated to an embedded DebugDispatcher.
  std::unique_ptr<debug::remote::DebugWebServer> debug_web_server_;
#endif

 private:
  enum AppStatus {
    kUninitializedAppStatus,
    kRunningAppStatus,
    kBlurredAppStatus,
    kConcealedAppStatus,
    kFrozenAppStatus,
    kWillQuitAppStatus,
    kQuitAppStatus,
    kShutDownAppStatus,
  };

  enum NetworkStatus {
    kDisconnectedNetworkStatus,
    kConnectedNetworkStatus,
  };

  // Stats related

  struct CValStats {
    CValStats();

    base::CVal<base::cval::SizeInBytes, base::CValPublic> free_cpu_memory;
    base::CVal<base::cval::SizeInBytes, base::CValPublic> used_cpu_memory;
    base::CVal<base::cval::SizeInBytes, base::CValPublic>
        tracked_gpu_buffer_bytes;
    base::CVal<base::cval::SizeInBytes, base::CValPublic>
        tracked_gpu_texture_bytes;
    base::CVal<base::cval::SizeInBytes, base::CValPublic>
        tracked_gpu_renderbuffer_bytes;

    // GPU memory stats are not always available, so we put them behind
    // base::optional so that we can enable them at runtime depending on system
    // capabilities.
    base::Optional<base::CVal<base::cval::SizeInBytes, base::CValPublic> >
        free_gpu_memory;
    base::Optional<base::CVal<base::cval::SizeInBytes, base::CValPublic> >
        used_gpu_memory;

    base::CVal<int64, base::CValPublic> app_start_time;
    base::CVal<base::TimeDelta, base::CValPublic> app_lifetime;
  };

  void UpdateUserAgent();
  void UpdatePeriodicStats();
  void DispatchEventInternal(base::Event* event);

  base::Optional<int64_t> preload_timestamp_;
  base::Optional<int64_t> start_timestamp_;

  // Json PrefStore used for persistent settings.
  std::unique_ptr<persistent_storage::PersistentSettings> persistent_settings_;

  // These represent the 'document unload timing info' from the spec to be
  // passed to the next document.
  base::TimeTicks unload_event_start_time_;
  base::TimeTicks unload_event_end_time_;

  // The task runner that will handle UI events.
  base::SequencedTaskRunner* const task_runner_;

  const base::Closure quit_closure_;

  static ssize_t available_memory_;
  static int64 lifetime_in_ms_;

  CValStats c_val_stats_;

  base::RepeatingTimer stats_update_timer_;

  // Deep links are stored here until they are reported consumed.
  std::string unconsumed_deep_link_;

  // Lock for access to unconsumed_deep_link_ from different threads.
  base::Lock unconsumed_deep_link_lock_;

  int64_t deep_link_timestamp_ = 0;

  // Called when deep links are consumed.
  void OnDeepLinkConsumedCallback(const std::string& link);

  // Dispatch events for deep links.
  void DispatchDeepLink(const char* link, int64_t timestamp);
  void DispatchDeepLinkIfNotConsumed();


  // Initializes all code necessary to start telemetry/metrics gathering and
  // reporting. See go/cobalt-telemetry.
  void InitMetrics();

  // Reference to the current metrics manager, the highest level control point
  // for metrics/telemetry.
  metrics::CobaltMetricsServicesManager* metrics_services_manager_;

  DISALLOW_COPY_AND_ASSIGN(Application);
};

}  // namespace browser
}  // namespace cobalt


extern "C" {
SB_IMPORT const char* GetCobaltUserAgentString();
}

#endif  // COBALT_BROWSER_APPLICATION_H_
