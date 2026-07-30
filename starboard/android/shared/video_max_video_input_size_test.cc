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

#include "starboard/android/shared/video_max_video_input_size.h"

#include <thread>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

TEST(VideoMaxVideoInputSizeTest, DefaultValueIsZero) {
  EXPECT_EQ(GetMaxVideoInputSizeForCurrentThread(), 0);
}

TEST(VideoMaxVideoInputSizeTest, SetAndGetValueOnCurrentThread) {
  SetMaxVideoInputSizeForCurrentThread(1024 * 1024);
  EXPECT_EQ(GetMaxVideoInputSizeForCurrentThread(), 1024 * 1024);

  // Reset back to 0
  SetMaxVideoInputSizeForCurrentThread(0);
  EXPECT_EQ(GetMaxVideoInputSizeForCurrentThread(), 0);
}

TEST(VideoMaxVideoInputSizeTest, ThreadIsolation) {
  SetMaxVideoInputSizeForCurrentThread(2048 * 2048);

  std::thread other_thread([]() {
    // Should be 0 on other thread
    EXPECT_EQ(GetMaxVideoInputSizeForCurrentThread(), 0);
    SetMaxVideoInputSizeForCurrentThread(512 * 512);
    EXPECT_EQ(GetMaxVideoInputSizeForCurrentThread(), 512 * 512);
  });
  other_thread.join();

  // Current thread value should remain unchanged
  EXPECT_EQ(GetMaxVideoInputSizeForCurrentThread(), 2048 * 2048);

  // Reset
  SetMaxVideoInputSizeForCurrentThread(0);
}

}  // namespace
}  // namespace starboard
