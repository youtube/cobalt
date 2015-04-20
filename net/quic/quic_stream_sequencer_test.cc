// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_stream_sequencer.h"

#include <utility>
#include <vector>

#include "base/rand_util.h"
#include "net/quic/reliable_quic_stream.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPiece;
using std::min;
using std::pair;
using std::vector;
using testing::_;
using testing::AnyNumber;
using testing::InSequence;
using testing::Return;
using testing::StrEq;

namespace net {

class QuicStreamSequencerPeer : public QuicStreamSequencer {
 public:
  explicit QuicStreamSequencerPeer(ReliableQuicStream* stream)
      : QuicStreamSequencer(stream) {
  }

  QuicStreamSequencerPeer(int32 max_mem, ReliableQuicStream* stream)
      : QuicStreamSequencer(max_mem, stream) {}

  virtual bool OnFrame(QuicStreamOffset byte_offset,
                       const char* data,
                       uint32 data_len) {
    QuicStreamFrame frame;
    frame.stream_id = 1;
    frame.offset = byte_offset;
    frame.data = StringPiece(data, data_len);
    return OnStreamFrame(frame);
  }

  void SetMemoryLimit(size_t limit) {
    max_frame_memory_ = limit;
  }

  const ReliableQuicStream* stream() const { return stream_; }
  uint64 num_bytes_consumed() const { return num_bytes_consumed_; }
  const FrameMap* frames() const { return &frames_; }
  int32 max_frame_memory() const { return max_frame_memory_; }
  QuicStreamOffset close_offset() const { return close_offset_; }
};

class MockStream : public ReliableQuicStream {
 public:
  MockStream(QuicSession* session, QuicStreamId id)
      : ReliableQuicStream(id, session) {
  }

