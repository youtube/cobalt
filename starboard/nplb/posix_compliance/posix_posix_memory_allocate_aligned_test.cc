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

/*
  The following error conditions are not tested due to the difficulty of
  reliably triggering them in a portable unit testing environment:
  - ENOMEM: There is insufficient memory to satisfy the request.
*/

#include <stdlib.h>

#include "starboard/common/memory.h"
#include "starboard/configuration.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard::nplb {
namespace {

// Choose a size that isn't naturally aligned to anything.
const size_t kSize = 1024 * 128 + 1;

TEST(PosixMemalignTest, AllocatesAligned) {
  const size_t kMaxAlign = 4096 + 1;
  for (size_t align = sizeof(void*); align < kMaxAlign; align <<= 1) {
    void* memory = NULL;
    std::ignore = posix_memalign(&memory, align, kSize);
    ASSERT_NE(static_cast<void*>(NULL), memory);
    EXPECT_TRUE(starboard::common::MemoryIsAligned(memory, align));
    free(memory);
  }
}

TEST(PosixMemalignTest, CanReadWriteToResult) {
  const size_t kAlign = 4096;
  void* memory = NULL;
  std::ignore = posix_memalign(&memory, kAlign, kSize);
  ASSERT_NE(static_cast<void*>(NULL), memory);
  char* data = static_cast<char*>(memory);
  for (size_t i = 0; i < kSize; ++i) {
    data[i] = static_cast<char>(i);
  }

  for (size_t i = 0; i < kSize; ++i) {
    EXPECT_EQ(data[i], static_cast<char>(i));
  }

  free(memory);
}

TEST(PosixMemalignTest, SuccessfulAllocation) {
  const size_t kAlignment = 512;
  const size_t kSizeMultipleofAlignment = 1024;
  void* memptr = NULL;

  int result = posix_memalign(&memptr, kAlignment, kSizeMultipleofAlignment);
  ASSERT_EQ(result, 0);
  ASSERT_NE(memptr, nullptr);

  // Verify the alignment.
  EXPECT_EQ(reinterpret_cast<uintptr_t>(memptr) % kAlignment, 0u);

  // Verify we can write to and read from the memory.
  memset(memptr, 0xAB, kSizeMultipleofAlignment);
  EXPECT_EQ(static_cast<unsigned char*>(memptr)[0], 0xAB);
  EXPECT_EQ(static_cast<unsigned char*>(memptr)[kSizeMultipleofAlignment - 1],
            0xAB);

  free(memptr);
}

TEST(PosixMemalignTest, ZeroSizeAllocation) {
  void* memptr = NULL;
  const size_t kAlignment = sizeof(void*) * 2;

  // The behavior for size = 0 is implementation-defined.
  // It can return 0 with a NULL or valid pointer, or return an error.
  // We just check that it doesn't crash. If it succeeds with a non-null
  // pointer, that pointer must be freeable.
  int result = posix_memalign(&memptr, kAlignment, 0);

  if (result == 0 && memptr != NULL) {
    free(memptr);
  }
}

TEST(PosixMemalignTest, FailsWithInvalidAlignmentNotPowerOfTwo) {
  void* memptr = NULL;
  // Alignment must be a power of two. 3 * sizeof(void*) is not.
  const size_t kInvalidAlignment = sizeof(void*) * 3;
  int result = posix_memalign(&memptr, kInvalidAlignment, 1024);

  EXPECT_EQ(result, EINVAL);
  // memptr should be unmodified or NULL on failure.
  EXPECT_EQ(memptr, nullptr);
}

TEST(PosixMemalignTest, FailsWithInvalidAlignmentNotMultipleOfVoidPointer) {
  void* memptr = NULL;
  // Alignment must be a multiple of sizeof(void*). 1 may not be.
  if (sizeof(void*) > 1) {
    int result = posix_memalign(&memptr, 1, 1024);
    EXPECT_EQ(result, EINVAL);
    EXPECT_EQ(memptr, nullptr);
  }
}

TEST(PosixMemalignTest, AlignsForVariousDataTypes) {
  void* memptr = NULL;

  // Test alignment for a long double.
  int result =
      posix_memalign(&memptr, alignof(long double), sizeof(long double));
  ASSERT_EQ(result, 0);
  ASSERT_NE(memptr, nullptr);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(memptr) % alignof(long double), 0u);
  free(memptr);

  // Test alignment for a long long.
  memptr = NULL;
  result = posix_memalign(&memptr, alignof(long long), sizeof(long long));
  ASSERT_EQ(result, 0);
  ASSERT_NE(memptr, nullptr);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(memptr) % alignof(long long), 0u);
  free(memptr);
}

}  // namespace
}  // namespace starboard::nplb
