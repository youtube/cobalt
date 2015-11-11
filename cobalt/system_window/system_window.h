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

#ifndef SYSTEM_WINDOW_SYSTEM_WINDOW_H_
#define SYSTEM_WINDOW_SYSTEM_WINDOW_H_

#include "base/memory/scoped_ptr.h"
#include "cobalt/base/event_dispatcher.h"

namespace cobalt {
namespace system_window {

// A SystemWindow represents a window on desktop systems as well as
// as a mechanism for routing system events and notifications.
// The SystemWindow routes callbacks for user input, and provides the
// information necessary to create a display render target for a graphics
// system. A SystemWindow is typically created via a call to
// CreateSystemWindow(), which should be implemented on every Cobalt
// platform.
class SystemWindow {
 public:
  explicit SystemWindow(base::EventDispatcher* event_dispatcher)
      : event_dispatcher_(event_dispatcher) {}
  virtual ~SystemWindow();

  base::EventDispatcher* event_dispatcher() const { return event_dispatcher_; }

 private:
  base::EventDispatcher* event_dispatcher_;
};

// The implementation of this function should be platform specific, and will
// create and return a platform-specific system window object. The system
// window object routes callbacks for user input and provides the information
// necessary to create a display render target for a graphics system.
scoped_ptr<SystemWindow> CreateSystemWindow(base::EventDispatcher*);

}  // namespace system_window
}  // namespace cobalt

#endif  // SYSTEM_WINDOW_SYSTEM_WINDOW_H_
