/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef BROWSER_APPLICATION_H_
#define BROWSER_APPLICATION_H_

#include "base/callback.h"
#include "base/message_loop.h"
#include "cobalt/account/account_manager.h"
#include "cobalt/base/event_dispatcher.h"
#include "cobalt/browser/browser_module.h"
#include "cobalt/system_window/system_window.h"

#if defined(ENABLE_WEBDRIVER)
#include "cobalt/webdriver/web_driver_module.h"
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

  void Run();
  void Quit();

 protected:
  Application();

  MessageLoop* message_loop() { return &message_loop_; }

 private:
  // The message loop that will handle UI events.
  MessageLoop message_loop_;

  base::Closure quit_closure_;

 protected:
  // Called to handle an event from the account manager.
  void OnAccountEvent(const base::Event* event);

  // A conduit for system events.
  base::EventDispatcher event_dispatcher_;

  // The main system window for our application.
  // This routes event callbacks, and provides a native window handle
  // on desktop systems.
  scoped_ptr<system_window::SystemWindow> system_window_;

  // Account manager with platform-specific implementation (e.g. PSN on PS3).
  scoped_ptr<account::AccountManager> account_manager_;

  // Main components of the Cobalt browser application.
  scoped_ptr<BrowserModule> browser_module_;

  base::EventCallback account_event_callback_;

#if defined(ENABLE_WEBDRIVER)
  // WebDriver implementation with embedded HTTP server.
  scoped_ptr<webdriver::WebDriverModule> web_driver_module_;
#endif
};

// Factory method for creating an application.  It should be implemented
// per-platform to allow for platform-specific customization.
scoped_ptr<Application> CreateApplication();

}  // namespace browser
}  // namespace cobalt

#endif  // BROWSER_APPLICATION_H_
