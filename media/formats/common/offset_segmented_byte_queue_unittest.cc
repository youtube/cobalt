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
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

// Three distinct 8-byte appends. Pushed in this order, every byte's value is
// its absolute offset.
constexpr std::array<uint8_t, 8> kSegmentA = {0, 1, 2, 3, 4, 5, 6, 7};
constexpr std::array<uint8_t, 8> kSegmentB = {8, 9, 10, 11, 12, 13, 14, 15};
constexpr std::array<uint8_t, 8> kSegmentC = {16, 17, 18, 19, 20, 21, 22, 23};

constexpr int64_t kMaxInt64 = std::numeric_limits<int64_t>::max();

// Passed as `max_offset` by tests that are not about it, so that it never
// limits the read.
constexpr int64_t kUnbounded = kMaxInt64;

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

// The bytes `first`..`last`, inclusive.
std::vector<uint8_t> Range(uint8_t first, uint8_t last) {
  std::vector<uint8_t> bytes;
  for (uint8_t i = first; i <= last; ++i) {
    bytes.push_back(i);
  }
  return bytes;
}

// The parameter is `borrow_mode`, so every test runs against both
// SegmentedByteQueue implementations. How each implementation stores and
// releases data is covered by segmented_byte_queue_unittest.cc; these tests
// cover the absolute offsets and the `max_offset` bound added on top.
class OffsetSegmentedByteQueueTest : public ::testing::TestWithParam<bool> {
 protected:
  OffsetSegmentedByteQueueTest()
      : queue_(OffsetSegmentedByteQueue::Create(GetParam())) {}

  void Push(base::span<const uint8_t> data) {
    EXPECT_TRUE(queue_->Push(data, base::ScopedClosureRunner()));
  }

  // Pushes the three segments, giving the bytes 0..23 at offsets 0..23.
  void PushAllThree() {
    Push(kSegmentA);
    Push(kSegmentB);
    Push(kSegmentC);
  }

  const std::unique_ptr<OffsetSegmentedByteQueue> queue_;
};

TEST_P(OffsetSegmentedByteQueueTest, CreateHonorsBorrowMode) {
  Push(kSegmentA);
  Push(kSegmentB);

  base::span<const uint8_t> run = queue_->PeekContiguousData(0, kUnbounded);
  if (GetParam()) {
    // Borrowed: the run is the first append itself.
    EXPECT_EQ(kSegmentA.data(), run.data());
    EXPECT_EQ(kSegmentA.size(), run.size());
  } else {
    // Owned: both appends were copied into one buffer.
    EXPECT_NE(kSegmentA.data(), run.data());
    EXPECT_EQ(Range(0, 15), ToVector(run));
  }
}

TEST_P(OffsetSegmentedByteQueueTest, StartsEmpty) {
  EXPECT_EQ(0, queue_->head());
  EXPECT_EQ(0, queue_->tail());
  EXPECT_EQ(0u, queue_->size());

  EXPECT_TRUE(queue_->PeekContiguousData(0, kUnbounded).empty());
  EXPECT_FALSE(queue_->PeekSegmentedData(0, 1, kUnbounded));
  EXPECT_FALSE(queue_->PeekLinearizedData(0, 1, kUnbounded));
}

TEST_P(OffsetSegmentedByteQueueTest, PushAdvancesTailNotHead) {
  PushAllThree();
  EXPECT_EQ(0, queue_->head());
  EXPECT_EQ(24, queue_->tail());
  EXPECT_EQ(24u, queue_->size());
}

TEST_P(OffsetSegmentedByteQueueTest, PopAdvancesHeadAndKeepsOffsetsStable) {
  PushAllThree();

  queue_->Pop(10);
  EXPECT_EQ(10, queue_->head());
  EXPECT_EQ(24, queue_->tail());
  EXPECT_EQ(14u, queue_->size());

  // Absolute offsets still address the same bytes after popping.
  auto segments = queue_->PeekSegmentedData(10, 6, kUnbounded);
  ASSERT_TRUE(segments);
  EXPECT_EQ(Range(10, 15), Flatten(*segments));
  auto linearized = queue_->PeekLinearizedData(10, 6, kUnbounded);
  ASSERT_TRUE(linearized);
  EXPECT_EQ(Range(10, 15), ToVector(*linearized));

  base::span<const uint8_t> run = queue_->PeekContiguousData(10, kUnbounded);
  ASSERT_FALSE(run.empty());
  EXPECT_EQ(10, run[0]);
}

