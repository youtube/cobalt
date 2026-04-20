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

#include <stdlib.h>

#include <cerrno>
#include <cstdint>

#include "testing/gtest/include/gtest/gtest.h"

/*
 * Untestable Scenarios for aligned_alloc:
 * - ENOMEM: Reliably triggering out-of-memory conditions in a portable
 * and non-disruptive way within a unit test is generally not feasible.
 * - Cases where alignment is not a power of 2, as we check for it in
 * allocator_shim_default_dispatch_to_partition_alloc.cc and have a compiler
 * flag that verify this requirement.
 */

namespace nplb {
namespace {

// Checks if the given memory address is aligned to the specified alignment.
bool IsMemoryAligned(const void* ptr, size_t alignment) {
  if (alignment == 0) {
    return false;
  }
  return reinterpret_cast<uintptr_t>(ptr) % alignment == 0;
}

TEST(PosixAlignedAllocTests,
     AllocatesMemorySuccessfullyWhenAlignmentAndSizeAreValid) {
  // Valid alignment is a power of 2 based on
  // https://man7.org/linux/man-pages/man3/aligned_alloc.3.html#DESCRIPTION
  const size_t kValidAlignment = 64;
  const size_t kValidSize = kValidAlignment * 2;
  ASSERT_TRUE((kValidAlignment > 0) &&
              ((kValidAlignment & (kValidAlignment - 1)) == 0));
  ASSERT_EQ(0u, kValidSize % kValidAlignment);

  errno = 0;
  void* ptr = aligned_alloc(kValidAlignment, kValidSize);

  ASSERT_NE(nullptr, ptr) << "aligned_alloc returned NULL. errno: "
                          << strerror(errno);
  EXPECT_EQ(0, errno);
  EXPECT_TRUE(IsMemoryAligned(ptr, kValidAlignment));

  free(ptr);
}

TEST(PosixAlignedAllocTests, HandlesZeroSizeCorrectly) {
  const size_t kValidAlignment = 128;
  const size_t kZeroSize = kValidAlignment * 0;
  ASSERT_TRUE((kValidAlignment > 0) &&
              ((kValidAlignment & (kValidAlignment - 1)) == 0));

  errno = 0;
  void* ptr = aligned_alloc(kValidAlignment, kZeroSize);

  // For size 0, aligned_alloc may return NULL or a unique pointer.
  // It should not set an error.
  EXPECT_EQ(errno, 0) << "errno was " << strerror(errno);

  if (ptr != nullptr) {
    EXPECT_TRUE(IsMemoryAligned(ptr, kValidAlignment));
  }
  free(ptr);
}

TEST(PosixAlignedAllocTests,
     AllocatesSuccessfullyWithMinimumValidAlignmentOne) {
  const size_t kAlignmentOne = 1;  // 2^0 = 1
  const size_t kAnySize = kAlignmentOne * 100;

  errno = 0;
  void* ptr = aligned_alloc(kAlignmentOne, kAnySize);

  ASSERT_NE(nullptr, ptr) << "aligned_alloc returned NULL. errno: "
                          << strerror(errno);
  EXPECT_EQ(0, errno);
  EXPECT_TRUE(IsMemoryAligned(ptr, kAlignmentOne));

  free(ptr);
}

TEST(PosixAlignedAllocTests,
     AllocatesSuccessfullyWithAlignmentEqualToSizeofVoidPointer) {
  const size_t kAlignmentSizeofVoidPtr = sizeof(void*);
  const size_t kTestSize = kAlignmentSizeofVoidPtr * 4;

  errno = 0;
  void* ptr = aligned_alloc(kAlignmentSizeofVoidPtr, kTestSize);

  ASSERT_NE(nullptr, ptr) << "aligned_alloc returned NULL. errno: "
                          << strerror(errno);
  EXPECT_EQ(0, errno);
  EXPECT_TRUE(IsMemoryAligned(ptr, kAlignmentSizeofVoidPtr));

  free(ptr);
}

TEST(PosixAlignedAllocTests, AllocatesSuccessfullyWithLargeAlignmentAndSize) {
  const size_t kLargeAlignment = 1024 * 1024;
  const size_t kLargeSize = kLargeAlignment * 2;
  ASSERT_TRUE((kLargeAlignment > 0) &&
              ((kLargeAlignment & (kLargeAlignment - 1)) == 0));
  ASSERT_EQ(0u, kLargeSize % kLargeAlignment);

  errno = 0;
  void* ptr = aligned_alloc(kLargeAlignment, kLargeSize);

  if (ptr == nullptr) {
    // ENOMEM is a possible outcome for large allocations, unlikely to ever hit
    // this given size constraints in Cobalt.
    EXPECT_EQ(ENOMEM, errno)
        << "Large allocation failed. ENOMEM expected, actual: "
        << strerror(errno);
  } else {
    EXPECT_EQ(0, errno);
    EXPECT_TRUE(IsMemoryAligned(ptr, kLargeAlignment));
    free(ptr);
  }
}

}  // namespace
}  // namespace nplb
