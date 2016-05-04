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

#include "starboard/window.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbWindowGetSizeTest, SunnyDay) {
  SbWindow window = SbWindowCreate(NULL);
  ASSERT_TRUE(SbWindowIsValid(window));

  SbWindowSize size;
  size.width = 0;
  size.height = 0;

  ASSERT_TRUE(SbWindowGetSize(window, &size));
  EXPECT_LT(0, size.width);
  EXPECT_LT(0, size.height);

  ASSERT_TRUE(SbWindowDestroy(window));
}

TEST(SbWindowGetSizeTest, RainyDayInvalid) {
  SbWindowSize size;
  ASSERT_FALSE(SbWindowGetSize(kSbWindowInvalid, &size));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
