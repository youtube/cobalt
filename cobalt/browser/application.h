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
#include "cobalt/browser/browser_module.h"
#include "cobalt/system_window/create_system_window.h"

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

 protected:
  Application();

  MessageLoop* ui_message_loop() { return &ui_message_loop_; }
  void Quit();

 private:
  // The message loop that will handle UI events.
  MessageLoop ui_message_loop_;

  base::Closure quit_closure_;

 protected:
  // The main system window for our application.
  // This routes event callbacks, and provides a native window handle
  // on desktop systems.
  scoped_ptr<system_window::SystemWindow> main_system_window_;

  // Main components of the Cobalt browser application.
  scoped_ptr<BrowserModule> browser_module_;
};

// Factory method for creating an application.  It should be implemented
// per-platform to allow for platform-specific customization.
scoped_ptr<Application> CreateApplication();

}  // namespace browser
}  // namespace cobalt

#endif  // BROWSER_APPLICATION_H_
