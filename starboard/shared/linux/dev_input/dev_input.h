// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_LINUX_DEV_INPUT_DEV_INPUT_H_
#define STARBOARD_SHARED_LINUX_DEV_INPUT_DEV_INPUT_H_

#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/time.h"
#include "starboard/window.h"

namespace starboard {
namespace shared {
namespace dev_input {

// A class that wraps /dev/input, providing enough functionality to be used in a
// Starboard Application implementation.
class DevInput {
 public:
  typedef ::starboard::shared::starboard::Application::Event Event;

  virtual ~DevInput() {}

  // Returns an event if one exists, otherwise returns NULL. The caller is
  // responsible for deleting the returned event.
  virtual Event* PollNextSystemEvent() = 0;

  // Waits for an event until the timeout |time| runs out. If an event occurs in
  // this time, it is returned, otherwise NULL is returned. The caller is
  // responsible for deleting the returned event.
  virtual Event* WaitForSystemEventWithTimeout(SbTime duration) = 0;

  // Wakes up any thread waiting within a call to
  // WaitForSystemEventWithTimeout().
  virtual void WakeSystemEventWait() = 0;

  // Creates an instance of DevInput for the given window.
  static DevInput* Create(SbWindow window);

  // Creates an instance of DevInput for the given window.
  // The wake_up_fd will be used in WaitForSystemEventWithTimeout() to return
  // early when input is available on it.
  static DevInput* Create(SbWindow window, int wake_up_fd);

 protected:
  DevInput() {}
};

}  // namespace dev_input
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_LINUX_DEV_INPUT_DEV_INPUT_H_
