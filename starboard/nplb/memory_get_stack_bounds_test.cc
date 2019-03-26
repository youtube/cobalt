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

#include "starboard/memory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbMemoryGetStackBoundsTest, SanityCheck) {
  const void* const kNull = NULL;
  void* high = NULL;
  void* low = NULL;
  void* stack_object = reinterpret_cast<void*>(&high);

  SbMemoryGetStackBounds(&high, &low);
  EXPECT_NE(kNull, high);
  EXPECT_NE(kNull, low);
  EXPECT_NE(low, high);
  EXPECT_LT(low, high);
  EXPECT_LE(low, stack_object);
  EXPECT_GT(high, stack_object);
  intptr_t h = reinterpret_cast<intptr_t>(high);
  intptr_t l = reinterpret_cast<intptr_t>(low);
  EXPECT_LT(4096, h - l);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
