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

#include <android/looper.h>
#include <android/native_window.h>
#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// #include "game-activity/GameActivity.h"
#include "starboard/android/shared/input_events_generator.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/common/condition_variable.h"
#include "starboard/common/mutex.h"
#include "starboard/configuration.h"
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
      kNativeWindowCreated,
      kNativeWindowDestroyed,
      kWindowFocusGained,
      kWindowFocusLost,
      kDeepLink,
    } CommandType;

    CommandType type = kUndefined;
    void* data = nullptr;
  };

  ApplicationAndroid(ALooper* looper,
                     SbEventHandleCallback sb_event_handle_callback);
  ~ApplicationAndroid() override;

  static ApplicationAndroid* Get() {
    return static_cast<ApplicationAndroid*>(
        ::starboard::shared::starboard::Application::Get());
  }

  SbWindow CreateWindow(const SbWindowOptions* options);
  bool DestroyWindow(SbWindow window);
  bool OnSearchRequested();
  void HandleDeepLink(const char* link_url);
  void SendTTSChangedEvent() {
    Inject(new Event(kSbEventTypeAccessibilityTextToSpeechSettingsChanged,
                     nullptr, nullptr));
  }

  void SendAndroidCommand(AndroidCommand::CommandType type, void* data);
  void SendAndroidCommand(AndroidCommand::CommandType type) {
    SendAndroidCommand(type, NULL);
  }
  // bool SendAndroidMotionEvent(const GameActivityMotionEvent* event);
  // bool SendAndroidKeyEvent(const GameActivityKeyEvent* event);

  void SendKeyboardInject(SbKey key);

  void SbWindowSendInputEvent(const char* input_text, bool is_composing);
  void SendLowMemoryEvent();
  void OsNetworkStatusChange(bool became_online);

  int64_t app_start_with_android_fix() { return app_start_timestamp_; }

  void SendDateTimeConfigurationChangedEvent();

  // Methods to get the Runtime Resource Overlay variables.
  // All RRO variables which can be retrieved here must be defined
  // in res/values/rro_variables.xml and be loaded in
  // dev/cobalt/coat/ResourceOverlay.java.
  int GetOverlayedIntValue(const char* var_name);
  std::string GetOverlayedStringValue(const char* var_name);
  bool GetOverlayedBoolValue(const char* var_name);

 protected:
  // --- Application overrides ---
  void Initialize() override;
  void Teardown() override;
  bool IsStartImmediate() override { return false; }
  void OnResume() override;
  void OnSuspend() override;

  // --- QueueApplication overrides ---
  bool MayHaveSystemEvents() override { return handle_system_events_.load(); }
  Event* WaitForSystemEventWithTimeout(int64_t time) override;
  void WakeSystemEventWait() override;

 private:
  ALooper* looper_;
  ANativeWindow* native_window_;

  // Pipes attached to the looper.
  int android_command_readfd_;
  int android_command_writefd_;
  int keyboard_inject_readfd_;
  int keyboard_inject_writefd_;

  // In certain situations, the Starboard thread should not try to process new
  // system events (e.g. while one is being processed).
  std::atomic_bool handle_system_events_{false};

  // Synchronization for commands that change availability of Android resources
  // such as the input and/or native_window_.
  Mutex android_command_mutex_;
  ConditionVariable android_command_condition_;

  // Track queued "stop" commands to avoid starting the app when Android has
  // already requested it be stopped.
  std::atomic<int32_t> android_stop_count_{0};

  // Set to true in the destructor to ensure other threads stop waiting.
  std::atomic_bool application_destroying_{false};

  // The last Activity lifecycle state command received.
  AndroidCommand::CommandType activity_state_;

  // The single open window, if any.
  SbWindow window_;

  // |input_events_generator_| is accessed from multiple threads, so use a mutex
  // to safely access it.
  Mutex input_mutex_;
  std::unique_ptr<InputEventsGenerator> input_events_generator_;

  bool last_is_accessibility_high_contrast_text_enabled_;

  jobject resource_overlay_;

  Mutex overlay_mutex_;
  std::unordered_map<std::string, bool> overlayed_bool_variables_;
  std::unordered_map<std::string, int> overlayed_int_variables_;
  std::unordered_map<std::string, std::string> overlayed_string_variables_;

  // Methods to process pipes attached to the Looper.
  void ProcessAndroidCommand();
  void ProcessKeyboardInject();

  int64_t app_start_timestamp_ = 0;
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_APPLICATION_ANDROID_H_
