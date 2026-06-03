// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "media/starboard/starboard_memory_allocator.h"

#include <string.h>
#include <unistd.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

class StarboardMemoryAllocatorTest : public ::testing::TestWithParam<bool> {
 protected:
  StarboardMemoryAllocatorTest() : allocator_(GetParam()) {}
  StarboardMemoryAllocator allocator_;
};

TEST_P(StarboardMemoryAllocatorTest, AllocateAndFree) {
  void* p = allocator_.Allocate(1024);
  EXPECT_NE(p, nullptr);
  // Write to the memory to ensure it's valid.
  memset(p, 0xAB, 1024);
  allocator_.Free(p);
}

TEST_P(StarboardMemoryAllocatorTest, AllocateZeroSize) {
  void* p = allocator_.Allocate(0);
  // Allocate(0) can return a nullptr or a unique non-null pointer.
  // In either case, it should be possible to free it.
  if (p) {
    allocator_.Free(p);
  }
}

TEST_P(StarboardMemoryAllocatorTest, AllocateWithAlignment) {
  const size_t alignment = 256;
  void* p = allocator_.Allocate(1024, alignment);
  EXPECT_NE(p, nullptr);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % alignment, 0U);
  allocator_.Free(p);
}

TEST_P(StarboardMemoryAllocatorTest, AllocateWithZeroAlignment) {
  const size_t alignment = 0;
  void* p = allocator_.Allocate(1024, alignment);
  EXPECT_NE(p, nullptr);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(p) % sizeof(void*), 0U);
  allocator_.Free(p);
}

TEST_P(StarboardMemoryAllocatorTest, GetCapacityAndAllocatedShouldBeZero) {
  EXPECT_EQ(allocator_.GetCapacity(), 0U);
  EXPECT_EQ(allocator_.GetAllocated(), 0U);
  void* p = allocator_.Allocate(1024);
  EXPECT_NE(p, nullptr);
  // Should still be zero as this allocator doesn't track allocations.
  EXPECT_EQ(allocator_.GetCapacity(), 0U);
  EXPECT_EQ(allocator_.GetAllocated(), 0U);
  allocator_.Free(p);
}

INSTANTIATE_TEST_SUITE_P(EnableDecommit,
                         StarboardMemoryAllocatorTest,
                         ::testing::Bool());

TEST(StarboardMemoryAllocatorDecommitSpecificTest,
     AllocateForAlignmentUpdatesSizeWhenDecommitEnabled) {
  StarboardMemoryAllocator allocator(/*enable_decommit=*/true);

  const size_t page_size = sysconf(_SC_PAGESIZE);
  const size_t initial_size = page_size / 2;  // A size smaller than page size

  size_t size = initial_size;
  size_t alignment = 1;
  void* ptr = allocator.AllocateForAlignment(&size, alignment);

  EXPECT_NE(ptr, nullptr);
  EXPECT_EQ(size, page_size);  // Should be aligned up to page size

  allocator.Free(ptr);
}

TEST(StarboardMemoryAllocatorDecommitSpecificTest,
     AllocateForAlignmentDoesNotUpdateSizeWhenDecommitDisabled) {
  StarboardMemoryAllocator allocator(/*enable_decommit=*/false);

  const size_t page_size = sysconf(_SC_PAGESIZE);
  const size_t initial_size = page_size / 2;

  size_t size = initial_size;
  size_t alignment = 1;
  void* ptr = allocator.AllocateForAlignment(&size, alignment);

  EXPECT_NE(ptr, nullptr);
  EXPECT_EQ(size, initial_size);  // Should remain unchanged

  allocator.Free(ptr);
}

}  // namespace
}  // namespace media