  MOCK_METHOD1(TerminateFromPeer, void(bool half_close));
  MOCK_METHOD2(ProcessData, uint32(const char* data, uint32 data_len));
  MOCK_METHOD1(Close, void(QuicErrorCode error));
  MOCK_METHOD0(OnCanWrite, void());
};

namespace {

static const char kPayload[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

class QuicStreamSequencerTest : public ::testing::Test {
 protected:
  QuicStreamSequencerTest()
      : session_(NULL),
        stream_(session_,  1),
        sequencer_(new QuicStreamSequencerPeer(&stream_)) {
  }

  QuicSession* session_;
  testing::StrictMock<MockStream> stream_;
  scoped_ptr<QuicStreamSequencerPeer> sequencer_;
};

TEST_F(QuicStreamSequencerTest, RejectOldFrame) {
  EXPECT_CALL(stream_, ProcessData(StrEq("abc"), 3))
      .WillOnce(Return(3));

  EXPECT_TRUE(sequencer_->OnFrame(0, "abc", 3));
  EXPECT_EQ(0u, sequencer_->frames()->size());
  EXPECT_EQ(3u, sequencer_->num_bytes_consumed());
  // Nack this - it matches a past sequence number and we should not see it
  // again.
  EXPECT_FALSE(sequencer_->OnFrame(0, "def", 3));
  EXPECT_EQ(0u, sequencer_->frames()->size());
}

TEST_F(QuicStreamSequencerTest, RejectOverlyLargeFrame) {
  /*
  EXPECT_DFATAL(sequencer_.reset(new QuicStreamSequencerPeer(2, &stream_)),
                "Setting max frame memory to 2.  "
                "Some frames will be impossible to handle.");

  EXPECT_DEBUG_DEATH(sequencer_->OnFrame(0, "abc", 3), "");
  */
}

TEST_F(QuicStreamSequencerTest, DropFramePastBuffering) {
  sequencer_->SetMemoryLimit(3);

  EXPECT_FALSE(sequencer_->OnFrame(3, "abc", 3));
}

TEST_F(QuicStreamSequencerTest, RejectBufferedFrame) {
  EXPECT_CALL(stream_, ProcessData(StrEq("abc"), 3));

  EXPECT_TRUE(sequencer_->OnFrame(0, "abc", 3));
  EXPECT_EQ(1u, sequencer_->frames()->size());
  EXPECT_EQ(0u, sequencer_->num_bytes_consumed());
  // Ignore this - it matches a buffered frame.
  // Right now there's no checking that the payload is consistent.
  EXPECT_FALSE(sequencer_->OnFrame(0, "def", 3));
  EXPECT_EQ(1u, sequencer_->frames()->size());
}

TEST_F(QuicStreamSequencerTest, FullFrameConsumed) {
  EXPECT_CALL(stream_, ProcessData(StrEq("abc"), 3)).WillOnce(Return(3));

  EXPECT_TRUE(sequencer_->OnFrame(0, "abc", 3));
  EXPECT_EQ(0u, sequencer_->frames()->size());
  EXPECT_EQ(3u, sequencer_->num_bytes_consumed());
}

TEST_F(QuicStreamSequencerTest, PartialFrameConsumed) {
  EXPECT_CALL(stream_, ProcessData(StrEq("abc"), 3)).WillOnce(Return(2));

  EXPECT_TRUE(sequencer_->OnFrame(0, "abc", 3));
  EXPECT_EQ(1u, sequencer_->frames()->size());
  EXPECT_EQ(2u, sequencer_->num_bytes_consumed());
  EXPECT_EQ("c", sequencer_->frames()->find(2)->second);
}

TEST_F(QuicStreamSequencerTest, NextxFrameNotConsumed) {
  EXPECT_CALL(stream_, ProcessData(StrEq("abc"), 3)).WillOnce(Return(0));

  EXPECT_TRUE(sequencer_->OnFrame(0, "abc", 3));
  EXPECT_EQ(1u, sequencer_->frames()->size());
  EXPECT_EQ(0u, sequencer_->num_bytes_consumed());
  EXPECT_EQ("abc", sequencer_->frames()->find(0)->second);
}

TEST_F(QuicStreamSequencerTest, FutureFrameNotProcessed) {
  EXPECT_TRUE(sequencer_->OnFrame(3, "abc", 3));
  EXPECT_EQ(1u, sequencer_->frames()->size());
  EXPECT_EQ(0u, sequencer_->num_bytes_consumed());
  EXPECT_EQ("abc", sequencer_->frames()->find(3)->second);
}

TEST_F(QuicStreamSequencerTest, OutOfOrderFrameProcessed) {
  // Buffer the first
  EXPECT_TRUE(sequencer_->OnFrame(6, "ghi", 3));
  EXPECT_EQ(1u, sequencer_->frames()->size());
  EXPECT_EQ(0u, sequencer_->num_bytes_consumed());
  // Buffer the second
  EXPECT_TRUE(sequencer_->OnFrame(3, "def", 3));
  EXPECT_EQ(2u, sequencer_->frames()->size());
  EXPECT_EQ(0u, sequencer_->num_bytes_consumed());

  InSequence s;
  EXPECT_CALL(stream_, ProcessData(StrEq("abc"), 3)).WillOnce(Return(3));
  EXPECT_CALL(stream_, ProcessData(StrEq("def"), 3)).WillOnce(Return(3));
  EXPECT_CALL(stream_, ProcessData(StrEq("ghi"), 3)).WillOnce(Return(3));

  // Ack right away
  EXPECT_TRUE(sequencer_->OnFrame(0, "abc", 3));
  EXPECT_EQ(9u, sequencer_->num_bytes_consumed());

  EXPECT_EQ(0u, sequencer_->frames()->size());
}

TEST_F(QuicStreamSequencerTest, OutOfOrderFramesProcessedWithBuffering) {
  sequencer_->SetMemoryLimit(9);

  // Too far to buffer.
  EXPECT_FALSE(sequencer_->OnFrame(9, "jkl", 3));

  // We can afford to buffer this.
  EXPECT_TRUE(sequencer_->OnFrame(6, "ghi", 3));
  EXPECT_EQ(0u, sequencer_->num_bytes_consumed());

  InSequence s;
  EXPECT_CALL(stream_, ProcessData(StrEq("abc"), 3)).WillOnce(Return(3));

  // Ack right away
  EXPECT_TRUE(sequencer_->OnFrame(0, "abc", 3));
  EXPECT_EQ(3u, sequencer_->num_bytes_consumed());

  // We should be willing to buffer this now.
  EXPECT_TRUE(sequencer_->OnFrame(9, "jkl", 3));
  EXPECT_EQ(3u, sequencer_->num_bytes_consumed());

  EXPECT_CALL(stream_, ProcessData(StrEq("def"), 3)).WillOnce(Return(3));
  EXPECT_CALL(stream_, ProcessData(StrEq("ghi"), 3)).WillOnce(Return(3));
  EXPECT_CALL(stream_, ProcessData(StrEq("jkl"), 3)).WillOnce(Return(3));

  EXPECT_TRUE(sequencer_->OnFrame(3, "def", 3));
  EXPECT_EQ(12u, sequencer_->num_bytes_consumed());
  EXPECT_EQ(0u, sequencer_->frames()->size());
}

TEST_F(QuicStreamSequencerTest, BasicCloseOrdered) {
  InSequence s;
  EXPECT_CALL(stream_, ProcessData(StrEq("abc"), 3)).WillOnce(Return(3));
  EXPECT_TRUE(sequencer_->OnFrame(0, "abc", 3));

  EXPECT_CALL(stream_, TerminateFromPeer(false));
  sequencer_->CloseStreamAtOffset(3, false);
  EXPECT_EQ(3u, sequencer_->close_offset());
}

TEST_F(QuicStreamSequencerTest, BasicHalfOrdered) {
  InSequence s;

  EXPECT_CALL(stream_, ProcessData(StrEq("abc"), 3)).WillOnce(Return(3));
  EXPECT_TRUE(sequencer_->OnFrame(0, "abc", 3));

  EXPECT_CALL(stream_, TerminateFromPeer(true));
  sequencer_->CloseStreamAtOffset(3, true);
  EXPECT_EQ(3u, sequencer_->close_offset());
}

TEST_F(QuicStreamSequencerTest, BasicCloseUnordered) {
  sequencer_->CloseStreamAtOffset(3, false);
  EXPECT_EQ(3u, sequencer_->close_offset());

  InSequence s;
  EXPECT_CALL(stream_, ProcessData(StrEq("abc"), 3)).WillOnce(Return(3));
  EXPECT_CALL(stream_, TerminateFromPeer(false));

  EXPECT_TRUE(sequencer_->OnFrame(0, "abc", 3));
}

TEST_F(QuicStreamSequencerTest, BasicHalfUnorderedWithFlush) {
  sequencer_->CloseStreamAtOffset(6, true);
  EXPECT_EQ(6u, sequencer_->close_offset());
  InSequence s;
  EXPECT_CALL(stream_, ProcessData(StrEq("abc"), 3)).WillOnce(Return(3));
  EXPECT_CALL(stream_, ProcessData(StrEq("def"), 3)).WillOnce(Return(3));
  EXPECT_CALL(stream_, TerminateFromPeer(true));

  EXPECT_TRUE(sequencer_->OnFrame(3, "def", 3));
  EXPECT_TRUE(sequencer_->OnFrame(0, "abc", 3));
}

TEST_F(QuicStreamSequencerTest, BasicCloseUnorderedWithFlush) {
  sequencer_->CloseStreamAtOffset(6, false);
  EXPECT_EQ(6u, sequencer_->close_offset());

  InSequence s;
  EXPECT_CALL(stream_, ProcessData(StrEq("abc"), 3)).WillOnce(Return(3));
  EXPECT_CALL(stream_, ProcessData(StrEq("def"), 3)).WillOnce(Return(3));
  EXPECT_CALL(stream_, TerminateFromPeer(false));

  EXPECT_TRUE(sequencer_->OnFrame(3, "def", 3));
  EXPECT_TRUE(sequencer_->OnFrame(0, "abc", 3));
}

TEST_F(QuicStreamSequencerTest, BasicHalfUnordered) {
  sequencer_->CloseStreamAtOffset(3, true);
  EXPECT_EQ(3u, sequencer_->close_offset());
  InSequence s;
  EXPECT_CALL(stream_, ProcessData(StrEq("abc"), 3)).WillOnce(Return(3));
  EXPECT_CALL(stream_, TerminateFromPeer(true));

  EXPECT_TRUE(sequencer_->OnFrame(0, "abc", 3));
}

TEST_F(QuicStreamSequencerTest, TerminateStreamBeforeCloseEqual) {
  sequencer_->CloseStreamAtOffset(3, true);
  EXPECT_EQ(3u, sequencer_->close_offset());

  sequencer_->CloseStreamAtOffset(3, false);
  EXPECT_EQ(3u, sequencer_->close_offset());

  InSequence s;
  EXPECT_CALL(stream_, ProcessData(StrEq("abc"), 3)).WillOnce(Return(3));
  EXPECT_CALL(stream_, TerminateFromPeer(false));
  EXPECT_TRUE(sequencer_->OnFrame(0, "abc", 3));
}

TEST_F(QuicStreamSequencerTest, CloseBeforeTermianteEqual) {
  sequencer_->CloseStreamAtOffset(3, false);
  EXPECT_EQ(3u, sequencer_->close_offset());

  sequencer_->CloseStreamAtOffset(3, true);
  EXPECT_EQ(3u, sequencer_->close_offset());

  InSequence s;
  EXPECT_CALL(stream_, ProcessData(StrEq("abc"), 3)).WillOnce(Return(3));
  EXPECT_CALL(stream_, TerminateFromPeer(false));
  EXPECT_TRUE(sequencer_->OnFrame(0, "abc", 3));
}

TEST_F(QuicStreamSequencerTest, MutipleOffsets) {
  sequencer_->CloseStreamAtOffset(3, false);
  EXPECT_EQ(3u, sequencer_->close_offset());

  EXPECT_CALL(stream_, Close(QUIC_MULTIPLE_TERMINATION_OFFSETS));
  sequencer_->CloseStreamAtOffset(5, false);
  EXPECT_EQ(3u, sequencer_->close_offset());

  EXPECT_CALL(stream_, Close(QUIC_MULTIPLE_TERMINATION_OFFSETS));
  sequencer_->CloseStreamAtOffset(1, false);
  EXPECT_EQ(3u, sequencer_->close_offset());

  sequencer_->CloseStreamAtOffset(3, false);
  EXPECT_EQ(3u, sequencer_->close_offset());
}

class QuicSequencerRandomTest : public QuicStreamSequencerTest {
 public:
  typedef pair<int, string> Frame;
  typedef vector<Frame> FrameList;

  void CreateFrames() {
    int payload_size = arraysize(kPayload) - 1;
    int remaining_payload = payload_size;
    while (remaining_payload != 0) {
      int size = min(OneToN(6), remaining_payload);
      int index = payload_size - remaining_payload;
      list_.push_back(make_pair(index, string(kPayload + index, size)));
      remaining_payload -= size;
    }
  }

  QuicSequencerRandomTest() {
    CreateFrames();
  }

  int OneToN(int n) {
    return base::RandInt(1, n);
  }

  int MaybeProcessMaybeBuffer(const char* data, uint32 len) {
    int to_process = len;
    if (base::RandUint64() % 2 != 0) {
      to_process = base::RandInt(0, len);
    }
    output_.append(data, to_process);
    return to_process;
  }

  string output_;
  FrameList list_;
};

// All frames are processed as soon as we have sequential data.
// Infinite buffering, so all frames are acked right away.
TEST_F(QuicSequencerRandomTest, RandomFramesNoDroppingNoBackup) {
  InSequence s;
  for (size_t i = 0; i < list_.size(); ++i) {
    string* data = &list_[i].second;
    EXPECT_CALL(stream_, ProcessData(StrEq(*data), data->size()))
        .WillOnce(Return(data->size()));
  }

  while (list_.size() != 0) {
    int index = OneToN(list_.size()) - 1;
    LOG(ERROR) << "Sending index " << index << " "
               << list_[index].second.data();
    EXPECT_TRUE(sequencer_->OnFrame(
        list_[index].first, list_[index].second.data(),
        list_[index].second.size()));
    list_.erase(list_.begin() + index);
  }
}

// All frames are processed as soon as we have sequential data.
// Buffering, so some frames are rejected.
TEST_F(QuicSequencerRandomTest, RandomFramesDroppingNoBackup) {
  sequencer_->SetMemoryLimit(26);

  InSequence s;
  for (size_t i = 0; i < list_.size(); ++i) {
    string* data = &list_[i].second;
    EXPECT_CALL(stream_, ProcessData(StrEq(*data), data->size()))
        .WillOnce(Return(data->size()));
  }

  while (list_.size() != 0) {
    int index = OneToN(list_.size()) - 1;
    LOG(ERROR) << "Sending index " << index << " "
               << list_[index].second.data();
    bool acked = sequencer_->OnFrame(
        list_[index].first, list_[index].second.data(),
        list_[index].second.size());
    if (acked) {
      list_.erase(list_.begin() + index);
    }
  }
}

}  // namespace

}  // namespace net
