// Copyright 2016 Google Inc. All Rights Reserved.
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

#include <android/looper.h>
#include <android/native_window.h>

#include "starboard/android/shared/input_events_generator.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/condition_variable.h"
#include "starboard/configuration.h"
#include "starboard/mutex.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/shared/starboard/queue_application.h"
#include "starboard/types.h"

namespace starboard {
namespace android {
namespace shared {

// Android application receiving commands and input through ALooper.
class ApplicationAndroid
    : public ::starboard::shared::starboard::QueueApplication {
 public:
  struct AndroidCommand {
    typedef enum {
      kUndefined,
      kStart,
      kResume,
      kPause,
      kStop,
      kDestroy,
      kInputQueueChanged,
      kNativeWindowCreated,
      kNativeWindowDestroyed,
      kWindowFocusGained,
      kWindowFocusLost,
    } CommandType;

    CommandType type;
    void* data;
  };

  explicit ApplicationAndroid(ALooper* looper);
  ~ApplicationAndroid() override;

  static ApplicationAndroid* Get() {
    return static_cast<ApplicationAndroid*>(
        ::starboard::shared::starboard::Application::Get());
  }

  SbWindow CreateWindow(const SbWindowOptions* options);
  bool DestroyWindow(SbWindow window);
  void SetExitOnActivityDestroy(int error_level);
  bool OnSearchRequested();
  void HandleDeepLink(const char* link_url);

  void SendAndroidCommand(AndroidCommand::CommandType type, void* data);
  void SendAndroidCommand(AndroidCommand::CommandType type) {
    SendAndroidCommand(type, NULL);
  }
  void SendKeyboardInject(SbKey key);

 protected:
  // --- Application overrides ---
  void Initialize() override;
  void Teardown() override;
  bool IsStartImmediate() override { return false; }

  // --- QueueApplication overrides ---
  bool MayHaveSystemEvents() override { return true; }
  Event* WaitForSystemEventWithTimeout(SbTime time) override;
  void WakeSystemEventWait() override;

 private:
  ALooper* looper_;
  ANativeWindow* native_window_;
  AInputQueue* input_queue_;

  // Pipes attached to the looper.
  int android_command_readfd_;
  int android_command_writefd_;
  int keyboard_inject_readfd_;
  int keyboard_inject_writefd_;

  // Synchronization for commands that change availability of Android resources
  // such as the input_queue_ and/or native_window_.
  Mutex android_command_mutex_;
  ConditionVariable android_command_condition_;

  // The last Activity lifecycle state command received.
  AndroidCommand::CommandType activity_state_;

  // The single open window, if any.
  SbWindow window_;

  scoped_ptr<InputEventsGenerator> input_events_generator_;

  bool last_is_accessibility_high_contrast_text_enabled_;

  int exit_error_level_;

  // Methods to process pipes attached to the Looper.
  void ProcessAndroidCommand();
  void ProcessAndroidInput();
  void ProcessKeyboardInject();
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_APPLICATION_ANDROID_H_
