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

#include "media/formats/common/segmented_byte_queue.h"

#include <stdint.h>

#include <array>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

// Three distinct 8-byte appends, so a byte's value tells which segment it came
// from.
constexpr std::array<uint8_t, 8> kSegmentA = {0, 1, 2, 3, 4, 5, 6, 7};
constexpr std::array<uint8_t, 8> kSegmentB = {8, 9, 10, 11, 12, 13, 14, 15};
constexpr std::array<uint8_t, 8> kSegmentC = {16, 17, 18, 19, 20, 21, 22, 23};

std::vector<uint8_t> Flatten(const SegmentedByteQueue::Segments& segments) {
  std::vector<uint8_t> flat;
  for (const auto& segment : segments) {
    flat.insert(flat.end(), segment.begin(), segment.end());
  }
  return flat;
}

std::vector<uint8_t> ToVector(base::span<const uint8_t> data) {
  return std::vector<uint8_t>(data.begin(), data.end());
}

std::vector<uint8_t> Range(uint8_t first, uint8_t last) {
  std::vector<uint8_t> bytes;
  for (uint8_t i = first; i <= last; ++i) {
    bytes.push_back(i);
  }
  return bytes;
}

template <typename Queue>
class SegmentedByteQueueTestBase : public ::testing::Test {
 protected:
  void Push(base::span<const uint8_t> data) {
    EXPECT_TRUE(queue_.Push(data, base::ScopedClosureRunner()));
  }

  // Pushes `data` with a closure that appends `id` to `released_` on release.
  void PushTracked(base::span<const uint8_t> data, int id) {
    base::ScopedClosureRunner release(base::BindOnce(
        [](std::vector<int>* released, int id) { released->push_back(id); },
        &released_, id));
    EXPECT_TRUE(queue_.Push(data, std::move(release)));
  }

  void PushAllThreeTracked() {
    PushTracked(kSegmentA, 0);
    PushTracked(kSegmentB, 1);
    PushTracked(kSegmentC, 2);
  }

  std::vector<int> released_;
  Queue queue_;
};

using BorrowedSegmentedByteQueueTest =
    SegmentedByteQueueTestBase<BorrowedSegmentedByteQueue>;
using OwnedSegmentedByteQueueTest =
    SegmentedByteQueueTestBase<OwnedSegmentedByteQueue>;

TEST_F(BorrowedSegmentedByteQueueTest, StartsEmpty) {
  EXPECT_EQ(0u, queue_.size());
  EXPECT_EQ(0u, queue_.GetSegmentCountForTesting());
  EXPECT_TRUE(queue_.PeekContiguousData().empty());

  EXPECT_TRUE(queue_.PeekSegmentedData(0));
  EXPECT_FALSE(queue_.PeekSegmentedData(1));
}

TEST_F(BorrowedSegmentedByteQueueTest,
       EmptyPushIsIgnoredAndReleasedImmediately) {
  PushTracked(base::span<const uint8_t>(), 0);
  EXPECT_EQ(std::vector<int>({0}), released_);
  EXPECT_EQ(0u, queue_.size());
  EXPECT_EQ(0u, queue_.GetSegmentCountForTesting());
}

TEST_F(BorrowedSegmentedByteQueueTest, ContiguousDataStopsAtSegmentEnd) {
  PushAllThreeTracked();

  base::span<const uint8_t> data = queue_.PeekContiguousData();
  EXPECT_EQ(kSegmentA.data(), data.data());
  EXPECT_EQ(8u, data.size());

  data = queue_.PeekContiguousData(10);
  EXPECT_EQ(&kSegmentB[2], data.data());
  EXPECT_EQ(6u, data.size());

  data = queue_.PeekContiguousData(23);
  EXPECT_EQ(&kSegmentC[7], data.data());
  EXPECT_EQ(1u, data.size());

  EXPECT_TRUE(queue_.PeekContiguousData(24).empty());
}

TEST_F(BorrowedSegmentedByteQueueTest, SegmentedDataSpansAllSegments) {
  PushAllThreeTracked();

  auto result = queue_.PeekSegmentedData(6, 12);
  ASSERT_TRUE(result);
  const auto& segments = *result;
  ASSERT_EQ(3u, segments.size());
  EXPECT_EQ(&kSegmentA[6], segments[0].data());
  EXPECT_EQ(2u, segments[0].size());
  EXPECT_EQ(kSegmentB.data(), segments[1].data());
  EXPECT_EQ(8u, segments[1].size());
  EXPECT_EQ(kSegmentC.data(), segments[2].data());
  EXPECT_EQ(2u, segments[2].size());
  EXPECT_EQ(Range(6, 17), Flatten(segments));
}

