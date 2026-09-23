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

#include "media/base/segmented_byte_queue.h"

#include <stdint.h>

#include <array>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

// Three distinct 8-byte appends, so a byte's value tells which segment it came
// from.
constexpr std::array<uint8_t, 8> kSegmentA = {0, 1, 2, 3, 4, 5, 6, 7};
constexpr std::array<uint8_t, 8> kSegmentB = {8, 9, 10, 11, 12, 13, 14, 15};
constexpr std::array<uint8_t, 8> kSegmentC = {16, 17, 18, 19, 20, 21, 22, 23};

std::vector<uint8_t> Flatten(
    const std::vector<base::span<const uint8_t>>& segments) {
  std::vector<uint8_t> flat;
  for (const auto& segment : segments) {
    flat.insert(flat.end(), segment.begin(), segment.end());
  }
  return flat;
}

std::vector<uint8_t> Range(uint8_t first, uint8_t last) {
  std::vector<uint8_t> bytes;
  for (uint8_t i = first; i <= last; ++i) {
    bytes.push_back(i);
  }
  return bytes;
}

class SegmentedByteQueueTest : public ::testing::Test {
 protected:
  void Push(base::span<const uint8_t> data) {
    queue_.Push(data, base::ScopedClosureRunner());
  }

  // Pushes `data` with a closure that appends `id` to `released_` on release.
  void PushTracked(base::span<const uint8_t> data, int id) {
    queue_.Push(
        data,
        base::ScopedClosureRunner(base::BindOnce(
            [](std::vector<int>* released, int id) { released->push_back(id); },
            &released_, id)));
  }

  void PushAllThreeTracked() {
    PushTracked(kSegmentA, 0);
    PushTracked(kSegmentB, 1);
    PushTracked(kSegmentC, 2);
  }

  std::vector<int> released_;
  SegmentedByteQueue queue_;
};

TEST_F(SegmentedByteQueueTest, StartsEmpty) {
  EXPECT_EQ(0u, queue_.size());
  EXPECT_EQ(0u, queue_.GetSegmentCountForTesting());
  EXPECT_TRUE(queue_.GetContiguousData().empty());

  std::vector<base::span<const uint8_t>> segments;
  EXPECT_TRUE(queue_.GetSegmentedData(&segments, 0));
  EXPECT_FALSE(queue_.GetSegmentedData(&segments, 1));
}

TEST_F(SegmentedByteQueueTest, EmptyPushIsIgnoredAndReleasedImmediately) {
  PushTracked(base::span<const uint8_t>(), 0);
  EXPECT_EQ(std::vector<int>({0}), released_);
  EXPECT_EQ(0u, queue_.size());
  EXPECT_EQ(0u, queue_.GetSegmentCountForTesting());
}

TEST_F(SegmentedByteQueueTest, ContiguousDataStopsAtSegmentEnd) {
  PushAllThreeTracked();

  base::span<const uint8_t> data = queue_.GetContiguousData();
  EXPECT_EQ(kSegmentA.data(), data.data());
  EXPECT_EQ(8u, data.size());

  data = queue_.GetContiguousData(10);
  EXPECT_EQ(&kSegmentB[2], data.data());
  EXPECT_EQ(6u, data.size());

  data = queue_.GetContiguousData(23);
  EXPECT_EQ(&kSegmentC[7], data.data());
  EXPECT_EQ(1u, data.size());

  EXPECT_TRUE(queue_.GetContiguousData(24).empty());
}

TEST_F(SegmentedByteQueueTest, SegmentedDataSpansAllSegments) {
  PushAllThreeTracked();

  std::vector<base::span<const uint8_t>> segments;
  ASSERT_TRUE(queue_.GetSegmentedData(&segments, 12, 6));
  ASSERT_EQ(3u, segments.size());
  EXPECT_EQ(&kSegmentA[6], segments[0].data());
  EXPECT_EQ(2u, segments[0].size());
  EXPECT_EQ(kSegmentB.data(), segments[1].data());
  EXPECT_EQ(8u, segments[1].size());
  EXPECT_EQ(kSegmentC.data(), segments[2].data());
  EXPECT_EQ(2u, segments[2].size());
  EXPECT_EQ(Range(6, 17), Flatten(segments));
}

TEST_F(SegmentedByteQueueTest, SegmentedDataEndingExactlyAtSegmentEnd) {
  PushAllThreeTracked();

  std::vector<base::span<const uint8_t>> segments;
  ASSERT_TRUE(queue_.GetSegmentedData(&segments, 4, 4));
  ASSERT_EQ(1u, segments.size());
  EXPECT_EQ(Range(4, 7), Flatten(segments));
}

TEST_F(SegmentedByteQueueTest, ZeroLengthSegmentedData) {
  PushAllThreeTracked();

  std::vector<base::span<const uint8_t>> segments = {kSegmentA};
  EXPECT_TRUE(queue_.GetSegmentedData(&segments, 0, 12));
  EXPECT_TRUE(segments.empty()) << "a successful call replaces the contents";

  EXPECT_TRUE(queue_.GetSegmentedData(&segments, 0, 24));
  EXPECT_TRUE(segments.empty());
}

