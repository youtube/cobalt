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

#include "cobalt/webdriver/session_driver.h"

namespace cobalt {
namespace webdriver {

SessionDriver::SessionDriver(
    const protocol::SessionId& session_id,
    const NavigateCallback& navigate_callback,
    const CreateWindowDriverCallback& create_window_driver_callback)
    : session_id_(session_id),
      capabilities_(protocol::Capabilities::CreateActualCapabilities()),
      navigate_callback_(navigate_callback),
      create_window_driver_callback_(create_window_driver_callback),
      next_window_id_(0) {
  window_driver_ = create_window_driver_callback_.Run(GetUniqueWindowId());
}

WindowDriver* SessionDriver::GetWindow(const protocol::WindowId& window_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (protocol::WindowId::IsCurrent(window_id) ||
      window_driver_->window_id() == window_id) {
    return window_driver_.get();
  } else {
    return NULL;
  }
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

util::CommandResult<void> SessionDriver::Navigate(const GURL& url) {
  DCHECK(thread_checker_.CalledOnValidThread());

  protocol::WindowId current_id = window_driver_->window_id();
  // Destroy the window_driver here to ensure there are no handles to anything
  // that will be destroyed when navigating.
  window_driver_.reset();
  base::WaitableEvent finished_event(true, false);
  navigate_callback_.Run(url, base::Bind(&base::WaitableEvent::Signal,
                                         base::Unretained(&finished_event)));
  // TODO(***REMOVED***): Implement timeout logic.
  finished_event.Wait();
  // Create a new WindowDriver using the same ID. Even though we've created a
  // new Window and WindowDriver, it should appear as though the navigation
  // happened within the same window.
  window_driver_ = create_window_driver_callback_.Run(current_id);

  return util::CommandResult<void>(protocol::Response::kSuccess);
}

util::CommandResult<std::string> SessionDriver::GetAlertText() {
  return util::CommandResult<std::string>(
      protocol::Response::kNoAlertOpenError);
}

util::CommandResult<void> SessionDriver::SwitchToWindow(
    const protocol::WindowId& window_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (window_id == window_driver_->window_id()) {
    return util::CommandResult<void>(protocol::Response::kSuccess);
  } else {
    return util::CommandResult<void>(protocol::Response::kNoSuchWindow);
  }
}

protocol::WindowId SessionDriver::GetUniqueWindowId() {
  DCHECK(thread_checker_.CalledOnValidThread());
  std::string window_id = base::StringPrintf("window-%d", next_window_id_++);
  return protocol::WindowId(window_id);
}

}  // namespace webdriver
}  // namespace cobalt
