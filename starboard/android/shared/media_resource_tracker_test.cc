// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/media_resource_tracker.h"

#include <chrono>
#include <thread>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

TEST(MediaResourceTrackerTest, InitialCountIsZero) {
  auto* tracker = MediaResourceTracker::GetInstance();
  EXPECT_EQ(0, tracker->WaitUntilZero(0));
}

TEST(MediaResourceTrackerTest, IncrementAndDecrement) {
  auto* tracker = MediaResourceTracker::GetInstance();
  tracker->Increment();
  tracker->Increment();

  std::thread worker([tracker]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    tracker->Decrement();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    tracker->Decrement();
  });

  EXPECT_EQ(0, tracker->WaitUntilZero(1000));
  worker.join();
}

TEST(MediaResourceTrackerTest, TimeoutWhenNotZero) {
  auto* tracker = MediaResourceTracker::GetInstance();
  tracker->Increment();

  // Should timeout and return remaining count of 1
  EXPECT_EQ(1, tracker->WaitUntilZero(50));

  // Clean up counter state
  tracker->Decrement();
  EXPECT_EQ(0, tracker->WaitUntilZero(0));
}

}  // namespace
}  // namespace starboard
