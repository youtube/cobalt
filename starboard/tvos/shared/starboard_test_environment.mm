// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/tvos/shared/starboard_test_environment.h"

#include "starboard/shared/starboard/queue_application.h"
#include "starboard/common/log.h" // For SB_UNREFERENCED_PARAMETER
#include "starboard/event.h" // For SbEventHandleCallback

namespace {
// A dummy event handler for the StubApplication.
void DummyEventHandleCallback(const SbEvent* event) {
  SB_UNREFERENCED_PARAMETER(event);
}
} // namespace

namespace starboard {

// A minimal concrete implementation of QueueApplication for testing.
// This is used to allow nplb tests to build without pulling in a full
// application implementation.
class StubApplication : public QueueApplication {
 public:
  StubApplication() : QueueApplication(DummyEventHandleCallback) {}
  ~StubApplication() override = default;

 private:
  bool MayHaveSystemEvents() override { return false; }
  Event* WaitForSystemEventWithTimeout(int64_t time) override {
    SB_UNREFERENCED_PARAMETER(time);
    return nullptr;
  }
  void WakeSystemEventWait() override {}
};

StarboardTestEnvironment::StarboardTestEnvironment(int argc, char** argv)
    : command_line_(argc, argv) {}

StarboardTestEnvironment::~StarboardTestEnvironment() = default;

void StarboardTestEnvironment::SetUp() {
  application_ = std::make_unique<StubApplication>();
}

void StarboardTestEnvironment::TearDown() {
  application_.reset();
}

}  // namespace starboard