TEST_F(BorrowedSegmentedByteQueueTest, SegmentedDataEndingExactlyAtSegmentEnd) {
  PushAllThreeTracked();

  auto segments = queue_.PeekSegmentedData(4, 4);
  ASSERT_TRUE(segments);
  ASSERT_EQ(1u, segments->size());
  EXPECT_EQ(Range(4, 7), Flatten(*segments));
}

TEST_F(BorrowedSegmentedByteQueueTest, ZeroLengthSegmentedData) {
  PushAllThreeTracked();

  auto segments = queue_.PeekSegmentedData(12, 0);
  ASSERT_TRUE(segments);
  EXPECT_TRUE(segments->empty());

  segments = queue_.PeekSegmentedData(24, 0);
  ASSERT_TRUE(segments);
  EXPECT_TRUE(segments->empty());
}

TEST_F(BorrowedSegmentedByteQueueTest, SegmentedDataRejectsUnbufferedRange) {
  PushAllThreeTracked();

  EXPECT_FALSE(queue_.PeekSegmentedData(20, 5));
  EXPECT_FALSE(queue_.PeekSegmentedData(25, 0));
  EXPECT_FALSE(queue_.PeekSegmentedData(25));
}

TEST_F(BorrowedSegmentedByteQueueTest, PopZeroIsANoOp) {
  PushAllThreeTracked();

  queue_.Pop(0);
  EXPECT_EQ(24u, queue_.size());
  EXPECT_EQ(3u, queue_.GetSegmentCountForTesting());
  EXPECT_TRUE(released_.empty());
}

TEST_F(BorrowedSegmentedByteQueueTest, PopWithinSegmentKeepsIt) {
  PushAllThreeTracked();

  queue_.Pop(3);
  EXPECT_EQ(21u, queue_.size());
  EXPECT_EQ(3u, queue_.GetSegmentCountForTesting());
  EXPECT_TRUE(released_.empty());

  // Offsets are relative to the new front.
  base::span<const uint8_t> data = queue_.PeekContiguousData();
  EXPECT_EQ(&kSegmentA[3], data.data());
  EXPECT_EQ(5u, data.size());
}

TEST_F(BorrowedSegmentedByteQueueTest,
       PopAcrossSeveralSegmentsReleasesInOrder) {
  PushAllThreeTracked();

  // Consumes A and B entirely and 3 bytes of C, in one call.
  queue_.Pop(19);
  EXPECT_EQ(std::vector<int>({0, 1}), released_);
  EXPECT_EQ(5u, queue_.size());
  EXPECT_EQ(1u, queue_.GetSegmentCountForTesting());

  auto segments = queue_.PeekSegmentedData(5);
  ASSERT_TRUE(segments);
  EXPECT_EQ(Range(19, 23), Flatten(*segments));

  queue_.Pop(5);
  EXPECT_EQ(std::vector<int>({0, 1, 2}), released_);
  EXPECT_EQ(0u, queue_.size());
  EXPECT_EQ(0u, queue_.GetSegmentCountForTesting());
}

TEST_F(BorrowedSegmentedByteQueueTest, PushAfterPartialPop) {
  PushTracked(kSegmentA, 0);
  queue_.Pop(6);
  PushTracked(kSegmentB, 1);

  EXPECT_EQ(10u, queue_.size());
  auto segments = queue_.PeekSegmentedData(10);
  ASSERT_TRUE(segments);
  ASSERT_EQ(2u, segments->size());
  EXPECT_EQ(Range(6, 15), Flatten(*segments));
}

// Segments point at the caller's memory, so adding more must not move or
// invalidate what has already been handed out.
TEST_F(BorrowedSegmentedByteQueueTest, PushDoesNotInvalidateReturnedSpans) {
  PushTracked(kSegmentA, 0);
  base::span<const uint8_t> contiguous = queue_.PeekContiguousData(2);
  auto result = queue_.PeekSegmentedData(8);
  ASSERT_TRUE(result);
  const auto& segments = *result;

  for (int i = 0; i < 32; ++i) {
    Push(kSegmentB);
  }

  EXPECT_EQ(&kSegmentA[2], contiguous.data());
  EXPECT_EQ(6u, contiguous.size());
  ASSERT_EQ(1u, segments.size());
  EXPECT_EQ(kSegmentA.data(), segments[0].data());
}