TEST_F(SegmentedByteQueueTest, SegmentedDataRejectsUnbufferedRange) {
  PushAllThreeTracked();

  std::vector<base::span<const uint8_t>> segments = {kSegmentA};
  EXPECT_FALSE(queue_.GetSegmentedData(&segments, 5, 20));
  EXPECT_FALSE(queue_.GetSegmentedData(&segments, 0, 25));
  EXPECT_FALSE(queue_.GetSegmentedData(&segments, 25));

  // A failed call leaves the output untouched.
  ASSERT_EQ(1u, segments.size());
  EXPECT_EQ(kSegmentA.data(), segments[0].data());
}

TEST_F(SegmentedByteQueueTest, PopZeroIsANoOp) {
  PushAllThreeTracked();

  queue_.Pop(0);
  EXPECT_EQ(24u, queue_.size());
  EXPECT_EQ(3u, queue_.GetSegmentCountForTesting());
  EXPECT_TRUE(released_.empty());
}

TEST_F(SegmentedByteQueueTest, PopWithinSegmentKeepsIt) {
  PushAllThreeTracked();

  queue_.Pop(3);
  EXPECT_EQ(21u, queue_.size());
  EXPECT_EQ(3u, queue_.GetSegmentCountForTesting());
  EXPECT_TRUE(released_.empty());

  // Offsets are relative to the new front.
  base::span<const uint8_t> data = queue_.GetContiguousData();
  EXPECT_EQ(&kSegmentA[3], data.data());
  EXPECT_EQ(5u, data.size());
}

TEST_F(SegmentedByteQueueTest, PopAcrossSeveralSegmentsReleasesInOrder) {
  PushAllThreeTracked();

  // Consumes A and B entirely and 3 bytes of C, in one call.
  queue_.Pop(19);
  EXPECT_EQ(std::vector<int>({0, 1}), released_);
  EXPECT_EQ(5u, queue_.size());
  EXPECT_EQ(1u, queue_.GetSegmentCountForTesting());

  std::vector<base::span<const uint8_t>> segments;
  ASSERT_TRUE(queue_.GetSegmentedData(&segments, 5));
  EXPECT_EQ(Range(19, 23), Flatten(segments));

  queue_.Pop(5);
  EXPECT_EQ(std::vector<int>({0, 1, 2}), released_);
  EXPECT_EQ(0u, queue_.size());
  EXPECT_EQ(0u, queue_.GetSegmentCountForTesting());
}

TEST_F(SegmentedByteQueueTest, PushAfterPartialPop) {
  PushTracked(kSegmentA, 0);
  queue_.Pop(6);
  PushTracked(kSegmentB, 1);

  EXPECT_EQ(10u, queue_.size());
  std::vector<base::span<const uint8_t>> segments;
  ASSERT_TRUE(queue_.GetSegmentedData(&segments, 10));
  ASSERT_EQ(2u, segments.size());
  EXPECT_EQ(Range(6, 15), Flatten(segments));
}

// Segments point at the caller's memory, so adding more must not move or
// invalidate what has already been handed out.
TEST_F(SegmentedByteQueueTest, PushDoesNotInvalidateReturnedSpans) {
  PushTracked(kSegmentA, 0);
  base::span<const uint8_t> contiguous = queue_.GetContiguousData(2);
  std::vector<base::span<const uint8_t>> segments;
  ASSERT_TRUE(queue_.GetSegmentedData(&segments, 8));

  for (int i = 0; i < 32; ++i) {
    Push(kSegmentB);
  }

  EXPECT_EQ(&kSegmentA[2], contiguous.data());
  EXPECT_EQ(6u, contiguous.size());
  ASSERT_EQ(1u, segments.size());
  EXPECT_EQ(kSegmentA.data(), segments[0].data());
}

TEST_F(SegmentedByteQueueTest, ResetReleasesEverything) {
  PushAllThreeTracked();
  queue_.Pop(3);

  queue_.Reset();
  EXPECT_EQ(std::vector<int>({0, 1, 2}), released_);
  EXPECT_EQ(0u, queue_.size());
  EXPECT_EQ(0u, queue_.GetSegmentCountForTesting());
  EXPECT_TRUE(queue_.GetContiguousData().empty());

  // The queue is usable again afterwards.
  Push(kSegmentC);
  EXPECT_EQ(kSegmentC.data(), queue_.GetContiguousData().data());
}

TEST_F(SegmentedByteQueueTest, DestructionReleasesEverything) {
  std::vector<int> released;
  {
    SegmentedByteQueue queue;
    queue.Push(kSegmentA,
               base::ScopedClosureRunner(base::BindOnce(
                   [](std::vector<int>* r) { r->push_back(0); }, &released)));
    EXPECT_TRUE(released.empty());
  }
  EXPECT_EQ(std::vector<int>({0}), released);
}

}  // namespace

}  // namespace media
