// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include <string>
#include <vector>

#include "starboard/common/string.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

TEST(StringTest, SplitString) {
  std::string str;
  std::vector<std::string> output = SplitString(str, '.');
  EXPECT_TRUE(output.empty());

  str = "The quick brown fox jumps over the lazy dog";
  output = SplitString(str, '.');
  EXPECT_TRUE(output.empty());

  std::vector<std::string> vec = {"The",  "quick", "brown", "fox", "jumps",
                                  "over", "the",   "lazy",  "dog"};
  output = SplitString(str, ' ');
  ASSERT_EQ(output.size(), vec.size());
  for (int i = 0; i < vec.size(); ++i) {
    ASSERT_EQ(output[i], vec[i]);
  }
}

}  // namespace
}  // namespace starboard
