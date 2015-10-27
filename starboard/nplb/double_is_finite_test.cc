// Copyright 2015 Google Inc. All Rights Reserved.
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

#include <limits>

#include "starboard/double.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(SbDoubleIsFiniteTest, SunnyDay) {
  double d = 1;
  EXPECT_TRUE((&d));

  d = std::numeric_limits<double>::infinity();
  EXPECT_FALSE(SbDoubleIsFinite(&d));
}

}  // namespace
