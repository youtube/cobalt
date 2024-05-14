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

// Here we are not trying to do anything fancy, just to really sanity check that
// this is hooked up to something.

#include <pthread.h>

#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

void* ThreadFunc(void* context) {
  pthread_setname_np(pthread_self(), "HideSplashTest");
  SbSystemHideSplashScreen();
  return NULL;
}

TEST(SbSystemHideSplashScreenTest, SunnyDay) {
  // Function returns no result, and correct execution cannot be determined
  // programmatically, but we should at least be able to call it twice without
  // crashing.
  SbSystemHideSplashScreen();
  SbSystemHideSplashScreen();
}

TEST(SbSystemHideSplashScreenTest, SunnyDayNewThread) {
  // We should be able to call the function from any thread.
  pthread_t thread = 0;
  pthread_create(&thread, nullptr, ThreadFunc, nullptr);
  EXPECT_TRUE(thread != 0);
  pthread_join(thread, NULL /*out_return*/);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
