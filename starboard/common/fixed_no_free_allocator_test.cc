/*
 * Copyright 2023 The Cobalt Authors. All Rights Reserved.
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "starboard/common/fixed_no_free_allocator.h"

#include <memory>

#include "starboard/common/pointer_arithmetic.h"
#include "testing/gtest/include/gtest/gtest.h"

struct AlignedMemoryDeleter {
  void operator()(uint8_t* p) { free(p); }
};

class FixedNoFreeAllocatorTest : public ::testing::Test {
 public:
  FixedNoFreeAllocatorTest();

 protected:
  static const std::size_t kAllocationSize = 8;
  static const std::size_t kAllocationAlignment = 8;
  static const std::size_t kMaxAllocations = 64;
  static const std::size_t kBufferSize = kAllocationSize * kMaxAllocations;

  std::unique_ptr<uint8_t, AlignedMemoryDeleter> buffer_;
  starboard::common::FixedNoFreeAllocator allocator_;

 private:
  void* AllocatedAlligned();
};

void* FixedNoFreeAllocatorTest::AllocatedAlligned() {
  void* tmp = nullptr;
  posix_memalign(&tmp, starboard::common::Allocator::kMinAlignment,
                 kBufferSize);
  return tmp;
}
FixedNoFreeAllocatorTest::FixedNoFreeAllocatorTest()
    : buffer_(static_cast<uint8_t*>(AllocatedAlligned())),
      allocator_(buffer_.get(), kBufferSize) {}

TEST_F(FixedNoFreeAllocatorTest, CanDoSimpleAllocations) {
  void* allocation = allocator_.Allocate(kAllocationSize);

  EXPECT_GE(allocation, buffer_.get());
  EXPECT_LE(reinterpret_cast<uintptr_t>(allocation),
            reinterpret_cast<uintptr_t>(buffer_.get()) + kBufferSize -
                kAllocationSize);
}

TEST_F(FixedNoFreeAllocatorTest, CanDoMultipleAllocationsProperly) {
  void* buffers[kMaxAllocations];
  for (int i = 0; i < kMaxAllocations; ++i) {
    buffers[i] = allocator_.Allocate(kAllocationSize);
    EXPECT_GE(buffers[i], buffer_.get());
    EXPECT_LE(reinterpret_cast<uintptr_t>(buffers[i]),
              reinterpret_cast<uintptr_t>(buffer_.get()) + kBufferSize -
                  kAllocationSize);

    // Make sure this allocation doesn't overlap with any previous ones.
    for (int j = 0; j < i; ++j) {
      EXPECT_NE(buffers[j], buffers[i]);
      if (buffers[j] < buffers[i]) {
        EXPECT_LE(starboard::common::AsInteger(buffers[j]) + kAllocationSize,
                  starboard::common::AsInteger(buffers[i]));
      } else {
        EXPECT_LE(starboard::common::AsInteger(buffers[i]) + kAllocationSize,
                  starboard::common::AsInteger(buffers[j]));
      }
    }
  }
}

TEST_F(FixedNoFreeAllocatorTest, CanDoMultipleAllocationsAndFreesProperly) {
  for (int i = 0; i < kMaxAllocations; ++i) {
    void* current_allocation = allocator_.Allocate(kAllocationSize);

    EXPECT_GE(current_allocation, buffer_.get());
    EXPECT_LE(reinterpret_cast<uintptr_t>(current_allocation),
              reinterpret_cast<uintptr_t>(buffer_.get()) + kBufferSize -
                  kAllocationSize);

    allocator_.Free(current_allocation);
  }
}

TEST_F(FixedNoFreeAllocatorTest, CanHandleOutOfMemory) {
  for (int i = 0; i < kMaxAllocations; ++i) {
    void* current_allocation = allocator_.Allocate(kAllocationSize);

    EXPECT_GE(current_allocation, buffer_.get());
    EXPECT_LE(reinterpret_cast<uintptr_t>(current_allocation),
              reinterpret_cast<uintptr_t>(buffer_.get()) + kBufferSize -
                  kAllocationSize);

    allocator_.Free(current_allocation);
  }

  // We should have exhausted our memory supply now, check that our next
  // allocation returns null.
  void* final_allocation = allocator_.Allocate(kAllocationSize);
  EXPECT_EQ(final_allocation, reinterpret_cast<void*>(NULL));
}

TEST_F(FixedNoFreeAllocatorTest, CanHandleAlignedMemory) {
  const int kMinimumAlignedMemoryAllocations =
      kBufferSize / (kAllocationSize + kAllocationAlignment);

  for (int i = 0; i < kMinimumAlignedMemoryAllocations; ++i) {
    void* current_allocation =
        allocator_.Allocate(kAllocationSize, kAllocationAlignment);
    EXPECT_EQ(0, reinterpret_cast<uintptr_t>(current_allocation) %
                     kAllocationAlignment);

    EXPECT_GE(current_allocation, buffer_.get());
    EXPECT_LE(reinterpret_cast<uintptr_t>(current_allocation),
              reinterpret_cast<uintptr_t>(buffer_.get()) + kBufferSize -
                  kAllocationSize);

    allocator_.Free(current_allocation);
  }
}
