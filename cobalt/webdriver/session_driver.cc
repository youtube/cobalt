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
namespace {
// There is only one window supported in Cobalt, and this is its ID.
const char kWindowId[] = "window-0";

}  // namespace

SessionDriver::SessionDriver(
    const protocol::SessionId& session_id,
    const base::WeakPtr<dom::Window>& window,
    const scoped_refptr<base::MessageLoopProxy>& message_loop)
    : session_id_(session_id),
      capabilities_(protocol::Capabilities::CreateActualCapabilities()) {
  window_driver_.reset(
      new WindowDriver(protocol::WindowId(kWindowId), window, message_loop));
}

WindowDriver* SessionDriver::GetWindow(const protocol::WindowId& window_id) {
  if (protocol::WindowId::IsCurrent(window_id) ||
      window_driver_->window_id() == window_id) {
    return window_driver_.get();
  } else {
    return NULL;
  }
}


}  // namespace webdriver
}  // namespace cobalt
