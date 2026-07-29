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

/*
  The following realloc error conditions are not tested due to the difficulty of
  reliably triggering them in a portable unit testing environment:

  - ENOMEM: Insufficient memory is available to fulfill the request. Forcing
    the system to run out of memory is not feasible in a unit test.
*/

#include <stdlib.h>

#include <cerrno>
#include <cstring>
#include <limits>
#include <vector>

#include "starboard/configuration_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

TEST(PosixReallocTest, NullPtrIsLikeMalloc) {
  void* mem = realloc(NULL, kSbMemoryPageSize);
  ASSERT_NE(mem, nullptr);
  // Verify we can write to the allocated memory.
  memset(mem, 0xAB, kSbMemoryPageSize);
  free(mem);
}

TEST(PosixReallocTest, GrowsAllocationAndPreservesData) {
  const int kInitialSize = kSbMemoryPageSize * 2;
  const int kNewSize = kSbMemoryPageSize * 3;
  char* mem = static_cast<char*>(malloc(kInitialSize));
  ASSERT_NE(mem, nullptr);

  // Fill the initial block with a pattern.
  for (int i = 0; i < kInitialSize; ++i) {
    mem[i] = static_cast<char>(i);
  }

  char* new_mem = static_cast<char*>(realloc(mem, kNewSize));
  ASSERT_NE(new_mem, nullptr);

  // Check that the original data is preserved.
  for (int i = 0; i < kInitialSize; ++i) {
    EXPECT_EQ(new_mem[i], static_cast<char>(i));
  }

  // Verify we can write to the newly allocated part.
  memset(new_mem + kInitialSize, 0xCD, kNewSize - kInitialSize);

  free(new_mem);
}

TEST(PosixReallocTest, ShrinksAllocationAndPreservesData) {
  const int kInitialSize = kSbMemoryPageSize * 3;
  const int kNewSize = kSbMemoryPageSize * 2;
  char* mem = static_cast<char*>(malloc(kInitialSize));
  ASSERT_NE(mem, nullptr);

  // Fill the initial block with a pattern.
  for (int i = 0; i < kInitialSize; ++i) {
    mem[i] = static_cast<char>(i);
  }

  char* new_mem = static_cast<char*>(realloc(mem, kNewSize));
  ASSERT_NE(new_mem, nullptr);

  // Check that the data is preserved up to the new, smaller size.
  for (int i = 0; i < kNewSize; ++i) {
    EXPECT_EQ(new_mem[i], static_cast<char>(i));
  }

  free(new_mem);
}

/* POSIX allows two behaviors for realloc with size 0:
  1. Return a null pointer and free the original memory.
  2. Return a valid pointer that can be passed to free().
  This test safely handles both possibilities. */
TEST(PosixReallocTest, HandlesZeroSize) {
  void* mem = malloc(kSbMemoryPageSize);
  ASSERT_NE(mem, nullptr);

  void* new_mem = realloc(mem, 0);
  if (new_mem != nullptr) {
    // If a pointer is returned, it must be valid to free.
    free(new_mem);
  }
  // If new_mem is nullptr, the original memory 'mem' was freed by realloc.
}

TEST(PosixReallocTest, GrowsAcrossPageBoundary) {
  const int kInitialSize = kSbMemoryPageSize - 16;
  const int kNewSize = kSbMemoryPageSize + 16;
  char* mem = static_cast<char*>(malloc(kInitialSize));
  ASSERT_NE(mem, nullptr);

  // Fill the initial block with data.
  for (int i = 0; i < kInitialSize; ++i) {
    mem[i] = static_cast<char>(i);
  }

  char* new_mem = static_cast<char*>(realloc(mem, kNewSize));
  ASSERT_NE(new_mem, nullptr);

  // Check that the original data is preserved.
  for (int i = 0; i < kInitialSize; ++i) {
    EXPECT_EQ(new_mem[i], static_cast<char>(i));
  }

  // Verify we can write to the newly allocated part.
  memset(new_mem + kInitialSize, 0, kNewSize - kInitialSize);

  free(new_mem);
}

}  // namespace
}  // namespace nplb
