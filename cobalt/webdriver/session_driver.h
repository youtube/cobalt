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

#ifndef COBALT_WEBDRIVER_SESSION_DRIVER_H_
#define COBALT_WEBDRIVER_SESSION_DRIVER_H_

#if defined(ENABLE_WEBDRIVER)

#include <memory>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "cobalt/dom/window.h"
#include "cobalt/webdriver/protocol/capabilities.h"
#include "cobalt/webdriver/protocol/log_entry.h"
#include "cobalt/webdriver/protocol/log_type.h"
#include "cobalt/webdriver/protocol/session_id.h"
#include "cobalt/webdriver/util/command_result.h"
#include "cobalt/webdriver/window_driver.h"
#include "url/gurl.h"

namespace cobalt {
namespace webdriver {

class SessionDriver {
 public:
  // Factory callback to create a WindowDriver for the currently displayed
  // Window.
  typedef base::Callback<std::unique_ptr<webdriver::WindowDriver>(
      const webdriver::protocol::WindowId& window_id)>
      CreateWindowDriverCallback;

  typedef base::Callback<bool(const base::TimeDelta&)>
      WaitForNavigationFunction;

  SessionDriver(const protocol::SessionId& session_id,
                const CreateWindowDriverCallback& create_window_driver_cb,
                const WaitForNavigationFunction& wait_for_navigation);

  void RefreshWindowDriver();

  const protocol::SessionId& session_id() const { return session_id_; }
  WindowDriver* GetWindow(const protocol::WindowId& window_id);
  WindowDriver* GetCurrentWindow() { return window_driver_.get(); }

  util::CommandResult<void> Navigate(const GURL& url);
  util::CommandResult<protocol::Capabilities> GetCapabilities();
  util::CommandResult<protocol::WindowId> GetCurrentWindowHandle();
  util::CommandResult<std::vector<protocol::WindowId> > GetWindowHandles();

  util::CommandResult<std::vector<std::string> > GetLogTypes();
  util::CommandResult<std::vector<protocol::LogEntry> > GetLog(
      const protocol::LogType& type);
  util::CommandResult<std::string> GetAlertText();
  util::CommandResult<void> SwitchToWindow(const protocol::WindowId& window_id);

  ~SessionDriver();

 private:
  protocol::WindowId GetUniqueWindowId();

  bool LogMessageHandler(int severity, const char* file, int line,
                         size_t message_start, const std::string& str);

  THREAD_CHECKER(thread_checker_);
  const protocol::SessionId session_id_;
  protocol::Capabilities capabilities_;
  CreateWindowDriverCallback create_window_driver_callback_;
  WaitForNavigationFunction wait_for_navigation_;
  int32 next_window_id_;
  std::unique_ptr<WindowDriver> window_driver_;

  int logging_callback_id_;
  typedef std::vector<protocol::LogEntry> LogEntryVector;
  LogEntryVector log_entries_;
  base::Lock log_lock_;
};

}  // namespace webdriver
}  // namespace cobalt

#endif  // defined(ENABLE_WEBDRIVER)
#endif  // COBALT_WEBDRIVER_SESSION_DRIVER_H_
