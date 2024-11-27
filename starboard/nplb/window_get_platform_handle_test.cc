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

#include "starboard/configuration.h"
#include "starboard/window.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbWindowGetPlatformHandleTest, SunnyDay) {
  SbWindow window = SbWindowCreate(NULL);
  ASSERT_TRUE(SbWindowIsValid(window));
  void* handle = SbWindowGetPlatformHandle(window);
  // 0 could actually be a valid value here, because we don't know what the
  // platform uses as its native window handle type, so we can't do any
  // verification here.
  ASSERT_TRUE(SbWindowDestroy(window));
}

TEST(SbWindowGetPlatformHandleTest, RainyDay) {
  void* handle = SbWindowGetPlatformHandle(kSbWindowInvalid);
  // We don't know what the platform's invalid window handle value is, so we
  // can't do any verification here.
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
