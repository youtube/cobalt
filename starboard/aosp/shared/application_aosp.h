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

#include <atomic>
#include <cstdint>

#include "starboard/shared/starboard/queue_application.h"

namespace starboard {

// Minimal QueueApplication for AOSP.
//
// It's responsible for forwarding input events from JNI to the Starboard
// event queue and managing the native window lifetime.
//
// Lifetime and ownership: a single instance is created on the stack by
// SbRunStarboardMain() and lives for as long as Starboard runs.
//
// Threading: the event queue is thread-safe, so events may be injected from any
// thread, including the Android UI thread. Callers running on Android threads
// must reach the instance through GetIfExists(), because the Activity can call
// into Starboard before this object exists and after it is gone.
class ApplicationAOSP : public QueueApplication {
 public:
  explicit ApplicationAOSP(SbEventHandleCallback sb_event_handle_callback)
      : QueueApplication(sb_event_handle_callback) {
    g_instance.store(this, std::memory_order_release);
  }
  ~ApplicationAOSP() override {
    // Cleared here rather than in a base class so the instance stops being
    // visible before ~QueueApplication() tears the event queue down.
    g_instance.store(nullptr, std::memory_order_release);
  }

  // Returns the live application, or nullptr when there is none. Android
  // callbacks run on the UI thread that delivers events both before
  // SbRunStarboardMain() has constructed the application and after it has
  // returned, so those callers must use this instead of
  // Application::Get() because it aborts when there is no instance.
  static ApplicationAOSP* GetIfExists() {
    return g_instance.load(std::memory_order_acquire);
  }

 protected:
  bool MayHaveSystemEvents() override { return false; }
  Event* WaitForSystemEventWithTimeout(int64_t /*time*/) override {
    return nullptr;
  }
  void WakeSystemEventWait() override {}

 private:
  // The live instance, or nullptr when there is none.
  static inline std::atomic<ApplicationAOSP*> g_instance{nullptr};
};

}  // namespace starboard

#endif  // STARBOARD_AOSP_SHARED_APPLICATION_AOSP_H_
