// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/window.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbWindowCreateTest, SunnyDayDefault) {
  SbWindow window = SbWindowCreate(NULL);
  ASSERT_TRUE(SbWindowIsValid(window));
  ASSERT_TRUE(SbWindowDestroy(window));
}

TEST(SbWindowCreateTest, SunnyDayDefaultSet) {
  SbWindowOptions options;
  SbWindowSetDefaultOptions(&options);
  SbWindow window = SbWindowCreate(&options);
  ASSERT_TRUE(SbWindowIsValid(window));
  ASSERT_TRUE(SbWindowDestroy(window));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