TEST_P(OffsetSegmentedByteQueueTest, ReadsAcrossAppendsUseAbsoluteOffsets) {
  PushAllThree();
  queue_->Pop(4);

  // Bytes 6..9 cross the boundary between the first two appends.
  auto segments = queue_->PeekSegmentedData(6, 4, kUnbounded);
  ASSERT_TRUE(segments);
  EXPECT_EQ(Range(6, 9), Flatten(*segments));
  auto linearized = queue_->PeekLinearizedData(6, 4, kUnbounded);
  ASSERT_TRUE(linearized);
  EXPECT_EQ(Range(6, 9), ToVector(*linearized));
}

TEST_P(OffsetSegmentedByteQueueTest, ReadsBeforeHeadFail) {
  PushAllThree();
  queue_->Pop(8);

  // Fails even when the range would reach past the head.
  EXPECT_TRUE(queue_->PeekContiguousData(4, kUnbounded).empty());
  EXPECT_FALSE(queue_->PeekSegmentedData(4, 8, kUnbounded));
  EXPECT_FALSE(queue_->PeekLinearizedData(4, 8, kUnbounded));
}

TEST_P(OffsetSegmentedByteQueueTest, ReadsPastTailFail) {
  PushAllThree();

  EXPECT_TRUE(queue_->PeekContiguousData(24, kUnbounded).empty());
  EXPECT_FALSE(queue_->PeekSegmentedData(20, 8, kUnbounded));
  EXPECT_FALSE(queue_->PeekSegmentedData(24, 1, kUnbounded));
  EXPECT_FALSE(queue_->PeekLinearizedData(20, 8, kUnbounded));
  EXPECT_FALSE(queue_->PeekLinearizedData(24, 1, kUnbounded));

  // So do offsets far past the tail, such as a malformed stream may supply.
  EXPECT_TRUE(queue_->PeekContiguousData(kMaxInt64, kUnbounded).empty());
  EXPECT_FALSE(queue_->PeekSegmentedData(kMaxInt64, 1, kUnbounded));
  EXPECT_FALSE(queue_->PeekLinearizedData(kMaxInt64, 1, kUnbounded));
}

TEST_P(OffsetSegmentedByteQueueTest, ZeroLengthReadsResolveFromHeadToTail) {
  PushAllThree();
  queue_->Pop(4);

  // A zero-length read succeeds anywhere from the head to the tail, inclusive,
  // and returns nothing.
  for (int64_t offset : {4, 12, 24}) {
    SCOPED_TRACE(offset);
    auto segments = queue_->PeekSegmentedData(offset, 0, kUnbounded);
    ASSERT_TRUE(segments);
    EXPECT_TRUE(segments->empty());
    auto linearized = queue_->PeekLinearizedData(offset, 0, kUnbounded);
    ASSERT_TRUE(linearized);
    EXPECT_TRUE(linearized->empty());
  }

  // Elsewhere it fails like any other read.
  EXPECT_FALSE(queue_->PeekSegmentedData(3, 0, kUnbounded));
  EXPECT_FALSE(queue_->PeekSegmentedData(25, 0, kUnbounded));
  EXPECT_FALSE(queue_->PeekLinearizedData(3, 0, kUnbounded));
  EXPECT_FALSE(queue_->PeekLinearizedData(25, 0, kUnbounded));
}

TEST_P(OffsetSegmentedByteQueueTest, TrimPopsUpToMaxOffset) {
  PushAllThree();

  // At or before the head there is nothing to pop.
  queue_->Trim(-1);
  queue_->Trim(0);
  EXPECT_EQ(0, queue_->head());

  queue_->Trim(10);
  EXPECT_EQ(10, queue_->head());
  EXPECT_EQ(14u, queue_->size());
  auto linearized = queue_->PeekLinearizedData(10, 14, kUnbounded);
  ASSERT_TRUE(linearized);
  EXPECT_EQ(Range(10, 23), ToVector(*linearized));

  queue_->Trim(10);
  EXPECT_EQ(10, queue_->head());

  // The tail itself is in range.
  queue_->Trim(24);
  EXPECT_EQ(24, queue_->head());
  EXPECT_EQ(24, queue_->tail());
}

TEST_P(OffsetSegmentedByteQueueTest, TrimPastTailPopsEverything) {
  PushAllThree();
  queue_->Pop(4);

  queue_->Trim(100);
  EXPECT_EQ(24, queue_->head());
  EXPECT_EQ(24, queue_->tail());
  EXPECT_EQ(0u, queue_->size());
}

