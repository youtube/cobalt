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

#ifndef STARBOARD_TVOS_SHARED_APPLICATION_DARWIN_H_
#define STARBOARD_TVOS_SHARED_APPLICATION_DARWIN_H_

#include <cstdint>
#include <memory>

#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/queue_application.h"

namespace starboard {

class CommandLine;

class ApplicationDarwin : public QueueApplication {
 public:
  explicit ApplicationDarwin(
      std::unique_ptr<::starboard::CommandLine> command_line);
  ~ApplicationDarwin() override;

  static ApplicationDarwin* Get() {
    Application* application = Application::Get();
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
  Application::Event* WaitForSystemEventWithTimeout(int64_t time) override {
    return nullptr;
  }
  void WakeSystemEventWait() override {}

 private:
  struct ObjCStorage;
  std::unique_ptr<ObjCStorage> objc_storage_;
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

}  // namespace starboard

#endif  // STARBOARD_TVOS_SHARED_APPLICATION_DARWIN_H_
