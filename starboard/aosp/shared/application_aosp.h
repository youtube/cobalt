// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_AOSP_SHARED_APPLICATION_AOSP_H_
#define STARBOARD_AOSP_SHARED_APPLICATION_AOSP_H_

#include <cstdint>

#include "starboard/shared/starboard/queue_application.h"

namespace starboard {

// Minimal QueueApplication for AOSP.
//
// It owns the Starboard event loop and is the base application the AOSP window
// and input handling hang off. ApplicationAndroid aborts on the event-loop
// methods, so AOSP cannot use it.
//
// Lifetime and ownership: a single instance is created on the stack by
// SbRunStarboardMain() and lives for as long as Starboard runs.
class ApplicationAOSP : public QueueApplication {
 public:
  explicit ApplicationAOSP(SbEventHandleCallback sb_event_handle_callback)
      : QueueApplication(sb_event_handle_callback) {}

 protected:
  // AOSP has no native event queue for the to poll, Android delivers
  // key events to the Activity and MainActivity forwards them over JNI
  // to Inject()
  //
  // From Chromium side it happens in the same way
  // via MessagePumpUIStarboard::ScheduleWork() calling SbEventSchedule().
  //
  // So GetNextEvent() only waits on the injected queue.
  bool MayHaveSystemEvents() override { return false; }
  Event* WaitForSystemEventWithTimeout(int64_t /*time*/) override {
    return nullptr;
  }
  void WakeSystemEventWait() override {}
};

}  // namespace starboard

#endif  // STARBOARD_AOSP_SHARED_APPLICATION_AOSP_H_
