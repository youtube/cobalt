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

#include "starboard/stub/application_stub.h"

#include "starboard/common/log.h"
#include "starboard/event.h"

namespace starboard {
namespace stub {

#if SB_API_VERSION >= 15
ApplicationStub::ApplicationStub(SbEventHandleCallback sb_event_handle_callback)
    : QueueApplication(sb_event_handle_callback) {}
#else
ApplicationStub::ApplicationStub() {}
#endif  // SB_API_VERSION >= 15

ApplicationStub::~ApplicationStub() {}

void ApplicationStub::Initialize() {}

void ApplicationStub::Teardown() {}

bool ApplicationStub::MayHaveSystemEvents() {
  return false;
}

shared::starboard::Application::Event* ApplicationStub::PollNextSystemEvent() {
  return NULL;
}

shared::starboard::Application::Event*
ApplicationStub::WaitForSystemEventWithTimeout(SbTime time) {
  return NULL;
}

void ApplicationStub::WakeSystemEventWait() {}

}  // namespace stub
}  // namespace starboard
