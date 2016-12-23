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

// This file isn't actually used on Android, but it's needed as a test driver.

#include <time.h>

#include "starboard/android/shared/application_android.h"
#include "starboard/configuration.h"
#include "starboard/shared/signal/crash_signals.h"
#include "starboard/shared/signal/suspend_signals.h"

namespace starboard {
namespace android {
namespace shared {

// TODO: We may need more than this short-circuit of ApplicationAndroid
class ApplicationAndroidStub : public ApplicationAndroid {
 public:
  ApplicationAndroidStub() : ApplicationAndroid(&app_) {
    memset(&app_, 0, sizeof(app_));
  }

 protected:
  Event* WaitForSystemEventWithTimeout(SbTime time) SB_OVERRIDE {
    if (state() == kStateUnstarted) {
      DispatchStart();
    }
    return NULL;
  }

  void WakeSystemEventWait() SB_OVERRIDE { }

  Event* GetNextEvent() SB_OVERRIDE {
    Event* event = ApplicationAndroid::GetNextEvent();
    if (event && event->event->type == kSbEventTypeStop) {
      app_.destroyRequested = true;
    }
    return event;
  }

 private:
  struct android_app app_;
};

extern "C" SB_EXPORT_PLATFORM int main(int argc, char** argv) {
  tzset();
  starboard::shared::signal::InstallCrashSignalHandlers();
  starboard::shared::signal::InstallSuspendSignalHandlers();
  starboard::android::shared::ApplicationAndroidStub application;
  int result = application.Run(argc, argv);
  starboard::shared::signal::UninstallSuspendSignalHandlers();
  starboard::shared::signal::UninstallCrashSignalHandlers();
  return result;
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
