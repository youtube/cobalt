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

#include "media/formats/common/offset_segmented_byte_queue.h"

#include <stdint.h>

#include <array>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

// Three distinct 8-byte appends, so that tests can tell which segment a byte
// came from purely by its value.
constexpr std::array<uint8_t, 8> kSegmentA = {0, 1, 2, 3, 4, 5, 6, 7};
constexpr std::array<uint8_t, 8> kSegmentB = {8, 9, 10, 11, 12, 13, 14, 15};
constexpr std::array<uint8_t, 8> kSegmentC = {16, 17, 18, 19, 20, 21, 22, 23};

// Flattens `segments` so tests can assert on content without caring about how
// it was split into runs.
std::vector<uint8_t> Flatten(
    const std::vector<base::span<const uint8_t>>& segments) {
  std::vector<uint8_t> flat;
  for (const auto& segment : segments) {
    flat.insert(flat.end(), segment.begin(), segment.end());
  }
  return flat;
}

// The bytes 0..23, i.e. the concatenation of the three canonical segments.
std::vector<uint8_t> AllBytes() {
  std::vector<uint8_t> expected;
  for (uint8_t i = 0; i < 24; ++i) {
    expected.push_back(i);
  }
  return expected;
}

class OffsetSegmentedByteQueueTest : public ::testing::Test {
 protected:
  // Appends `data` with no retention closure, for tests that do not care about
  // release timing.
  void Push(base::span<const uint8_t> data) {
    queue_.Push(data, base::ScopedClosureRunner());
  }

  // A retention closure that sets `*flag` when the segment is released.
  static base::ScopedClosureRunner ReleaseFlagSetter(bool* flag) {
    return base::ScopedClosureRunner(
        base::BindOnce([](bool* f) { *f = true; }, flag));
  }

  // Pushes the three canonical segments, giving a queue of 24 bytes valued
  // 0..23 spread over three segments.
  void PushAllThree() {
    Push(kSegmentA);
    Push(kSegmentB);
    Push(kSegmentC);
  }

  OffsetSegmentedByteQueue queue_;
};

TEST_F(OffsetSegmentedByteQueueTest, StartsEmpty) {
  EXPECT_EQ(0, queue_.head());
  EXPECT_EQ(0, queue_.tail());
  EXPECT_EQ(0u, queue_.size());

  std::vector<base::span<const uint8_t>> segments;
  EXPECT_FALSE(queue_.GetSegmentedData(&segments, 1, 0));
}

TEST_F(OffsetSegmentedByteQueueTest, PushAdvancesTailNotHead) {
  PushAllThree();
  EXPECT_EQ(0, queue_.head());
  EXPECT_EQ(24, queue_.tail());
  EXPECT_EQ(24u, queue_.size());
  EXPECT_EQ(3u, queue_.GetSegmentCountForTesting());
}

// The central property of this class: a range is readable even though it is
// not contiguous. A parser gating on contiguous length would stall here.
TEST_F(OffsetSegmentedByteQueueTest, StraddlingRangeIsReturnedAsSeveralRuns) {
  PushAllThree();

  // Bytes 6..9 cross the A/B boundary.
  std::vector<base::span<const uint8_t>> segments;
  ASSERT_TRUE(queue_.GetSegmentedData(&segments, 4, 6));

  ASSERT_EQ(2u, segments.size());
  EXPECT_EQ(2u, segments[0].size());
  EXPECT_EQ(2u, segments[1].size());
  EXPECT_EQ(std::vector<uint8_t>({6, 7, 8, 9}), Flatten(segments));
}

TEST_F(OffsetSegmentedByteQueueTest, RangeWithinOneSegmentIsASingleRun) {
  PushAllThree();

  std::vector<base::span<const uint8_t>> segments;
  ASSERT_TRUE(queue_.GetSegmentedData(&segments, 8, 8));

  ASSERT_EQ(1u, segments.size());
  EXPECT_EQ(std::vector<uint8_t>(kSegmentB.begin(), kSegmentB.end()),
            Flatten(segments));
}

TEST_F(OffsetSegmentedByteQueueTest, RangeSpanningEverything) {
  PushAllThree();

  std::vector<base::span<const uint8_t>> segments;
  ASSERT_TRUE(queue_.GetSegmentedData(&segments, 24, 0));
  EXPECT_EQ(3u, segments.size());
  EXPECT_EQ(AllBytes(), Flatten(segments));
}

TEST_F(OffsetSegmentedByteQueueTest, GetSegmentedDataRejectsUnbufferedRange) {
  PushAllThree();

  std::vector<base::span<const uint8_t>> segments;
  EXPECT_FALSE(queue_.GetSegmentedData(&segments, 8, 20));
  EXPECT_FALSE(queue_.GetSegmentedData(&segments, 1, 24));
  EXPECT_FALSE(queue_.GetSegmentedData(&segments, 1, 100));
}

