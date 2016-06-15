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

#include "starboard/stub/application_stub.h"

#include "starboard/event.h"
#include "starboard/log.h"

namespace starboard {
namespace stub {

ApplicationStub::ApplicationStub() {
  SB_NOTIMPLEMENTED();
}

ApplicationStub::~ApplicationStub() {
  SB_NOTIMPLEMENTED();
}

void ApplicationStub::Initialize() {
  SB_NOTIMPLEMENTED();
}

void ApplicationStub::Teardown() {
  SB_NOTIMPLEMENTED();
}

bool ApplicationStub::MayHaveSystemEvents() {
  SB_NOTIMPLEMENTED();
  return false;
}

shared::starboard::Application::Event* ApplicationStub::PollNextSystemEvent() {
  SB_NOTIMPLEMENTED();
  return NULL;
}

shared::starboard::Application::Event*
ApplicationStub::WaitForSystemEventWithTimeout(SbTime time) {
  SB_NOTIMPLEMENTED();
  return NULL;
}

void ApplicationStub::WakeSystemEventWait() {
  SB_NOTIMPLEMENTED();
}

}  // namespace stub
}  // namespace starboard
