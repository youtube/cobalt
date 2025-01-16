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

#include "ui/ozone/platform/starboard/test/starboard_test_helper.h"

#include "starboard/system.h"

namespace ui {
namespace {
// SbEventHandle needs to call into the test class' event handler to signal the
// |started_condition_|, so the test object is tracked here.
OzoneStarboardTest* ozone_starboard_test_instance = nullptr;

// Static callback for SbEvents. This will pass events to the test class'
// implementation of |EventHandleInternal| to signal the main thread has
// started.
void SbEventHandle(const SbEvent* event) {
  if (ozone_starboard_test_instance) {
    ozone_starboard_test_instance->EventHandleInternal(event);
  }
}
}  // namespace

OzoneStarboardTest::OzoneStarboardTest() {
  ozone_starboard_test_instance = this;

  started_condition_ = std::make_unique<ConditionVariable>(started_mutex_);

  // Start the main starboard thread to allow Starboard function calls.
  sb_main_ = std::make_unique<OzoneStarboardThread>();
  started_mutex_.Acquire();
  sb_main_->Start();
  // Wait for the |kSbEventTypeStart| to signal the initialization completion
  // before continuing.
  started_condition_->Wait();
  started_mutex_.Release();
}

OzoneStarboardTest::~OzoneStarboardTest() {
  // Kill and clean up the Starboard main thread.
  SbSystemRequestStop(0);
  sb_main_->Join();
  sb_main_.reset();

  started_condition_.reset();
  ozone_starboard_test_instance = nullptr;
}

// Note: If overriding this function, be sure to signal the started_condition_
// or the test will hang.
void OzoneStarboardTest::EventHandleInternal(const SbEvent* event) {
  switch (event->type) {
    case kSbEventTypeStart:
      started_condition_->Signal();
      break;
    default:
      break;
  }
}

void OzoneStarboardTest::OzoneStarboardThread::Run() {
  SbRunStarboardMain(0, nullptr, &SbEventHandle);
}
}  // namespace ui
