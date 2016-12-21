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

#include "starboard/android/shared/application_android.h"

#include <time.h>

#include "starboard/event.h"
#include "starboard/log.h"

namespace starboard {
namespace android {

ApplicationAndroid::ApplicationAndroid() {
  SB_NOTIMPLEMENTED();
}

ApplicationAndroid::~ApplicationAndroid() {
  SB_NOTIMPLEMENTED();
}

void ApplicationAndroid::Initialize() {
  // Called once here to help SbTimeZoneGet*Name()
  tzset();
}

void ApplicationAndroid::Teardown() {
  SB_NOTIMPLEMENTED();
}

bool ApplicationAndroid::MayHaveSystemEvents() {
  SB_NOTIMPLEMENTED();
  return false;
}

shared::starboard::Application::Event*
    ApplicationAndroid::PollNextSystemEvent() {
  SB_NOTIMPLEMENTED();
  return NULL;
}

shared::starboard::Application::Event*
ApplicationAndroid::WaitForSystemEventWithTimeout(SbTime time) {
  SB_NOTIMPLEMENTED();
  return NULL;
}

void ApplicationAndroid::WakeSystemEventWait() {
  SB_NOTIMPLEMENTED();
}

}  // namespace android
}  // namespace starboard
