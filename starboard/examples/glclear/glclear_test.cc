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

#include <array>
#include "testing/gtest/include/gtest/gtest.h"

template <typename T, size_t N>
class Stats {
 public:
  void addSample(T t) {
    samples[head++] = t;
    head = head % N;
    wrapped = wrapped || (head == 0);
  }

 public:
  bool wrapped = false;
  size_t head = 0;
  std::array<T, N> samples;
};

using IntStats = Stats<int, 4>;

namespace starboard {
namespace {

TEST(GlClear, ISOK) {
  EXPECT_EQ(1, 1);
  IntStats st;
  st.addSample(1);
  st.addSample(2);
  EXPECT_FALSE(st.wrapped);
  st.addSample(3);
  EXPECT_FALSE(st.wrapped);
  st.addSample(4);
  EXPECT_TRUE(st.wrapped);
}

}  // namespace
}  // namespace starboard
