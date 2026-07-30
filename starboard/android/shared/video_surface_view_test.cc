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

#include "starboard/android/shared/video_surface_view.h"

#include <thread>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

TEST(VideoSurfaceViewTest, DefaultValueIsNullptr) {
  EXPECT_EQ(GetSurfaceViewForCurrentThread(), nullptr);
}

TEST(VideoSurfaceViewTest, SetAndGetValueOnCurrentThread) {
  int dummy_surface = 42;
  void* fake_surface = &dummy_surface;

  SetVideoSurfaceViewForCurrentThread(fake_surface);
  EXPECT_EQ(GetSurfaceViewForCurrentThread(), fake_surface);

  // Reset back to nullptr
  SetVideoSurfaceViewForCurrentThread(nullptr);
  EXPECT_EQ(GetSurfaceViewForCurrentThread(), nullptr);
}

TEST(VideoSurfaceViewTest, ThreadIsolation) {
  int dummy_surface = 123;
  void* fake_surface = &dummy_surface;

  SetVideoSurfaceViewForCurrentThread(fake_surface);

  std::thread other_thread([]() {
    // Should be nullptr on other thread
    EXPECT_EQ(GetSurfaceViewForCurrentThread(), nullptr);

    int dummy_other = 456;
    SetVideoSurfaceViewForCurrentThread(&dummy_other);
    EXPECT_EQ(GetSurfaceViewForCurrentThread(), &dummy_other);
  });
  other_thread.join();

  // Current thread value should remain unchanged
  EXPECT_EQ(GetSurfaceViewForCurrentThread(), fake_surface);

  // Reset
  SetVideoSurfaceViewForCurrentThread(nullptr);
}

}  // namespace
}  // namespace starboard
