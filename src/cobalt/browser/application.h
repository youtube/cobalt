// Copyright 2015 Google Inc. All Rights Reserved.
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

#include "base/callback.h"
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/threading/thread_checker.h"
#include "cobalt/account/account_manager.h"
#include "cobalt/base/event_dispatcher.h"
#include "cobalt/browser/browser_module.h"
#include "cobalt/browser/memory_tracker/tool.h"
#include "cobalt/system_window/system_window.h"
#include "starboard/event.h"

#if defined(ENABLE_WEBDRIVER)
#include "cobalt/webdriver/web_driver_module.h"
#endif

#if defined(ENABLE_REMOTE_DEBUGGING)
#include "cobalt/debug/debug_web_server.h"
#endif

namespace cobalt {
namespace browser {

// The Application class is meant to manage the main thread's UI message
// loop. This class is not designed to be thread safe.
class Application {
 public:
  // The passed in |quit_closure| can be called internally by the Application to
  // signal that it would like to quit.
  Application(const base::Closure& quit_closure, bool should_preload);
  virtual ~Application();

  // Start from a preloaded state.
  void Start();
  void Quit();
  void HandleStarboardEvent(const SbEvent* event);

 protected:
  MessageLoop* message_loop() { return message_loop_; }

 private:
  // The message loop that will handle UI events.
  MessageLoop* message_loop_;

  const base::Closure quit_closure_;

 protected:
  // Called to handle a network event.
  void OnNetworkEvent(const base::Event* event);

  // Called to handle an application event.
  void OnApplicationEvent(SbEventType event_type);

  // Called to handle a deep link event.
  void OnDeepLinkEvent(const base::Event* event);

#if SB_API_VERSION >= 8
  // Called to handle a window size change event.
  void OnWindowSizeChangedEvent(const base::Event* event);
#endif  // SB_API_VERSION >= 8

#if SB_HAS(ON_SCREEN_KEYBOARD)
  void OnOnScreenKeyboardShownEvent(const base::Event* event);
  void OnOnScreenKeyboardHiddenEvent(const base::Event* event);
  void OnOnScreenKeyboardFocusedEvent(const base::Event* event);
  void OnOnScreenKeyboardBlurredEvent(const base::Event* event);
#endif  // SB_HAS(ON_SCREEN_KEYBOARD)

#if SB_HAS(CAPTIONS)
  void OnCaptionSettingsChangedEvent(const base::Event* event);
#endif  // SB_HAS(CAPTIONS)

  // Called when a navigation occurs in the BrowserModule.
  void WebModuleRecreated();

  // A conduit for system events.
  base::EventDispatcher event_dispatcher_;

  // Account manager.
  scoped_ptr<account::AccountManager> account_manager_;

  // Main components of the Cobalt browser application.
  scoped_ptr<BrowserModule> browser_module_;

  // Event callbacks.
  base::EventCallback network_event_callback_;
  base::EventCallback deep_link_event_callback_;
#if SB_API_VERSION >= 8
  base::EventCallback window_size_change_event_callback_;
#endif  // SB_API_VERSION >= 8
#if SB_HAS(ON_SCREEN_KEYBOARD)
  base::EventCallback on_screen_keyboard_shown_event_callback_;
  base::EventCallback on_screen_keyboard_hidden_event_callback_;
  base::EventCallback on_screen_keyboard_focused_event_callback_;
  base::EventCallback on_screen_keyboard_blurred_event_callback_;
#endif  // SB_HAS(ON_SCREEN_KEYBOARD)

  // Thread checkers to ensure that callbacks for network and application events
  // always occur on the same thread.
  base::ThreadChecker network_event_thread_checker_;
  base::ThreadChecker application_event_thread_checker_;

#if defined(ENABLE_WEBDRIVER)
  // WebDriver implementation with embedded HTTP server.
  scoped_ptr<webdriver::WebDriverModule> web_driver_module_;
#endif

#if defined(ENABLE_REMOTE_DEBUGGING)
  // Web server to serve devtools front end. Debugging messages are sent and
  // received via a WebSocket and communicated to an embedded DebugServer.
  scoped_ptr<debug::DebugWebServer> debug_web_server_;
#endif

 private:
  enum AppStatus {
    kUninitializedAppStatus,
    kPreloadingAppStatus,
    kRunningAppStatus,
    kPausedAppStatus,
    kSuspendedAppStatus,
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
    base::optional<base::CVal<base::cval::SizeInBytes, base::CValPublic> >
        free_gpu_memory;
    base::optional<base::CVal<base::cval::SizeInBytes, base::CValPublic> >
        used_gpu_memory;

    base::CVal<int64, base::CValPublic> app_start_time;
    base::CVal<base::TimeDelta, base::CValPublic> app_lifetime;
  };

  void RegisterUserLogs();
  void UpdateAndMaybeRegisterUserAgent();
  void UpdatePeriodicStats();
  void DispatchEventInternal(base::Event* event);

  static ssize_t available_memory_;
  static int64 lifetime_in_ms_;

  static AppStatus app_status_;
  static int app_suspend_count_;
  static int app_resume_count_;
  static int app_pause_count_;
  static int app_unpause_count_;

  static NetworkStatus network_status_;
  static int network_connect_count_;
  static int network_disconnect_count_;

  CValStats c_val_stats_;

  base::Timer stats_update_timer_;

  scoped_ptr<memory_tracker::Tool> memory_tracker_tool_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_APPLICATION_H_
