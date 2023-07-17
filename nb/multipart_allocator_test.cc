/*
 * Copyright 2017 Google Inc. All Rights Reserved.
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

#include "nb/multipart_allocator.h"

#include <vector>

#include "starboard/common/log.h"
#include "starboard/common/memory.h"
#include "starboard/configuration.h"
#include "starboard/memory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nb {
namespace {

using starboard::common::MemoryIsZero;

TEST(MultipartAllocatorAllocationsTest, DefaultCtor) {
  MultipartAllocator::Allocations allocations;
  EXPECT_EQ(allocations.number_of_buffers(), 0);
  // Call these functions as a sanity check that they are still callable.
  allocations.buffers();
  allocations.buffer_sizes();
}

TEST(MultipartAllocatorAllocationsTest, CopyCtor) {
  {
    // Allocations with 0 blocks.
    MultipartAllocator::Allocations allocations;
    MultipartAllocator::Allocations copy(allocations);
    EXPECT_EQ(copy.number_of_buffers(), 0);
    // Call these functions as a sanity check that they are still callable.
    copy.buffers();
    copy.buffer_sizes();
  }

  {
    // Allocations with one blocks.
    const int kBufferSize = 128;
    char buffer[kBufferSize];

    MultipartAllocator::Allocations allocations(buffer, kBufferSize);
    MultipartAllocator::Allocations copy(allocations);
    EXPECT_EQ(copy.number_of_buffers(), 1);
    EXPECT_EQ(copy.buffers()[0], buffer);
    EXPECT_EQ(copy.buffer_sizes()[0], kBufferSize);
  }

  {
    // Allocations with more than one blocks.
    const int kBufferSize0 = 128;
    const int kBufferSize1 = 16;
    char buffer0[kBufferSize0];
    char buffer1[kBufferSize1];

    std::vector<void*> buffers = {buffer0, buffer1};
    std::vector<int> buffer_sizes = {kBufferSize0, kBufferSize1};

    MultipartAllocator::Allocations allocations(
        static_cast<int>(buffers.size()), buffers.data(), buffer_sizes.data());
    MultipartAllocator::Allocations copy(allocations);
    EXPECT_EQ(copy.number_of_buffers(), 2);
    EXPECT_EQ(copy.buffers()[0], buffer0);
    EXPECT_EQ(copy.buffer_sizes()[0], kBufferSize0);
    EXPECT_EQ(copy.buffers()[1], buffer1);
    EXPECT_EQ(copy.buffer_sizes()[1], kBufferSize1);
  }
}

TEST(MultipartAllocatorAllocationsTest, AssignmentOperator) {
  {
    // Allocations with 0 blocks.
    MultipartAllocator::Allocations allocations;
    MultipartAllocator::Allocations copy;
    copy = allocations;
    EXPECT_EQ(copy.number_of_buffers(), 0);
    // Call these functions as a sanity check that they are still callable.
    copy.buffers();
    copy.buffer_sizes();
  }

  {
    // Allocations with one blocks.
    const int kBufferSize = 128;
    char buffer[kBufferSize];

    MultipartAllocator::Allocations allocations(buffer, kBufferSize);
    MultipartAllocator::Allocations copy;
    copy = allocations;
    EXPECT_EQ(copy.number_of_buffers(), 1);
    EXPECT_EQ(copy.buffers()[0], buffer);
    EXPECT_EQ(copy.buffer_sizes()[0], kBufferSize);
  }

  {
    // Allocations with more than one blocks.
    const int kBufferSize0 = 128;
    const int kBufferSize1 = 16;
    char buffer0[kBufferSize0];
    char buffer1[kBufferSize1];

    std::vector<void*> buffers = {buffer0, buffer1};
    std::vector<int> buffer_sizes = {kBufferSize0, kBufferSize1};

    MultipartAllocator::Allocations allocations(
        static_cast<int>(buffers.size()), buffers.data(), buffer_sizes.data());
    MultipartAllocator::Allocations copy;
    copy = allocations;
    EXPECT_EQ(copy.number_of_buffers(), 2);
    EXPECT_EQ(copy.buffers()[0], buffer0);
    EXPECT_EQ(copy.buffer_sizes()[0], kBufferSize0);
    EXPECT_EQ(copy.buffers()[1], buffer1);
    EXPECT_EQ(copy.buffer_sizes()[1], kBufferSize1);
  }
}

TEST(MultipartAllocatorAllocationsTest, SingleBuffer) {
  const int kBufferSize = 128;
  char buffer[kBufferSize];

  MultipartAllocator::Allocations allocations(buffer, kBufferSize);
  EXPECT_EQ(allocations.number_of_buffers(), 1);
  EXPECT_EQ(allocations.buffers()[0], buffer);
  EXPECT_EQ(allocations.buffer_sizes()[0], kBufferSize);
}

TEST(MultipartAllocatorAllocationsTest, SingleBufferShrunk) {
  const int kBufferSize = 128;
  char buffer[kBufferSize];

  MultipartAllocator::Allocations allocations(buffer, kBufferSize);

  allocations.ShrinkTo(kBufferSize / 2);
  EXPECT_EQ(allocations.number_of_buffers(), 1);
  EXPECT_EQ(allocations.buffers()[0], buffer);
  EXPECT_EQ(allocations.buffer_sizes()[0], kBufferSize / 2);

  allocations.ShrinkTo(0);
  EXPECT_EQ(allocations.number_of_buffers(), 1);
  EXPECT_EQ(allocations.buffers()[0], buffer);
  EXPECT_EQ(allocations.buffer_sizes()[0], 0);
}

TEST(MultipartAllocatorAllocationsTest, SingleBufferWrite) {
  const int kBufferSize = 128;
  char buffer[kBufferSize * 2];  // Use extra space for boundary checking.
  char source[kBufferSize];

  MultipartAllocator::Allocations allocations(buffer, kBufferSize);
  memset(source, 'x', kBufferSize);

  memset(buffer, 0, kBufferSize * 2);
  allocations.Write(0, source, kBufferSize);
  EXPECT_EQ(memcmp(buffer, source, kBufferSize), 0);
  EXPECT_TRUE(MemoryIsZero(buffer + kBufferSize, kBufferSize));

  memset(buffer, 0, kBufferSize * 2);
  allocations.Write(kBufferSize / 2, source, kBufferSize / 2);
  EXPECT_TRUE(MemoryIsZero(buffer, kBufferSize / 2));
  EXPECT_EQ(memcmp(buffer + kBufferSize / 2, source, kBufferSize / 2), 0);
  EXPECT_TRUE(MemoryIsZero(buffer + kBufferSize, kBufferSize));

  memset(buffer, 0, kBufferSize * 2);
  allocations.Write(kBufferSize, source, 0);
  EXPECT_TRUE(MemoryIsZero(buffer, kBufferSize * 2));
  EXPECT_TRUE(MemoryIsZero(buffer + kBufferSize, kBufferSize));
}

TEST(MultipartAllocatorAllocationsTest, SingleBufferRead) {
  const int kBufferSize = 128;
  char buffer[kBufferSize];
  char destination[kBufferSize * 2];

  MultipartAllocator::Allocations allocations(buffer, kBufferSize);

  memset(buffer, 'x', kBufferSize);
  memset(destination, 0, kBufferSize * 2);
  allocations.Read(destination);
  EXPECT_EQ(memcmp(buffer, destination, kBufferSize), 0);
  EXPECT_TRUE(MemoryIsZero(destination + kBufferSize, kBufferSize));
}

TEST(MultipartAllocatorAllocationsTest, MultipleBuffers) {
  const int kBufferSize0 = 128;
  const int kBufferSize1 = 16;
  char buffer0[kBufferSize0];
  char buffer1[kBufferSize1];

  std::vector<void*> buffers = {buffer0, buffer1};
  std::vector<int> buffer_sizes = {kBufferSize0, kBufferSize1};

  MultipartAllocator::Allocations allocations(
      static_cast<int>(buffers.size()), buffers.data(), buffer_sizes.data());
  EXPECT_EQ(allocations.number_of_buffers(), 2);
  EXPECT_EQ(allocations.buffers()[0], buffer0);
  EXPECT_EQ(allocations.buffers()[1], buffer1);
  EXPECT_EQ(allocations.buffer_sizes()[0], kBufferSize0);
  EXPECT_EQ(allocations.buffer_sizes()[1], kBufferSize1);
}

TEST(MultipartAllocatorAllocationsTest, MultipleBuffersShrink) {
  const int kBufferSize0 = 128;
  const int kBufferSize1 = 16;
  char buffer0[kBufferSize0];
  char buffer1[kBufferSize1];

  std::vector<void*> buffers = {buffer0, buffer1};
  std::vector<int> buffer_sizes = {kBufferSize0, kBufferSize1};

  MultipartAllocator::Allocations allocations(
      static_cast<int>(buffers.size()), buffers.data(), buffer_sizes.data());

  allocations.ShrinkTo(kBufferSize0 + kBufferSize1 / 2);
  EXPECT_EQ(allocations.number_of_buffers(), 2);
  EXPECT_EQ(allocations.buffers()[0], buffer0);
  EXPECT_EQ(allocations.buffers()[1], buffer1);
  EXPECT_EQ(allocations.buffer_sizes()[0], kBufferSize0);
  EXPECT_EQ(allocations.buffer_sizes()[1], kBufferSize1 / 2);

  allocations.ShrinkTo(kBufferSize0);
  EXPECT_EQ(allocations.number_of_buffers(), 2);
  EXPECT_EQ(allocations.buffers()[0], buffer0);
  EXPECT_EQ(allocations.buffers()[1], buffer1);
  EXPECT_EQ(allocations.buffer_sizes()[0], kBufferSize0);
  EXPECT_EQ(allocations.buffer_sizes()[1], 0);

  allocations.ShrinkTo(kBufferSize0 / 2);
  EXPECT_EQ(allocations.number_of_buffers(), 2);
  EXPECT_EQ(allocations.buffers()[0], buffer0);
  EXPECT_EQ(allocations.buffers()[1], buffer1);
  EXPECT_EQ(allocations.buffer_sizes()[0], kBufferSize0 / 2);
  EXPECT_EQ(allocations.buffer_sizes()[1], 0);

  allocations.ShrinkTo(0);
  EXPECT_EQ(allocations.number_of_buffers(), 2);
  EXPECT_EQ(allocations.buffers()[0], buffer0);
  EXPECT_EQ(allocations.buffers()[1], buffer1);
  EXPECT_EQ(allocations.buffer_sizes()[0], 0);
  EXPECT_EQ(allocations.buffer_sizes()[1], 0);
}

TEST(MultipartAllocatorAllocationsTest, MultipleBuffersWrite) {
  const int kBufferSize0 = 128;
  const int kBufferSize1 = 16;
  char buffer0[kBufferSize0];
  char buffer1[kBufferSize1 * 2];  // Use extra space for boundary checking.
  char source[kBufferSize0 + kBufferSize1];

  std::vector<void*> buffers = {buffer0, buffer1};
  std::vector<int> buffer_sizes = {kBufferSize0, kBufferSize1};

  MultipartAllocator::Allocations allocations(
      static_cast<int>(buffers.size()), buffers.data(), buffer_sizes.data());
  memset(source, 'x', kBufferSize0 + kBufferSize1);

  memset(buffer0, 0, kBufferSize0);
  memset(buffer1, 0, kBufferSize1 * 2);
  allocations.Write(0, source, kBufferSize0 + kBufferSize1);
  EXPECT_EQ(memcmp(buffer0, source, kBufferSize0), 0);
  EXPECT_EQ(memcmp(buffer1, source, kBufferSize1), 0);
  EXPECT_TRUE(MemoryIsZero(buffer1 + kBufferSize1, kBufferSize1));

  memset(buffer0, 0, kBufferSize0);
  memset(buffer1, 0, kBufferSize1 * 2);
  allocations.Write(kBufferSize0 / 2, source, kBufferSize0 / 2 + kBufferSize1);
  EXPECT_TRUE(MemoryIsZero(buffer0, kBufferSize0 / 2));
  EXPECT_EQ(memcmp(buffer0 + kBufferSize0 / 2, source, kBufferSize0 / 2), 0);
  EXPECT_EQ(memcmp(buffer1, source, kBufferSize1), 0);
  EXPECT_TRUE(MemoryIsZero(buffer1 + kBufferSize1, kBufferSize1));

  memset(buffer0, 0, kBufferSize0);
  memset(buffer1, 0, kBufferSize1 * 2);
  allocations.Write(kBufferSize0, source, kBufferSize1);
  EXPECT_TRUE(MemoryIsZero(buffer0, kBufferSize0));
  EXPECT_EQ(memcmp(buffer1, source, kBufferSize1), 0);
  EXPECT_TRUE(MemoryIsZero(buffer1 + kBufferSize1, kBufferSize1));

  memset(buffer0, 0, kBufferSize0);
  memset(buffer1, 0, kBufferSize1 * 2);
  allocations.Write(kBufferSize0 + kBufferSize1, source, 0);
  EXPECT_TRUE(MemoryIsZero(buffer0, kBufferSize0));
  EXPECT_TRUE(MemoryIsZero(buffer1, kBufferSize1 * 2));
}

TEST(MultipartAllocatorAllocationsTest, MultipleBuffersRead) {
  const int kBufferSize0 = 128;
  const int kBufferSize1 = 16;
  char buffer0[kBufferSize0];
  char buffer1[kBufferSize1];
  char destination[kBufferSize0 + kBufferSize1 * 2];

  std::vector<void*> buffers = {buffer0, buffer1};
  std::vector<int> buffer_sizes = {kBufferSize0, kBufferSize1};

  MultipartAllocator::Allocations allocations(
      static_cast<int>(buffers.size()), buffers.data(), buffer_sizes.data());

  memset(buffer0, 'x', kBufferSize0);
  memset(buffer1, 'y', kBufferSize1);
  memset(destination, 0, kBufferSize0 + kBufferSize1 * 2);
  allocations.Read(destination);
  EXPECT_EQ(memcmp(buffer0, destination, kBufferSize0), 0);
  EXPECT_EQ(memcmp(buffer1, destination + kBufferSize0, kBufferSize1), 0);
  EXPECT_TRUE(
      MemoryIsZero(destination + kBufferSize0 + kBufferSize1, kBufferSize1));
}

}  // namespace
}  // namespace nb
