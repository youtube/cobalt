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

#include "cobalt/webdriver/session_driver.h"

#include "base/logging.h"
#include "cobalt/base/log_message_handler.h"

namespace cobalt {
namespace webdriver {
namespace {

const char kBrowserLog[] = "browser";

// Default page-load timeout.
const int kPageLoadTimeoutInSeconds = 30;

// Max retries for the "can_retry" CommandResult case.
const int kMaxRetries = 5;

protocol::LogEntry::LogLevel SeverityToLogLevel(int severity) {
  switch (severity) {
    case logging::LOG_INFO:
      return protocol::LogEntry::kInfo;
    case logging::LOG_WARNING:
    case logging::LOG_ERROR:
      return protocol::LogEntry::kWarning;
    case logging::LOG_FATAL:
      return protocol::LogEntry::kSevere;
  }
  return protocol::LogEntry::kInfo;
}

}  // namespace

SessionDriver::SessionDriver(
    const protocol::SessionId& session_id,
    const CreateWindowDriverCallback& create_window_driver_callback,
    const WaitForNavigationFunction& wait_for_navigation)
    : session_id_(session_id),
      capabilities_(protocol::Capabilities::CreateActualCapabilities()),
      create_window_driver_callback_(create_window_driver_callback),
      wait_for_navigation_(wait_for_navigation),
      next_window_id_(0),
      logging_callback_id_(0) {
  logging_callback_id_ = base::LogMessageHandler::GetInstance()->AddCallback(
      base::Bind(&SessionDriver::LogMessageHandler, base::Unretained(this)));
  window_driver_ = create_window_driver_callback_.Run(GetUniqueWindowId());
}

void SessionDriver::RefreshWindowDriver() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  window_driver_ =
      create_window_driver_callback_.Run(window_driver_->window_id());
}

SessionDriver::~SessionDriver() {
  // No more calls to LogMessageHandler will be made after this, so we can
  // safely be destructed.
  base::LogMessageHandler::GetInstance()->RemoveCallback(logging_callback_id_);
}

WindowDriver* SessionDriver::GetWindow(const protocol::WindowId& window_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (protocol::WindowId::IsCurrent(window_id) ||
      window_driver_->window_id() == window_id) {
    return window_driver_.get();
  } else {
    return NULL;
  }
}

util::CommandResult<void> SessionDriver::Navigate(const GURL& url) {
  int retries = 0;
  util::CommandResult<void> result;
  do {
    result = window_driver_->Navigate(std::move(url));
  } while (result.can_retry() && (retries++ < kMaxRetries));
  if (result.is_success()) {
    // TODO: Use timeout as specified by the webdriver client.
    wait_for_navigation_.Run(
        base::TimeDelta::FromSeconds(kPageLoadTimeoutInSeconds));
  }
  return result;
}

util::CommandResult<protocol::Capabilities> SessionDriver::GetCapabilities() {
  return util::CommandResult<protocol::Capabilities>(capabilities_);
}

util::CommandResult<protocol::WindowId>
SessionDriver::GetCurrentWindowHandle() {
  return util::CommandResult<protocol::WindowId>(window_driver_->window_id());
}

util::CommandResult<std::vector<protocol::WindowId> >
SessionDriver::GetWindowHandles() {
  typedef util::CommandResult<std::vector<protocol::WindowId> > CommandResult;
  // There is only one window, so just return a list of that.
  std::vector<protocol::WindowId> window_handles;
  window_handles.push_back(window_driver_->window_id());
  return CommandResult(window_handles);
}

util::CommandResult<std::vector<std::string> > SessionDriver::GetLogTypes() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  typedef util::CommandResult<std::vector<std::string> > CommandResult;

  std::vector<std::string> log_types;
  // Only "browser" log is supported.
  log_types.push_back(kBrowserLog);
  return CommandResult(log_types);
}

util::CommandResult<std::vector<protocol::LogEntry> > SessionDriver::GetLog(
    const protocol::LogType& type) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  typedef util::CommandResult<std::vector<protocol::LogEntry> > CommandResult;
  // Return an empty log vector for unsupported log types.
  CommandResult result((LogEntryVector()));
  if (type.type() == kBrowserLog) {
    base::AutoLock auto_lock(log_lock_);
    result = CommandResult(log_entries_);
    log_entries_.clear();
  }
  return result;
}

util::CommandResult<std::string> SessionDriver::GetAlertText() {
  return util::CommandResult<std::string>(
      protocol::Response::kNoAlertOpenError);
}

util::CommandResult<void> SessionDriver::SwitchToWindow(
    const protocol::WindowId& window_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (window_id == window_driver_->window_id()) {
    return util::CommandResult<void>(protocol::Response::kSuccess);
  } else {
    return util::CommandResult<void>(protocol::Response::kNoSuchWindow);
  }
}

protocol::WindowId SessionDriver::GetUniqueWindowId() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::string window_id = base::StringPrintf("window-%d", next_window_id_++);
  return protocol::WindowId(window_id);
}

bool SessionDriver::LogMessageHandler(int severity, const char* file, int line,
                                      size_t message_start,
                                      const std::string& str) {
  // Could be called from an arbitrary thread.
  base::Time log_time = base::Time::Now();
  protocol::LogEntry::LogLevel level = SeverityToLogLevel(severity);

  base::AutoLock auto_lock(log_lock_);
  log_entries_.push_back(protocol::LogEntry(log_time, level, str));
  // Don't capture this log entry - give other handlers a shot at it.
  return false;
}

}  // namespace webdriver
}  // namespace cobalt
