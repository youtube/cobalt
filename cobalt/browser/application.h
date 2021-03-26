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
#include "base/message_loop/message_loop.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "cobalt/account/account_manager.h"
#include "cobalt/base/event_dispatcher.h"
#include "cobalt/browser/browser_module.h"
#include "cobalt/browser/memory_tracker/tool.h"
#include "cobalt/system_window/system_window.h"
#if SB_IS(EVERGREEN)
#include "cobalt/updater/updater_module.h"
#endif
#include "starboard/event.h"

#if defined(ENABLE_WEBDRIVER)
#include "cobalt/webdriver/web_driver_module.h"
#endif

#if defined(ENABLE_DEBUGGER)
#include "cobalt/debug/console/command_manager.h"
#include "cobalt/debug/remote/debug_web_server.h"
#endif  // ENABLE_DEBUGGER

namespace cobalt {
namespace browser {

// The Application class is meant to manage the main thread's UI message
// loop. This class is not designed to be thread safe.
class Application {
 public:
  // The passed in |quit_closure| can be called internally by the Application
  // to signal that it would like to quit.
  Application(const base::Closure& quit_closure, bool should_preload,
              SbTimeMonotonic timestamp);
  virtual ~Application();

  // Start from a preloaded state.
  void Start(SbTimeMonotonic timestamp);
  void Quit();
  void HandleStarboardEvent(const SbEvent* event);

 protected:
  base::MessageLoop* message_loop() { return message_loop_; }

  // Called to handle a network event.
  void OnNetworkEvent(const base::Event* event);

  // Called to handle an application event.
  void OnApplicationEvent(SbEventType event_type,
                          SbTimeMonotonic timestamp);

  // Called to handle a window size change event.
  void OnWindowSizeChangedEvent(const base::Event* event);

#if SB_API_VERSION >= 12 || SB_HAS(ON_SCREEN_KEYBOARD)
  void OnOnScreenKeyboardShownEvent(const base::Event* event);
  void OnOnScreenKeyboardHiddenEvent(const base::Event* event);
  void OnOnScreenKeyboardFocusedEvent(const base::Event* event);
  void OnOnScreenKeyboardBlurredEvent(const base::Event* event);
  void OnOnScreenKeyboardSuggestionsUpdatedEvent(const base::Event* event);
#endif  // SB_API_VERSION >= 12 ||
        // SB_HAS(ON_SCREEN_KEYBOARD)

#if SB_API_VERSION >= 12 || SB_HAS(CAPTIONS)
  void OnCaptionSettingsChangedEvent(const base::Event* event);
#endif  // SB_API_VERSION >= 12 || SB_HAS(CAPTIONS)

  void OnWindowOnOnlineEvent(const base::Event* event);
  void OnWindowOnOfflineEvent(const base::Event* event);

#if SB_API_VERSION >= 13
  void OnDateTimeConfigurationChangedEvent(const base::Event* event);
#endif

  // Called when a navigation occurs in the BrowserModule.
  void WebModuleCreated();

  // A conduit for system events.
  base::EventDispatcher event_dispatcher_;

  // Account manager.
  std::unique_ptr<account::AccountManager> account_manager_;

  // Storage manager used by the network module below.
  std::unique_ptr<storage::StorageManager> storage_manager_;

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
#if SB_API_VERSION >= 12 || SB_HAS(ON_SCREEN_KEYBOARD)
  base::EventCallback on_screen_keyboard_shown_event_callback_;
  base::EventCallback on_screen_keyboard_hidden_event_callback_;
  base::EventCallback on_screen_keyboard_focused_event_callback_;
  base::EventCallback on_screen_keyboard_blurred_event_callback_;
  base::EventCallback on_screen_keyboard_suggestions_updated_event_callback_;
#endif  // SB_API_VERSION >= 12 ||
        // SB_HAS(ON_SCREEN_KEYBOARD)
#if SB_API_VERSION >= 12 || SB_HAS(CAPTIONS)
  base::EventCallback on_caption_settings_changed_event_callback_;
#endif  // SB_API_VERSION >= 12 || SB_HAS(CAPTIONS)
#if SB_API_VERSION >= SB_NETWORK_EVENT_VERSION
  base::EventCallback on_window_on_online_event_callback_;
  base::EventCallback on_window_on_offline_event_callback_;
#endif
#if SB_API_VERSION >= 13
  base::EventCallback on_date_time_configuration_changed_event_callback_;
#endif

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

  // The message loop that will handle UI events.
  base::MessageLoop* message_loop_;

  const base::Closure quit_closure_;

  static ssize_t available_memory_;
  static int64 lifetime_in_ms_;

  CValStats c_val_stats_;

  base::RepeatingTimer stats_update_timer_;

#if defined(ENABLE_DEBUGGER) && defined(STARBOARD_ALLOWS_MEMORY_TRACKING)
  std::unique_ptr<memory_tracker::Tool> memory_tracker_tool_;

  // Command handler object for creating a memory tracker.
  debug::console::ConsoleCommandManager::CommandHandler
      memory_tracker_command_handler_;

  // Create a memory tracker with the given message
  void OnMemoryTrackerCommand(const std::string& message);
#endif  // defined(ENABLE_DEBUGGER) && defined(STARBOARD_ALLOWS_MEMORY_TRACKING)

  // Deep links are stored here until they are reported consumed.
  std::string unconsumed_deep_link_;

  // Lock for access to unconsumed_deep_link_ from different threads.
  base::Lock unconsumed_deep_link_lock_;

  SbTimeMonotonic deep_link_timestamp_ = 0;

  // Called when deep links are consumed.
  void OnDeepLinkConsumedCallback(const std::string& link);

  // Dispatch events for deep links.
  void DispatchDeepLink(const char* link,
                        SbTimeMonotonic timestamp);
  void DispatchDeepLinkIfNotConsumed();

  DISALLOW_COPY_AND_ASSIGN(Application);
};

}  // namespace browser
}  // namespace cobalt


extern "C" {
SB_IMPORT const char* GetCobaltUserAgentString();
}

#endif  // COBALT_BROWSER_APPLICATION_H_