TEST_F(OffsetSegmentedByteQueueTest,
       GetSegmentedDataLeavesOutputAloneOnFailure) {
  PushAllThree();

  std::vector<base::span<const uint8_t>> segments;
  ASSERT_TRUE(queue_.GetSegmentedData(&segments, 8, 0));
  ASSERT_EQ(1u, segments.size());

  // Asking for more than is buffered must not disturb the previous result.
  EXPECT_FALSE(queue_.GetSegmentedData(&segments, 25, 0));
  EXPECT_EQ(1u, segments.size());
}

// The run list is caller-owned, so two results can be live simultaneously.
TEST_F(OffsetSegmentedByteQueueTest, ConcurrentResultsAreIndependent) {
  PushAllThree();

  std::vector<base::span<const uint8_t>> first;
  std::vector<base::span<const uint8_t>> second;
  ASSERT_TRUE(queue_.GetSegmentedData(&first, 8, 0));
  ASSERT_TRUE(queue_.GetSegmentedData(&second, 8, 8));

  EXPECT_EQ(std::vector<uint8_t>(kSegmentA.begin(), kSegmentA.end()),
            Flatten(first));
  EXPECT_EQ(std::vector<uint8_t>(kSegmentB.begin(), kSegmentB.end()),
            Flatten(second));
}

TEST_F(OffsetSegmentedByteQueueTest, PopAdvancesHeadAndKeepsOffsetsStable) {
  PushAllThree();

  queue_.Pop(10);
  EXPECT_EQ(10, queue_.head());
  EXPECT_EQ(24, queue_.tail());
  EXPECT_EQ(14u, queue_.size());

  // Absolute offsets still address the same bytes after popping.
  std::vector<base::span<const uint8_t>> segments;
  ASSERT_TRUE(queue_.GetSegmentedData(&segments, 6, 10));
  EXPECT_EQ(std::vector<uint8_t>({10, 11, 12, 13, 14, 15}), Flatten(segments));
}

TEST_F(OffsetSegmentedByteQueueTest, PopReleasesWholeSegmentsOnly) {
  PushAllThree();
  ASSERT_EQ(3u, queue_.GetSegmentCountForTesting());

  // Partially into the first segment: nothing can be released yet.
  queue_.Pop(4);
  EXPECT_EQ(3u, queue_.GetSegmentCountForTesting());

  // Exactly to the end of the first segment: it goes away.
  queue_.Pop(4);
  EXPECT_EQ(2u, queue_.GetSegmentCountForTesting());
}

TEST_F(OffsetSegmentedByteQueueTest, ReadingBeforeHeadFails) {
  PushAllThree();
  queue_.Pop(8);

  std::vector<base::span<const uint8_t>> segments;
  EXPECT_FALSE(queue_.GetSegmentedData(&segments, 1, 4));

  EXPECT_TRUE(queue_.GetContiguousData(4).empty());
}

TEST_F(OffsetSegmentedByteQueueTest, TrimMirrorsOffsetByteQueueSemantics) {
  PushAllThree();

  // Trimming behind the head is a no-op success.
  EXPECT_TRUE(queue_.Trim(-1));
  EXPECT_EQ(0, queue_.head());

  EXPECT_TRUE(queue_.Trim(8));
  EXPECT_EQ(8, queue_.head());

  // Past the tail: drops everything buffered, reports failure.
  EXPECT_FALSE(queue_.Trim(100));
  EXPECT_EQ(24, queue_.head());
  EXPECT_EQ(24, queue_.tail());
  EXPECT_EQ(0u, queue_.GetSegmentCountForTesting());
}

// A segment must not be released until it has been fully consumed, since the
// parser may still be reading through it.
TEST_F(OffsetSegmentedByteQueueTest, SegmentReleasedOnlyWhenFullyConsumed) {
  bool released = false;

  queue_.Push(kSegmentA, ReleaseFlagSetter(&released));
  Push(kSegmentB);

  EXPECT_FALSE(released);

  queue_.Pop(7);
  EXPECT_FALSE(released) << "one byte of the first segment still unread";

  queue_.Pop(1);
  EXPECT_TRUE(released);
}

TEST_F(OffsetSegmentedByteQueueTest, SegmentsAreReadWithoutCopying) {
  Push(kSegmentA);

  std::vector<base::span<const uint8_t>> segments;
  ASSERT_TRUE(queue_.GetSegmentedData(&segments, 8, 0));

  // The view must point at the caller's memory, not a copy of it.
  ASSERT_EQ(1u, segments.size());
  EXPECT_EQ(kSegmentA.data(), segments[0].data());
}

