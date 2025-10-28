// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef INTERNAL_STARBOARD_SHARED_UIKIT_APPLICATION_DARWIN_H_
#define INTERNAL_STARBOARD_SHARED_UIKIT_APPLICATION_DARWIN_H_

#include <GLES2/gl2.h>

#include "starboard/configuration.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/shared/starboard/audio_sink/audio_sink_internal.h"
#include "starboard/shared/starboard/queue_application.h"
#include "starboard/types.h"

namespace starboard {
namespace shared {
namespace uikit {

class ApplicationDarwin : public shared::starboard::QueueApplication {
 public:
#if SB_API_VERSION >= 15
  explicit ApplicationDarwin(SbEventHandleCallback sb_event_handle_callback)
      : QueueApplication(sb_event_handle_callback) {}
#endif  //  SB_API_VERSION >= 15
  static ApplicationDarwin* Get() {
    shared::starboard::Application* application =
        shared::starboard::Application::Get();
    return static_cast<ApplicationDarwin*>(application);
  }

  // These are helper functions to simplify injecting application events.
  template <typename T>
  static void InjectEvent(SbEventType type, T* data);
  static void InjectEvent(SbEventType type) {
    Get()->Inject(new Event{type, nullptr, nullptr});
  }
  static void InjectEvent(SbEventType type,
                          void* data,
                          SbEventDataDestructor data_destructor) {
    Get()->Inject(new Event{type, data, data_destructor});
  }

  static void InjectEvent(SbEventType type,
                          int64_t timestamp,
                          void* data,
                          SbEventDataDestructor data_destructor) {
    Get()->Inject(new Event{type, timestamp, data, data_destructor});
  }

  static void IncrementIdleTimerLockCount();
  static void DecrementIdleTimerLockCount();

 protected:
  // --- Application overrides ---
  bool IsStartImmediate() override { return false; }
  bool MayHaveSystemEvents() override { return false; }
  shared::starboard::Application::Event* WaitForSystemEventWithTimeout(
      int64_t time) override {
    return nullptr;
  }
  void WakeSystemEventWait() override {}
  void Initialize() override;
  void Teardown() override;
};

template <typename T>
inline void ApplicationDarwin::InjectEvent(SbEventType type, T* data) {
  Get()->Inject(new Event{type, data, &ApplicationDarwin::DeleteDestructor<T>});
}

template <>
inline void ApplicationDarwin::InjectEvent<char>(SbEventType type, char* data) {
  Get()->Inject(
      new Event{type, data, &ApplicationDarwin::DeleteArrayDestructor<char>});
}

}  // namespace uikit
}  // namespace shared
}  // namespace starboard

#endif  // INTERNAL_STARBOARD_SHARED_UIKIT_APPLICATION_DARWIN_H_
