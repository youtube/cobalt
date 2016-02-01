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

#ifndef COBALT_WEBDRIVER_SESSION_DRIVER_H_
#define COBALT_WEBDRIVER_SESSION_DRIVER_H_

#if defined(ENABLE_WEBDRIVER)

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop_proxy.h"
#include "cobalt/dom/window.h"
#include "cobalt/webdriver/protocol/capabilities.h"
#include "cobalt/webdriver/protocol/session_id.h"
#include "cobalt/webdriver/util/command_result.h"
#include "cobalt/webdriver/window_driver.h"
#include "googleurl/src/gurl.h"

namespace cobalt {
namespace webdriver {

class SessionDriver {
 public:
  // SessionDriver calls this to navigate to the specified URL. The provided
  // Closure will be called when navigation is complete, as defined by the
  // OnLoad event.
  typedef base::Callback<void(const GURL&, const base::Closure&)>
      NavigateCallback;

  // Factory callback to create a WindowDriver for the currently displayed
  // Window.
  typedef base::Callback<scoped_ptr<webdriver::WindowDriver>(
      const webdriver::protocol::WindowId& window_id)>
      CreateWindowDriverCallback;

  SessionDriver(const protocol::SessionId& session_id,
                const NavigateCallback& navigate_callback,
                const CreateWindowDriverCallback& create_window_driver_cb);
  const protocol::SessionId& session_id() const { return session_id_; }
  WindowDriver* GetWindow(const protocol::WindowId& window_id);
  WindowDriver* GetCurrentWindow() { return window_driver_.get(); }

  util::CommandResult<protocol::Capabilities> GetCapabilities();
  util::CommandResult<protocol::WindowId> GetCurrentWindowHandle();
  util::CommandResult<std::vector<protocol::WindowId> > GetWindowHandles();

  util::CommandResult<void> Navigate(const GURL& url);
  util::CommandResult<std::string> GetAlertText();

 private:
  protocol::WindowId GetUniqueWindowId();

  base::ThreadChecker thread_checker_;
  const protocol::SessionId session_id_;
  protocol::Capabilities capabilities_;
  NavigateCallback navigate_callback_;
  CreateWindowDriverCallback create_window_driver_callback_;
  int32 next_window_id_;
  scoped_ptr<WindowDriver> window_driver_;
};

}  // namespace webdriver
}  // namespace cobalt

#endif  // defined(ENABLE_WEBDRIVER)
#endif  // COBALT_WEBDRIVER_SESSION_DRIVER_H_
