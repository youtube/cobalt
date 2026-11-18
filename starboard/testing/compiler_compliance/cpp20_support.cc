// Copyright 2024 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the aLicense is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstdint>
#include <iostream>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

// These examples are taken after referring to
// 1) C++20 allowlist from chromium m114 milestone branch :
// https://chromium.googlesource.com/chromium/src/+/refs/branch-heads/5735/styleguide/c++/c++-features.md
// 2) cpp reference : https://en.cppreference.com/w/cpp

// Test std::string ends_with support
TEST(Cpp20Support, StringEndsWith) {
  EXPECT_TRUE(std::string("foobar").ends_with("bar"));
}

// Test std::erase_if support
TEST(Cpp20Support, EraseIf) {
  std::vector<char> cnt(10);
  std::iota(cnt.begin(), cnt.end(), '0');
  EXPECT_EQ(1U, std::erase(cnt, '3'));
  EXPECT_EQ(5U, std::erase_if(cnt, [](char x) { return (x - '0') % 2 == 0; }));
  ASSERT_EQ(4U, cnt.size());
  EXPECT_EQ(std::vector<char>({'1', '5', '7', '9'}), cnt);
}

// Test std::midpoint support
TEST(Cpp20Support, MidPoint) {
  std::uint32_t a = std::numeric_limits<std::uint32_t>::max();
  std::uint32_t b = std::numeric_limits<std::uint32_t>::max() - 2;
  EXPECT_EQ(std::midpoint(a, b), std::numeric_limits<std::uint32_t>::max() - 1);
}

// Test designated initializers
TEST(Cpp20Support, DesignatedInitializer) {
  struct S {
    int x = 1;
    int y = 2;
  };
  S s{.y = 3};  // OK, s.x == 1, s.y == 3
  EXPECT_EQ(s.x, 1);
  EXPECT_EQ(s.y, 3);
}

}  // namespace
}  // namespace starboard
