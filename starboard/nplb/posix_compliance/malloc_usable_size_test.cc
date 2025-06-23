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

#include <malloc.h>
#include <stdlib.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

const size_t kSize = 1024 * 128;

TEST(MallocUsableSizeTest, HappyPath) {
  void* memory = malloc(kSize);
  EXPECT_NE(nullptr, memory);
  EXPECT_GE(malloc_usable_size(memory), kSize);
  free(memory);
}

TEST(MallocUsableSizeTest, NullPtrSize) {
  // If ptr is NULL, 0 should be returned.
  EXPECT_EQ(0u, malloc_usable_size(nullptr));
}

TEST(MallocUsableSizeTest, HappyPathAfterAllocatingZero) {
  void* memory = malloc(0);
  EXPECT_GE(malloc_usable_size(memory), 0u);
  free(memory);
}

// Tests that once the memory is written to, the behaviour is the same.
TEST(MallocUsableSizeTest, CastAndWriteDataSize) {
  void* memory = malloc(kSize);
  ASSERT_NE(static_cast<void*>(NULL), memory);
  char* data = static_cast<char*>(memory);
  for (size_t i = 0; i < kSize; ++i) {
    data[i] = static_cast<char>(i);
  }

  EXPECT_GE(malloc_usable_size(data), kSize);
  EXPECT_GE(malloc_usable_size(memory), kSize);

  free(memory);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
