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
#include "cobalt/browser/memory_settings/auto_mem.h"
#include "cobalt/browser/memory_settings/checker.h"
#include "cobalt/browser/memory_tracker/tool.h"
#include "cobalt/system_window/system_window.h"

#if defined(ENABLE_WEBDRIVER)
#include "cobalt/webdriver/web_driver_module.h"
#endif

#if defined(ENABLE_REMOTE_DEBUGGING)
#include "cobalt/debug/debug_web_server.h"
#endif

namespace cobalt {
namespace browser {

// The Application base class is meant to manage the main thread's UI
// message loop.  A platform-specific application will be created via
// CreateApplication().  This class and all of its subclasses are not designed
// to be thread safe.
class Application {
 public:
  virtual ~Application();

  void Quit();

 protected:
  explicit Application(const base::Closure& quit_closure);

  MessageLoop* message_loop() { return message_loop_; }

 private:
  // The message loop that will handle UI events.
  MessageLoop* message_loop_;

  base::Closure quit_closure_;

 protected:
  // Called to handle a network event.
  void OnNetworkEvent(const base::Event* event);

  // Called to handle an application event.
  void OnApplicationEvent(const base::Event* event);

  // Called to handle a deep link event.
  void OnDeepLinkEvent(const base::Event* event);

  // Called when a navigation occurs in the BrowserModule.
  void WebModuleRecreated();

  // A conduit for system events.
  base::EventDispatcher event_dispatcher_;

  // The main system window for our application.
  // This routes event callbacks, and provides a native window handle
  // on desktop systems.
  scoped_ptr<system_window::SystemWindow> system_window_;

  // Account manager.
  scoped_ptr<account::AccountManager> account_manager_;

  // Main components of the Cobalt browser application.
  scoped_ptr<BrowserModule> browser_module_;

  // Event callbacks.
  base::EventCallback network_event_callback_;
  base::EventCallback application_event_callback_;
  base::EventCallback deep_link_event_callback_;

  // Thread checkers to ensure that callbacks for network and application events
  // always occur on the same thread.
  base::ThreadChecker network_event_thread_checker_;
  base::ThreadChecker application_event_thread_checker_;

  // Time the application started
  base::TimeTicks start_time_;

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
    explicit CValStats(base::TimeTicks start_time);

    base::CVal<base::cval::SizeInBytes, base::CValPublic> free_cpu_memory;
    base::CVal<base::cval::SizeInBytes, base::CValPublic> used_cpu_memory;

    // GPU memory stats are not always available, so we put them behind
    // base::optional so that we can enable them at runtime depending on system
    // capabilities.
    base::optional<base::CVal<base::cval::SizeInBytes, base::CValPublic> >
        free_gpu_memory;
    base::optional<base::CVal<base::cval::SizeInBytes, base::CValPublic> >
        used_gpu_memory;

    // The total memory that is reserved by the engine, including the part that
    // is actually occupied by JS objects, and the part that is not yet.
    base::CVal<base::cval::SizeInBytes, base::CValPublic> js_reserved_memory;

    base::CVal<int64> app_start_time;
    base::CVal<base::TimeDelta, base::CValPublic> app_lifetime;
  };

  void RegisterUserLogs();
  void UpdateAndMaybeRegisterUserAgent();

  void UpdatePeriodicStats();

  math::Size InitSystemWindow(CommandLine* command_line);

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

  // Memory configuration tool.
  scoped_ptr<memory_settings::AutoMem> auto_mem_;

  // Fires memory warning once when memory exceeds specified max cpu/gpu
  // memory.
  memory_settings::Checker memory_settings_checker_;
};

// Factory method for creating an application.  It should be implemented
// per-platform to allow for platform-specific customization.  The passed
// in |quit_closure| can be called internally by the application to signal that
// it would like to quit.
scoped_ptr<Application> CreateApplication(const base::Closure& quit_closure);

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_APPLICATION_H_
