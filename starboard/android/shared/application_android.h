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

#ifndef STARBOARD_ANDROID_SHARED_APPLICATION_ANDROID_H_
#define STARBOARD_ANDROID_SHARED_APPLICATION_ANDROID_H_

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "starboard/android/shared/runtime_resource_overlay.h"
#include "starboard/android/shared/starboard_bridge.h"
#include "starboard/common/log.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/shared/starboard/command_line.h"
#include "starboard/shared/starboard/queue_application.h"
#include "starboard/types.h"

namespace starboard {
namespace android {
namespace shared {

using ::starboard::shared::starboard::CommandLine;

class ApplicationAndroid
    : public ::starboard::shared::starboard::QueueApplication {
 public:
  ApplicationAndroid(std::unique_ptr<CommandLine> command_line);
  ~ApplicationAndroid();

  static ApplicationAndroid* Get() {
    return static_cast<ApplicationAndroid*>(
        ::starboard::shared::starboard::Application::Get());
  }

  int64_t app_start_with_android_fix() { return app_start_timestamp_; }

 protected:
  // --- QueueApplication overrides ---
  bool MayHaveSystemEvents() override { return false; }
  Event* WaitForSystemEventWithTimeout(int64_t time) override {
    SB_NOTIMPLEMENTED();
    return NULL;
  }
  void WakeSystemEventWait() override {}

 private:
  // starboard_bridge_ is a global singleton, use a raw pointer to not interfere
  // with it's lifecycle management.
  const raw_ptr<StarboardBridge> starboard_bridge_ =
      StarboardBridge::GetInstance();

  int64_t app_start_timestamp_ = 0;
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_APPLICATION_ANDROID_H_