TEST_F(OffsetSegmentedByteQueueTest, ResetReleasesSegments) {
  bool released = false;

  queue_.Push(kSegmentA, ReleaseFlagSetter(&released));
  ASSERT_FALSE(released);

  queue_.Reset();
  EXPECT_TRUE(released);
  EXPECT_EQ(0, queue_.head());
  EXPECT_EQ(0, queue_.tail());
}

TEST_F(OffsetSegmentedByteQueueTest, DestructionReleasesSegments) {
  bool released = false;
  {
    OffsetSegmentedByteQueue queue;
    queue.Push(kSegmentA, ReleaseFlagSetter(&released));
    EXPECT_FALSE(released);
  }
  EXPECT_TRUE(released);
}

// Segments with and without retention closures coexist, and a release fires
// only for the segment it was pushed with.
TEST_F(OffsetSegmentedByteQueueTest, RetainedAndUnretainedSegmentsInterleave) {
  bool released = false;

  Push(kSegmentA);
  queue_.Push(kSegmentB, ReleaseFlagSetter(&released));
  Push(kSegmentC);

  std::vector<base::span<const uint8_t>> segments;
  ASSERT_TRUE(queue_.GetSegmentedData(&segments, 24, 0));
  EXPECT_EQ(AllBytes(), Flatten(segments));

  queue_.Pop(8);
  EXPECT_FALSE(released) << "only the unretained first segment is gone";

  queue_.Pop(8);
  EXPECT_TRUE(released);
}

TEST_F(OffsetSegmentedByteQueueTest, EmptyPushesAreIgnored) {
  Push(base::span<const uint8_t>());
  EXPECT_EQ(0u, queue_.GetSegmentCountForTesting());
  EXPECT_EQ(0, queue_.tail());

  // An empty push must still drop its retention rather than leak it.
  bool released = false;
  queue_.Push(base::span<const uint8_t>(), ReleaseFlagSetter(&released));
  EXPECT_TRUE(released);
  EXPECT_EQ(0u, queue_.GetSegmentCountForTesting());
}

TEST_F(OffsetSegmentedByteQueueTest, ZeroLengthRequestAtTailSucceeds) {
  Push(kSegmentA);

  std::vector<base::span<const uint8_t>> segments;
  EXPECT_TRUE(queue_.GetSegmentedData(&segments, 0, 8));
  EXPECT_TRUE(segments.empty());
}

TEST_F(OffsetSegmentedByteQueueTest, ContiguousRunWithinOneSegment) {
  PushAllThree();

  base::span<const uint8_t> data = queue_.GetContiguousData(10);

  // The run stops at the end of the segment holding offset 10, not at the end
  // of the queue.
  ASSERT_EQ(6u, data.size());
  EXPECT_EQ(std::vector<uint8_t>({10, 11, 12, 13, 14, 15}),
            std::vector<uint8_t>(data.begin(), data.end()));

  // No copy was made: the span points into the pushed segment itself.
  EXPECT_EQ(&kSegmentB[2], data.data());
}

TEST_F(OffsetSegmentedByteQueueTest, ContiguousRunAtSegmentStart) {
  PushAllThree();

  base::span<const uint8_t> data = queue_.GetContiguousData(8);
  ASSERT_EQ(8u, data.size());
  EXPECT_EQ(&kSegmentB[0], data.data());
}

TEST_F(OffsetSegmentedByteQueueTest, ContiguousRunStopsAtSegmentBoundary) {
  PushAllThree();

  // A caller wanting the four bytes at offset 6 cannot be served directly:
  // they cross the A/B boundary at offset 8, so the run is short.
  base::span<const uint8_t> data = queue_.GetContiguousData(6);
  EXPECT_EQ(2u, data.size());

  // The short run above means "gather instead", not "need more data": the
  // range is readable in full through the other two accessors.
  std::vector<base::span<const uint8_t>> segments;
  ASSERT_TRUE(queue_.GetSegmentedData(&segments, 4, 6));
  EXPECT_EQ(2u, segments.size());
  EXPECT_EQ(std::vector<uint8_t>({6, 7, 8, 9}), Flatten(segments));
}

TEST_F(OffsetSegmentedByteQueueTest, ContiguousRunAtOrBeyondTailIsEmpty) {
  Push(kSegmentA);

  EXPECT_TRUE(queue_.GetContiguousData(8).empty());
  EXPECT_TRUE(queue_.GetContiguousData(100).empty());
}