TEST_P(OffsetSegmentedByteQueueTest, PushAfterPartialPopContinuesOffsets) {
  Push(kSegmentA);
  queue_->Pop(6);
  Push(kSegmentB);

  EXPECT_EQ(6, queue_->head());
  EXPECT_EQ(16, queue_->tail());
  auto linearized = queue_->PeekLinearizedData(6, 10, kUnbounded);
  ASSERT_TRUE(linearized);
  EXPECT_EQ(Range(6, 15), ToVector(*linearized));
}

TEST_P(OffsetSegmentedByteQueueTest, ResetRestartsOffsetsAtZero) {
  PushAllThree();
  queue_->Pop(10);
  ASSERT_EQ(10, queue_->head());

  queue_->Reset();
  EXPECT_EQ(0, queue_->head());
  EXPECT_EQ(0, queue_->tail());

  Push(kSegmentC);
  EXPECT_EQ(8, queue_->tail());
  EXPECT_EQ(ToVector(kSegmentC),
            ToVector(queue_->PeekContiguousData(0, kUnbounded)));
}

TEST_P(OffsetSegmentedByteQueueTest, PeekContiguousDataStopsAtMaxOffset) {
  PushAllThree();

  // The run is cut at `max_offset`, wherever its segment ends.
  EXPECT_EQ(Range(2, 4), ToVector(queue_->PeekContiguousData(2, 5)));

  // A `max_offset` past the end of the run leaves it alone. Borrowed, the run
  // ends with the first append; owned, everything is one run.
  EXPECT_EQ(GetParam() ? Range(2, 7) : Range(2, 23),
            ToVector(queue_->PeekContiguousData(2, 100)));

  // Nothing is returned at or past `max_offset`, nor before the head.
  EXPECT_TRUE(queue_->PeekContiguousData(5, 5).empty());
  EXPECT_TRUE(queue_->PeekContiguousData(6, 5).empty());
  queue_->Pop(4);
  EXPECT_TRUE(queue_->PeekContiguousData(2, 5).empty());
}

TEST_P(OffsetSegmentedByteQueueTest, PeekSegmentedDataStopsAtMaxOffset) {
  PushAllThree();

  // A range ending exactly at `max_offset` is readable, even across appends.
  auto segments = queue_->PeekSegmentedData(6, 4, 10);
  ASSERT_TRUE(segments);
  EXPECT_EQ(Range(6, 9), Flatten(*segments));

  // A range ending past it is not, even though it is buffered.
  EXPECT_FALSE(queue_->PeekSegmentedData(6, 4, 9));
  EXPECT_FALSE(queue_->PeekSegmentedData(6, 1, 6));
  EXPECT_TRUE(queue_->PeekSegmentedData(6, 0, 6));

  // `max_offset` never makes unbuffered data readable.
  EXPECT_FALSE(queue_->PeekSegmentedData(20, 8, 100));
  EXPECT_FALSE(queue_->PeekSegmentedData(-1, 1, 10));
  EXPECT_FALSE(queue_->PeekSegmentedData(kMaxInt64, 1, kMaxInt64));
}

TEST_P(OffsetSegmentedByteQueueTest, PeekLinearizedDataStopsAtMaxOffset) {
  PushAllThree();

  // A range ending exactly at `max_offset` is readable, even across appends.
  auto linearized = queue_->PeekLinearizedData(6, 4, 10);
  ASSERT_TRUE(linearized);
  EXPECT_EQ(Range(6, 9), ToVector(*linearized));

  // A range ending past it is not, even though it is buffered.
  EXPECT_FALSE(queue_->PeekLinearizedData(6, 4, 9));
  EXPECT_FALSE(queue_->PeekLinearizedData(6, 1, 6));
  EXPECT_TRUE(queue_->PeekLinearizedData(6, 0, 6));

  // `max_offset` never makes unbuffered data readable.
  EXPECT_FALSE(queue_->PeekLinearizedData(20, 8, 100));
  EXPECT_FALSE(queue_->PeekLinearizedData(-1, 1, 10));
  EXPECT_FALSE(queue_->PeekLinearizedData(kMaxInt64, 1, kMaxInt64));
}

INSTANTIATE_TEST_SUITE_P(All,
                         OffsetSegmentedByteQueueTest,
                         ::testing::Bool(),
                         [](const ::testing::TestParamInfo<bool>& info) {
                           return std::string(info.param ? "Borrowed"
                                                         : "Owned");
                         });

}  // namespace

}  // namespace media