TEST_F(BorrowedSegmentedByteQueueTest,
       LinearizedDataWithinSegmentIsReadInPlace) {
  PushAllThreeTracked();

  auto data = queue_.PeekLinearizedData(9, 6);
  ASSERT_TRUE(data);
  EXPECT_EQ(&kSegmentB[1], data->data());
  EXPECT_EQ(6u, data->size());

  data = queue_.PeekLinearizedData(16, 8);
  ASSERT_TRUE(data);
  EXPECT_EQ(kSegmentC.data(), data->data());
  EXPECT_EQ(8u, data->size());

  EXPECT_FALSE(queue_.HasScratchForTesting());
}

TEST_F(BorrowedSegmentedByteQueueTest, LinearizedDataAcrossSegmentsIsGathered) {
  PushAllThreeTracked();

  auto data = queue_.PeekLinearizedData(6, 12);
  ASSERT_TRUE(data);
  EXPECT_EQ(Range(6, 17), ToVector(*data));
  EXPECT_TRUE(queue_.HasScratchForTesting());

  data = queue_.PeekLinearizedData(24);
  ASSERT_TRUE(data);
  EXPECT_EQ(Range(0, 23), ToVector(*data));
}

TEST_F(BorrowedSegmentedByteQueueTest, ZeroLengthLinearizedData) {
  PushAllThreeTracked();

  auto data = queue_.PeekLinearizedData(12, 0);
  ASSERT_TRUE(data);
  EXPECT_TRUE(data->empty());

  data = queue_.PeekLinearizedData(24, 0);
  ASSERT_TRUE(data);
  EXPECT_TRUE(data->empty());

  EXPECT_FALSE(queue_.HasScratchForTesting());
}

TEST_F(BorrowedSegmentedByteQueueTest, LinearizedDataRejectsUnbufferedRange) {
  PushAllThreeTracked();

  EXPECT_FALSE(queue_.PeekLinearizedData(20, 5));
  EXPECT_FALSE(queue_.PeekLinearizedData(24, 1));
  EXPECT_FALSE(queue_.PeekLinearizedData(25, 0));
  EXPECT_FALSE(queue_.PeekLinearizedData(25, 1));
  EXPECT_FALSE(queue_.HasScratchForTesting());
}

TEST_F(BorrowedSegmentedByteQueueTest, ScratchIsReusedUntilReset) {
  PushAllThreeTracked();

  auto data = queue_.PeekLinearizedData(4, 16);
  ASSERT_TRUE(data);
  ASSERT_EQ(Range(4, 19), ToVector(*data));
  const uint8_t* const scratch = data->data();

  // Popping and gathering a smaller range reuses the same storage.
  queue_.Pop(4);
  data = queue_.PeekLinearizedData(2, 4);
  ASSERT_TRUE(data);
  ASSERT_EQ(Range(6, 9), ToVector(*data));
  EXPECT_EQ(scratch, data->data());

  queue_.Reset();
  EXPECT_FALSE(queue_.HasScratchForTesting());
}

TEST_F(BorrowedSegmentedByteQueueTest, PushDoesNotInvalidateLinearizedData) {
  PushTracked(kSegmentA, 0);
  PushTracked(kSegmentB, 1);
  auto gathered = queue_.PeekLinearizedData(6, 4);
  ASSERT_TRUE(gathered);
  ASSERT_EQ(Range(6, 9), ToVector(*gathered));

  for (int i = 0; i < 32; ++i) {
    Push(kSegmentC);
  }

  EXPECT_EQ(Range(6, 9), ToVector(*gathered));
}

TEST_F(BorrowedSegmentedByteQueueTest, ResetReleasesEverything) {
  PushAllThreeTracked();
  queue_.Pop(3);

  queue_.Reset();
  // The release order is unspecified (libc++ releases back to front), so only
  // check that every segment was released.
  EXPECT_THAT(released_, ::testing::UnorderedElementsAre(0, 1, 2));
  EXPECT_EQ(0u, queue_.size());
  EXPECT_EQ(0u, queue_.GetSegmentCountForTesting());
  EXPECT_TRUE(queue_.PeekContiguousData().empty());

  // The queue is usable again afterwards.
  Push(kSegmentC);
  EXPECT_EQ(kSegmentC.data(), queue_.PeekContiguousData().data());
}

TEST_F(BorrowedSegmentedByteQueueTest, DestructionReleasesEverything) {
  std::vector<int> released;
  {
    BorrowedSegmentedByteQueue queue;
    EXPECT_TRUE(queue.Push(
        kSegmentA,
        base::ScopedClosureRunner(base::BindOnce(
            [](std::vector<int>* r) { r->push_back(0); }, &released))));
    EXPECT_TRUE(released.empty());
  }
  EXPECT_EQ(std::vector<int>({0}), released);
}

