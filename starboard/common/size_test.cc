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

#include "starboard/common/size.h"

#include <sstream>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

TEST(SizeTest, DefaultConstructor) {
  constexpr Size size;

  EXPECT_EQ(0, size.width);
  EXPECT_EQ(0, size.height);
}

TEST(SizeTest, Constructor) {
  constexpr Size size(1920, 1080);

  EXPECT_EQ(1920, size.width);
  EXPECT_EQ(1080, size.height);
}

TEST(SizeTest, StreamInsertion) {
  constexpr Size size(1280, 720);
  std::stringstream ss;

  ss << size;

  EXPECT_EQ("1280x720", ss.str());
}

}  // namespace
}  // namespace starboard
