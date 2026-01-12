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
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/shared/starboard/command_line.h"

namespace starboard::android::shared {

using ::starboard::shared::starboard::CommandLine;

class ApplicationAndroid : public ::starboard::shared::starboard::Application {
 public:
  ApplicationAndroid(std::unique_ptr<CommandLine> command_line,
                     ScopedJavaGlobalRef<jobject> asset_manager,
                     const std::string& files_dir,
                     const std::string& cache_dir,
                     const std::string& native_library_dir);
  ~ApplicationAndroid();

  static ApplicationAndroid* Get() {
    return static_cast<ApplicationAndroid*>(
        ::starboard::shared::starboard::Application::Get());
  }

  int64_t app_start_with_android_fix() { return app_start_timestamp_; }

 protected:
  // --- Application pure virtual method implementations ---
  Application::Event* GetNextEvent() override;
  void Inject(Application::Event* event) override;
  void InjectTimedEvent(Application::TimedEvent* timed_event) override;
  void CancelTimedEvent(SbEventId event_id) override;
  Application::TimedEvent* GetNextDueTimedEvent() override;
  int64_t GetNextTimedEventTargetTime() override;

 private:
  // starboard_bridge_ is a global singleton, use a raw pointer to not interfere
  // with it's lifecycle management.
  const raw_ptr<StarboardBridge> starboard_bridge_ =
      StarboardBridge::GetInstance();

  int64_t app_start_timestamp_ = 0;
};

}  // namespace starboard::android::shared

#endif  // STARBOARD_ANDROID_SHARED_APPLICATION_ANDROID_H_
