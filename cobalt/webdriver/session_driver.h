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

#ifndef WEBDRIVER_SESSION_DRIVER_H_
#define WEBDRIVER_SESSION_DRIVER_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop_proxy.h"
#include "cobalt/dom/window.h"
#include "cobalt/webdriver/protocol/capabilities.h"
#include "cobalt/webdriver/protocol/session_id.h"
#include "cobalt/webdriver/window_driver.h"

namespace cobalt {
namespace webdriver {

class SessionDriver {
 public:
  SessionDriver(const protocol::SessionId& session_id,
                const base::WeakPtr<dom::Window>& window,
                const scoped_refptr<base::MessageLoopProxy>& message_loop);
  const protocol::SessionId& session_id() const { return session_id_; }
  const protocol::Capabilities* capabilities() const {
    return capabilities_.get();
  }

  WindowDriver* GetWindow(const protocol::WindowId& window_id);
  WindowDriver* GetCurrentWindow() { return window_driver_.get(); }

 private:
  const protocol::SessionId session_id_;
  scoped_ptr<protocol::Capabilities> capabilities_;
  scoped_ptr<WindowDriver> window_driver_;
};

}  // namespace webdriver
}  // namespace cobalt

#endif  // WEBDRIVER_SESSION_DRIVER_H_