TEST_F(OwnedSegmentedByteQueueTest, PushCopiesAndReleasesImmediately) {
  PushTracked(kSegmentA, 0);
  EXPECT_EQ(std::vector<int>({0}), released_);
  EXPECT_EQ(8u, queue_.size());

  base::span<const uint8_t> data = queue_.PeekContiguousData();
  EXPECT_NE(kSegmentA.data(), data.data());
  EXPECT_EQ(Range(0, 7), ToVector(data));
}

TEST_F(OwnedSegmentedByteQueueTest, EmptyPushIsIgnoredAndReleasedImmediately) {
  PushTracked(base::span<const uint8_t>(), 0);
  EXPECT_EQ(std::vector<int>({0}), released_);
  EXPECT_EQ(0u, queue_.size());
  EXPECT_TRUE(queue_.PeekContiguousData().empty());
}

TEST_F(OwnedSegmentedByteQueueTest, ReadsAreContiguousAcrossAppends) {
  PushAllThreeTracked();

  base::span<const uint8_t> contiguous = queue_.PeekContiguousData(6);
  EXPECT_EQ(Range(6, 23), ToVector(contiguous));

  auto result = queue_.PeekSegmentedData(6, 12);
  ASSERT_TRUE(result);
  const auto& segments = *result;
  ASSERT_EQ(1u, segments.size());
  EXPECT_EQ(contiguous.data(), segments[0].data());
  EXPECT_EQ(12u, segments[0].size());

  auto linearized = queue_.PeekLinearizedData(6, 12);
  ASSERT_TRUE(linearized);
  EXPECT_EQ(contiguous.data(), linearized->data()) << "read in place";
  EXPECT_EQ(Range(6, 17), ToVector(*linearized));
}

TEST_F(OwnedSegmentedByteQueueTest, ZeroLengthAndUnbufferedRanges) {
  PushAllThreeTracked();

  EXPECT_TRUE(queue_.PeekContiguousData(24).empty());

  EXPECT_FALSE(queue_.PeekSegmentedData(20, 5));
  EXPECT_FALSE(queue_.PeekSegmentedData(25, 0));
  auto segments = queue_.PeekSegmentedData(24, 0);
  ASSERT_TRUE(segments);
  EXPECT_TRUE(segments->empty());

  EXPECT_FALSE(queue_.PeekLinearizedData(20, 5));
  EXPECT_FALSE(queue_.PeekLinearizedData(25, 0));
  EXPECT_FALSE(queue_.PeekLinearizedData(25, 1));
  auto linearized = queue_.PeekLinearizedData(24, 0);
  ASSERT_TRUE(linearized);
  EXPECT_TRUE(linearized->empty());
}

TEST_F(OwnedSegmentedByteQueueTest, PopAndReset) {
  PushAllThreeTracked();

  queue_.Pop(19);
  EXPECT_EQ(5u, queue_.size());
  EXPECT_EQ(Range(19, 23), ToVector(queue_.PeekContiguousData()));
  auto linearized = queue_.PeekLinearizedData(5);
  ASSERT_TRUE(linearized);
  EXPECT_EQ(Range(19, 23), ToVector(*linearized));
  auto segments = queue_.PeekSegmentedData(5);
  ASSERT_TRUE(segments);
  EXPECT_EQ(Range(19, 23), Flatten(*segments));

  queue_.Reset();
  EXPECT_EQ(0u, queue_.size());
  EXPECT_TRUE(queue_.PeekContiguousData().empty());
}

// The two implementations are told apart by whether the queue reads the
// appended memory in place.
TEST(SegmentedByteQueueCreateTest, CreatesOwnedQueue) {
  std::unique_ptr<SegmentedByteQueue> queue =
      SegmentedByteQueue::Create(/*borrow_mode=*/false);
  ASSERT_TRUE(queue->Push(kSegmentA, base::ScopedClosureRunner()));
  EXPECT_NE(kSegmentA.data(), queue->PeekContiguousData().data());
}

TEST(SegmentedByteQueueCreateTest, CreatesBorrowedQueue) {
  std::unique_ptr<SegmentedByteQueue> queue =
      SegmentedByteQueue::Create(/*borrow_mode=*/true);
  ASSERT_TRUE(queue->Push(kSegmentA, base::ScopedClosureRunner()));
  EXPECT_EQ(kSegmentA.data(), queue->PeekContiguousData().data());
}

}  // namespace

}  // namespace media