TEST_F(OffsetSegmentedByteQueueTest, ContiguousRunBeforeHeadIsEmpty) {
  PushAllThree();
  queue_.Pop(8);

  ASSERT_EQ(8, queue_.head());
  EXPECT_TRUE(queue_.GetContiguousData(4).empty());

  // Offsets stay absolute after the pop.
  base::span<const uint8_t> data = queue_.GetContiguousData(8);
  ASSERT_EQ(8u, data.size());
  EXPECT_EQ(8, data[0]);
}

TEST_F(OffsetSegmentedByteQueueTest, ContiguousRunAfterPartialPop) {
  PushAllThree();
  queue_.Pop(10);

  // The front segment is trimmed in place, so offsets into it still resolve.
  base::span<const uint8_t> data = queue_.GetContiguousData(10);
  ASSERT_EQ(6u, data.size());
  EXPECT_EQ(&kSegmentB[2], data.data());
}

TEST_F(OffsetSegmentedByteQueueTest, PopAcrossSeveralSegmentsReleasesInOrder) {
  std::vector<int> released;
  auto tracker = [&released](int id) {
    return base::ScopedClosureRunner(base::BindOnce(
        [](std::vector<int>* r, int id) { r->push_back(id); }, &released, id));
  };
  queue_.Push(kSegmentA, tracker(0));
  queue_.Push(kSegmentB, tracker(1));
  queue_.Push(kSegmentC, tracker(2));

  queue_.Pop(19);
  EXPECT_EQ(std::vector<int>({0, 1}), released);
  EXPECT_EQ(19, queue_.head());
  EXPECT_EQ(24, queue_.tail());
  EXPECT_EQ(1u, queue_.GetSegmentCountForTesting());

  base::span<const uint8_t> data = queue_.GetContiguousData(19);
  EXPECT_EQ(&kSegmentC[3], data.data());
  EXPECT_EQ(5u, data.size());
}

TEST_F(OffsetSegmentedByteQueueTest, PopZeroIsANoOp) {
  PushAllThree();
  queue_.Pop(0);
  EXPECT_EQ(0, queue_.head());
  EXPECT_EQ(24, queue_.tail());
  EXPECT_EQ(3u, queue_.GetSegmentCountForTesting());
}

TEST_F(OffsetSegmentedByteQueueTest, TrimExactlyToHeadIsANoOp) {
  PushAllThree();
  queue_.Pop(4);

  EXPECT_TRUE(queue_.Trim(4));
  EXPECT_EQ(4, queue_.head());
  EXPECT_EQ(20u, queue_.size());
}

TEST_F(OffsetSegmentedByteQueueTest, TrimExactlyToTailReleasesEverything) {
  bool released = false;
  Push(kSegmentA);
  queue_.Push(kSegmentB, ReleaseFlagSetter(&released));

  EXPECT_TRUE(queue_.Trim(16)) << "the tail itself is in range";
  EXPECT_TRUE(released);
  EXPECT_EQ(16, queue_.head());
  EXPECT_EQ(16, queue_.tail());
  EXPECT_EQ(0u, queue_.GetSegmentCountForTesting());
}

TEST_F(OffsetSegmentedByteQueueTest, ZeroLengthRequestInMiddleSucceeds) {
  PushAllThree();
  queue_.Pop(4);

  std::vector<base::span<const uint8_t>> segments;
  EXPECT_TRUE(queue_.GetSegmentedData(&segments, 0, 12));
  EXPECT_TRUE(segments.empty());

  // Before the head is still rejected, even for zero bytes.
  EXPECT_FALSE(queue_.GetSegmentedData(&segments, 0, 2));
}

TEST_F(OffsetSegmentedByteQueueTest, PushAfterPartialPopContinuesOffsets) {
  Push(kSegmentA);
  queue_.Pop(6);
  Push(kSegmentB);

  EXPECT_EQ(6, queue_.head());
  EXPECT_EQ(16, queue_.tail());

  std::vector<base::span<const uint8_t>> segments;
  ASSERT_TRUE(queue_.GetSegmentedData(&segments, 10, 6));
  EXPECT_EQ(2u, segments.size());
  EXPECT_EQ(std::vector<uint8_t>({6, 7, 8, 9, 10, 11, 12, 13, 14, 15}),
            Flatten(segments));
}

TEST_F(OffsetSegmentedByteQueueTest, PushAfterResetRestartsAtZero) {
  PushAllThree();
  queue_.Pop(10);
  ASSERT_EQ(10, queue_.head());

  queue_.Reset();
  Push(kSegmentC);

  EXPECT_EQ(0, queue_.head());
  EXPECT_EQ(8, queue_.tail());
  EXPECT_EQ(kSegmentC.data(), queue_.GetContiguousData(0).data());
}

}  // namespace

}  // namespace media
