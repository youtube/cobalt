// Copyright (c) 2016, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef STARBOARD_ANDROID_APPLICATION_ANDROID_H_
#define STARBOARD_ANDROID_APPLICATION_ANDROID_H_

#include "starboard/configuration.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/application.h"
#include "starboard/shared/starboard/queue_application.h"
#include "starboard/types.h"
#include "starboard/window.h"
#include "third_party/starboard/android/shared/window_internal.h"

#include "starboard/key.h"

#include <android/log.h>
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "native-activity", __VA_ARGS__))

namespace starboard {
namespace android {
namespace shared {

// Android application engine using the generic queue and a android implementation.
class ApplicationAndroid : public ::starboard::shared::starboard::QueueApplication {
 public:
  ApplicationAndroid() {
  };
  ~ApplicationAndroid() SB_OVERRIDE {
  };

  static ApplicationAndroid* Get() {
    return static_cast<ApplicationAndroid*>(::starboard::shared::starboard::Application::Get());
  }

  SbWindow CreateWindow(const SbWindowOptions* options);
  bool DestroyWindow(SbWindow window);

  /* getters/setters */
  SbWindow GetWindow();
  void SetWindowHandle(ANativeWindow *handle);
  ANativeWindow* GetWindowHandle();
  void SetVideoWindowHandle(ANativeWindow *handle);
  ANativeWindow* GetVideoWindowHandle();
  void SetExternalFilesDir(const char *dir);
  const char* GetExternalFilesDir();
  void SetExternalCacheDir(const char *dir);
  const char* GetExternalCacheDir();
  void SetMessageFDs(int readFD, int writeFD);
  int GetMessageReadFD();
  int GetMessageWriteFD();

 protected:
  // --- Application overrides ---
  void Initialize() SB_OVERRIDE;
  void Teardown() SB_OVERRIDE;

  // --- QueueApplication overrides ---
  bool MayHaveSystemEvents() SB_OVERRIDE;
  Event* PollNextSystemEvent() SB_OVERRIDE;
  Event* WaitForSystemEventWithTimeout(SbTime time) SB_OVERRIDE;
  void WakeSystemEventWait() SB_OVERRIDE;

 private:
  ::starboard::shared::starboard::Application::Event* CreateEventFromAndroidEvent(AInputEvent*);
  SbKey KeyCodeToSbKey(int16_t code);
  SbKeyLocation KeyCodeToSbKeyLocation(int16_t code);
  unsigned int MetaStateToModifiers(int32_t state);

  SbWindow window_;
  ANativeWindow *hwindow_;
  ANativeWindow *vwindow_;

  const char* external_files_dir_;
  const char* external_cache_dir_;
  int msg_read_fd;
  int msg_write_fd;
};

}  // namespace android
}  // namespace starboard
}  // namespace shared

#endif  // STARBOARD_ANDROID_APPLICATION_STUB_H_
