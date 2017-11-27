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

#include <android/native_activity.h>
#include <android_native_app_glue.h>

#include "starboard/android/shared/input_events_generator.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/configuration.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/shared/starboard/queue_application.h"
#include "starboard/types.h"

namespace starboard {
namespace android {
namespace shared {

// Android application using android_native_app_glue and the generic queue.
class ApplicationAndroid
    : public ::starboard::shared::starboard::QueueApplication {
 public:
  explicit ApplicationAndroid(struct android_app* android_state);
  ~ApplicationAndroid() override;

  static ApplicationAndroid* Get() {
    return static_cast<ApplicationAndroid*>(
        ::starboard::shared::starboard::Application::Get());
  }

  SbWindow CreateWindow(const SbWindowOptions* options);
  bool DestroyWindow(SbWindow window);
  ANativeActivity* GetActivity();
  void SetExitOnActivityDestroy(int error_level);
  bool OnSearchRequested();
  void HandleDeepLink(const char* link_url);
  void TriggerKeyboardInject(SbKey key);
  void OnKeyboardInject();

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
  // Helpers to bind to android_native_app_glue
  friend void ::android_main(struct android_app* state);
  static void HandleCommand(struct android_app* app, int32_t cmd);
  static int32_t HandleInput(struct android_app* app, AInputEvent* event);

  void OnAndroidCommand(int32_t cmd);
  bool OnAndroidInput(AInputEvent* event);

  // The application state from android_native_app_glue.
  struct android_app* android_state_;

  int keyboard_inject_readfd_;
  int keyboard_inject_writefd_;

  // The single open window, if any.
  SbWindow window_;
  scoped_ptr<InputEventsGenerator> input_events_generator_;

  bool last_is_accessibility_high_contrast_text_enabled_;

  int exit_error_level_;

  android_poll_source keyboard_inject_source_;
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_APPLICATION_ANDROID_H_
