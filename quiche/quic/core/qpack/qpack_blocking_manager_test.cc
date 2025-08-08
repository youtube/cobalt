// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "quiche/quic/core/qpack/qpack_blocking_manager.h"

#include <limits>

#include "quiche/quic/platform/api/quic_test.h"

namespace quic {
namespace test {

class QpackBlockingManagerPeer {
 public:
  static bool stream_is_blocked(const QpackBlockingManager* manager,
                                QuicStreamId stream_id) {
    for (const auto& header_blocks_for_stream : manager->header_blocks_) {
      if (header_blocks_for_stream.first != stream_id) {
        continue;
      }
      for (const auto& indices : header_blocks_for_stream.second) {
        if (QpackBlockingManager::RequiredInsertCount(indices) >
            manager->known_received_count_) {
          return true;
        }
      }
    }

    return false;
  }
};

namespace {

class QpackBlockingManagerTest : public QuicTest {
 protected:
  QpackBlockingManagerTest() = default;
  ~QpackBlockingManagerTest() override = default;

  bool stream_is_blocked(QuicStreamId stream_id) const {
    return QpackBlockingManagerPeer::stream_is_blocked(&manager_, stream_id);
  }

  QpackBlockingManager manager_;
};

TEST_F(QpackBlockingManagerTest, Empty) {
  EXPECT_EQ(0u, manager_.known_received_count());
  EXPECT_EQ(std::numeric_limits<uint64_t>::max(),
            manager_.smallest_blocking_index());

  EXPECT_FALSE(manager_.OnHeaderAcknowledgement(0));
  EXPECT_FALSE(manager_.OnHeaderAcknowledgement(1));
}

TEST_F(QpackBlockingManagerTest, NotBlockedByInsertCountIncrement) {
  EXPECT_TRUE(manager_.OnInsertCountIncrement(2));

  // Stream 0 is not blocked, because it only references entries that are
  // already acknowledged by an Insert Count Increment instruction.
  manager_.OnHeaderBlockSent(0, {1, 0});
  EXPECT_FALSE(stream_is_blocked(0));
}

TEST_F(QpackBlockingManagerTest, UnblockedByInsertCountIncrement) {
  manager_.OnHeaderBlockSent(0, {1, 0});
  EXPECT_TRUE(stream_is_blocked(0));

  EXPECT_TRUE(manager_.OnInsertCountIncrement(2));
  EXPECT_FALSE(stream_is_blocked(0));
}

TEST_F(QpackBlockingManagerTest, NotBlockedByHeaderAcknowledgement) {
  manager_.OnHeaderBlockSent(0, {2, 1, 1});
  EXPECT_TRUE(stream_is_blocked(0));

  EXPECT_TRUE(manager_.OnHeaderAcknowledgement(0));
  EXPECT_FALSE(stream_is_blocked(0));

  // Stream 1 is not blocked, because it only references entries that are
  // already acknowledged by a Header Acknowledgement instruction.
  manager_.OnHeaderBlockSent(1, {2, 2});
  EXPECT_FALSE(stream_is_blocked(1));
}

TEST_F(QpackBlockingManagerTest, UnblockedByHeaderAcknowledgement) {
  manager_.OnHeaderBlockSent(0, {2, 1, 1});
  manager_.OnHeaderBlockSent(1, {2, 2});
  EXPECT_TRUE(stream_is_blocked(0));
  EXPECT_TRUE(stream_is_blocked(1));

  EXPECT_TRUE(manager_.OnHeaderAcknowledgement(0));
  EXPECT_FALSE(stream_is_blocked(0));
  EXPECT_FALSE(stream_is_blocked(1));
}

TEST_F(QpackBlockingManagerTest, KnownReceivedCount) {
  EXPECT_EQ(0u, manager_.known_received_count());

  // Sending a header block does not change Known Received Count.
  manager_.OnHeaderBlockSent(0, {0});
  EXPECT_EQ(0u, manager_.known_received_count());

  manager_.OnHeaderBlockSent(1, {1});
  EXPECT_EQ(0u, manager_.known_received_count());

  // Header Acknowledgement might increase Known Received Count.
  EXPECT_TRUE(manager_.OnHeaderAcknowledgement(0));
  EXPECT_EQ(1u, manager_.known_received_count());

  manager_.OnHeaderBlockSent(2, {5});
  EXPECT_EQ(1u, manager_.known_received_count());

  EXPECT_TRUE(manager_.OnHeaderAcknowledgement(1));
  EXPECT_EQ(2u, manager_.known_received_count());

  // Insert Count Increment increases Known Received Count.
  EXPECT_TRUE(manager_.OnInsertCountIncrement(2));
  EXPECT_EQ(4u, manager_.known_received_count());

  EXPECT_TRUE(manager_.OnHeaderAcknowledgement(2));
  EXPECT_EQ(6u, manager_.known_received_count());

  // Stream Cancellation does not change Known Received Count.
  manager_.OnStreamCancellation(0);
  EXPECT_EQ(6u, manager_.known_received_count());

  // Header Acknowledgement of a block with smaller Required Insert Count does
  // not increase Known Received Count.
  manager_.OnHeaderBlockSent(0, {3});
  EXPECT_EQ(6u, manager_.known_received_count());

  EXPECT_TRUE(manager_.OnHeaderAcknowledgement(0));
  EXPECT_EQ(6u, manager_.known_received_count());

  // Header Acknowledgement of a block with equal Required Insert Count does not
  // increase Known Received Count.
  manager_.OnHeaderBlockSent(1, {5});
  EXPECT_EQ(6u, manager_.known_received_count());

  EXPECT_TRUE(manager_.OnHeaderAcknowledgement(1));
  EXPECT_EQ(6u, manager_.known_received_count());
}

TEST_F(QpackBlockingManagerTest, SmallestBlockingIndex) {
  EXPECT_EQ(std::numeric_limits<uint64_t>::max(),
            manager_.smallest_blocking_index());

  manager_.OnHeaderBlockSent(0, {0});
  EXPECT_EQ(0u, manager_.smallest_blocking_index());

  manager_.OnHeaderBlockSent(1, {2});
  EXPECT_EQ(0u, manager_.smallest_blocking_index());

  EXPECT_TRUE(manager_.OnHeaderAcknowledgement(0));
  EXPECT_EQ(2u, manager_.smallest_blocking_index());

  manager_.OnHeaderBlockSent(1, {1});
  EXPECT_EQ(1u, manager_.smallest_blocking_index());

  EXPECT_TRUE(manager_.OnHeaderAcknowledgement(1));
  EXPECT_EQ(1u, manager_.smallest_blocking_index());

  // Insert Count Increment does not change smallest blocking index.
  EXPECT_TRUE(manager_.OnInsertCountIncrement(2));
  EXPECT_EQ(1u, manager_.smallest_blocking_index());

  manager_.OnStreamCancellation(1);
  EXPECT_EQ(std::numeric_limits<uint64_t>::max(),
            manager_.smallest_blocking_index());
}

TEST_F(QpackBlockingManagerTest, HeaderAcknowledgementsOnSingleStream) {
  EXPECT_EQ(0u, manager_.known_received_count());
  EXPECT_EQ(std::numeric_limits<uint64_t>::max(),
            manager_.smallest_blocking_index());

  manager_.OnHeaderBlockSent(0, {2, 1, 1});
  EXPECT_EQ(0u, manager_.known_received_count());
  EXPECT_TRUE(stream_is_blocked(0));
  EXPECT_EQ(1u, manager_.smallest_blocking_index());

  manager_.OnHeaderBlockSent(0, {1, 0});
  EXPECT_EQ(0u, manager_.known_received_count());
  EXPECT_TRUE(stream_is_blocked(0));
  EXPECT_EQ(0u, manager_.smallest_blocking_index());

  EXPECT_TRUE(manager_.OnHeaderAcknowledgement(0));
  EXPECT_EQ(3u, manager_.known_received_count());
  EXPECT_FALSE(stream_is_blocked(0));
  EXPECT_EQ(0u, manager_.smallest_blocking_index());

  manager_.OnHeaderBlockSent(0, {3});
  EXPECT_EQ(3u, manager_.known_received_count());
  EXPECT_TRUE(stream_is_blocked(0));
  EXPECT_EQ(0u, manager_.smallest_blocking_index());

  EXPECT_TRUE(manager_.OnHeaderAcknowledgement(0));
  EXPECT_EQ(3u, manager_.known_received_count());
  EXPECT_TRUE(stream_is_blocked(0));
  EXPECT_EQ(3u, manager_.smallest_blocking_index());

  EXPECT_TRUE(manager_.OnHeaderAcknowledgement(0));
  EXPECT_EQ(4u, manager_.known_received_count());
  EXPECT_FALSE(stream_is_blocked(0));
  EXPECT_EQ(std::numeric_limits<uint64_t>::max(),
            manager_.smallest_blocking_index());

  EXPECT_FALSE(manager_.OnHeaderAcknowledgement(0));
}

TEST_F(QpackBlockingManagerTest, CancelStream) {
  manager_.OnHeaderBlockSent(0, {3});
  EXPECT_TRUE(stream_is_blocked(0));
  EXPECT_EQ(3u, manager_.smallest_blocking_index());

  manager_.OnHeaderBlockSent(0, {2});
  EXPECT_TRUE(stream_is_blocked(0));
  EXPECT_EQ(2u, manager_.smallest_blocking_index());

  manager_.OnHeaderBlockSent(1, {4});
  EXPECT_TRUE(stream_is_blocked(0));
  EXPECT_TRUE(stream_is_blocked(1));
  EXPECT_EQ(2u, manager_.smallest_blocking_index());

  manager_.OnStreamCancellation(0);
  EXPECT_FALSE(stream_is_blocked(0));
  EXPECT_TRUE(stream_is_blocked(1));
  EXPECT_EQ(4u, manager_.smallest_blocking_index());

  manager_.OnStreamCancellation(1);
  EXPECT_FALSE(stream_is_blocked(0));
  EXPECT_FALSE(stream_is_blocked(1));
  EXPECT_EQ(std::numeric_limits<uint64_t>::max(),
            manager_.smallest_blocking_index());
}

TEST_F(QpackBlockingManagerTest, BlockingAllowedOnStream) {
  const QuicStreamId kStreamId1 = 1;
  const QuicStreamId kStreamId2 = 2;
  const QuicStreamId kStreamId3 = 3;

  // No stream can block if limit is 0.
  EXPECT_FALSE(manager_.blocking_allowed_on_stream(kStreamId1, 0));
  EXPECT_FALSE(manager_.blocking_allowed_on_stream(kStreamId2, 0));

  // Either stream can block if limit is larger.
  EXPECT_TRUE(manager_.blocking_allowed_on_stream(kStreamId1, 1));
  EXPECT_TRUE(manager_.blocking_allowed_on_stream(kStreamId2, 1));

  // Doubly block first stream.
  manager_.OnHeaderBlockSent(kStreamId1, {0});
  manager_.OnHeaderBlockSent(kStreamId1, {1});

  // First stream is already blocked so it can carry more blocking references.
  EXPECT_TRUE(manager_.blocking_allowed_on_stream(kStreamId1, 1));
  // Second stream is not allowed to block if limit is already reached.
  EXPECT_FALSE(manager_.blocking_allowed_on_stream(kStreamId2, 1));

  // Either stream can block if limit is larger than number of blocked streams.
  EXPECT_TRUE(manager_.blocking_allowed_on_stream(kStreamId1, 2));
  EXPECT_TRUE(manager_.blocking_allowed_on_stream(kStreamId2, 2));

  // Block second stream.
  manager_.OnHeaderBlockSent(kStreamId2, {2});

  // Streams are already blocked so either can carry more blocking references.
  EXPECT_TRUE(manager_.blocking_allowed_on_stream(kStreamId1, 2));
  EXPECT_TRUE(manager_.blocking_allowed_on_stream(kStreamId2, 2));

  // Third, unblocked stream is not allowed to block unless limit is strictly
  // larger than number of blocked streams.
  EXPECT_FALSE(manager_.blocking_allowed_on_stream(kStreamId3, 2));
  EXPECT_TRUE(manager_.blocking_allowed_on_stream(kStreamId3, 3));

  // Acknowledge decoding of first header block on first stream.
  // Stream is still blocked on its second header block.
  manager_.OnHeaderAcknowledgement(kStreamId1);

  EXPECT_TRUE(manager_.blocking_allowed_on_stream(kStreamId1, 2));
  EXPECT_TRUE(manager_.blocking_allowed_on_stream(kStreamId2, 2));

  // Acknowledge decoding of second header block on first stream.
  // This unblocks the stream.
  manager_.OnHeaderAcknowledgement(kStreamId1);

  // First stream is not allowed to block if limit is already reached.
  EXPECT_FALSE(manager_.blocking_allowed_on_stream(kStreamId1, 1));
  // Second stream is already blocked so it can carry more blocking references.
  EXPECT_TRUE(manager_.blocking_allowed_on_stream(kStreamId2, 1));

  // Either stream can block if limit is larger than number of blocked streams.
  EXPECT_TRUE(manager_.blocking_allowed_on_stream(kStreamId1, 2));
  EXPECT_TRUE(manager_.blocking_allowed_on_stream(kStreamId2, 2));

  // Unblock second stream.
  manager_.OnHeaderAcknowledgement(kStreamId2);

  // No stream can block if limit is 0.
  EXPECT_FALSE(manager_.blocking_allowed_on_stream(kStreamId1, 0));
  EXPECT_FALSE(manager_.blocking_allowed_on_stream(kStreamId2, 0));

  // Either stream can block if limit is larger.
  EXPECT_TRUE(manager_.blocking_allowed_on_stream(kStreamId1, 1));
  EXPECT_TRUE(manager_.blocking_allowed_on_stream(kStreamId2, 1));
}

TEST_F(QpackBlockingManagerTest, InsertCountIncrementOverflow) {
  EXPECT_TRUE(manager_.OnInsertCountIncrement(10));
  EXPECT_EQ(10u, manager_.known_received_count());

  EXPECT_FALSE(manager_.OnInsertCountIncrement(
      std::numeric_limits<uint64_t>::max() - 5));
}

}  // namespace
}  // namespace test
}  // namespace quic
