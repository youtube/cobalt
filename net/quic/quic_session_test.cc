// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_session.h"
#include "net/quic/quic_connection.h"

#include <set>

#include "base/hash_tables.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::hash_map;
using std::set;
using testing::_;
using testing::InSequence;

namespace net {
namespace test {
namespace {

class TestCryptoStream : public QuicCryptoStream {
 public:
  explicit TestCryptoStream(QuicSession* session)
      : QuicCryptoStream(session) {
  }

  void OnHandshakeMessage(const CryptoHandshakeMessage& message) {
    SetHandshakeComplete(QUIC_NO_ERROR);
  }
};

class TestStream : public ReliableQuicStream {
 public:
  TestStream(QuicStreamId id, QuicSession* session)
      : ReliableQuicStream(id, session) {
  }

  virtual uint32 ProcessData(const char* data, uint32 data_len) {
    return data_len;
  }

  MOCK_METHOD0(OnCanWrite, void());
};

class TestSession : public QuicSession {
 public:
  TestSession(QuicConnection* connection, bool is_server)
      : QuicSession(connection, is_server),
        crypto_stream_(this) {
  }

  virtual QuicCryptoStream* GetCryptoStream() {
    return &crypto_stream_;
  }

  virtual TestStream* CreateOutgoingReliableStream() {
    TestStream* stream = new TestStream(GetNextStreamId(), this);
    ActivateStream(stream);
    return stream;
  }

  virtual TestStream* CreateIncomingReliableStream(QuicStreamId id) {
    return new TestStream(id, this);
  }

  bool IsClosedStream(QuicStreamId id) {
    return QuicSession::IsClosedStream(id);
  }

  ReliableQuicStream* GetIncomingReliableStream(QuicStreamId stream_id) {
    return QuicSession::GetIncomingReliableStream(stream_id);
  }

  // Helper method for gmock
  void MarkTwoWriteBlocked() {
    this->MarkWriteBlocked(2);
  }

  TestCryptoStream crypto_stream_;
};

class QuicSessionTest : public ::testing::Test {
 protected:
  QuicSessionTest()
      : guid_(1),
        connection_(new MockConnection(guid_, IPEndPoint())),
        session_(connection_, true) {
  }

  void CheckClosedStreams() {
    for (int i = kCryptoStreamId; i < 100; i++) {
      if (closed_streams_.count(i) == 0) {
        EXPECT_FALSE(session_.IsClosedStream(i)) << " stream id: " << i;
      } else {
        EXPECT_TRUE(session_.IsClosedStream(i)) << " stream id: " << i;
      }
    }
  }

  void CloseStream(QuicStreamId id) {
    session_.CloseStream(id);
    closed_streams_.insert(id);
  }

  QuicGuid guid_;
  MockConnection* connection_;
  TestSession session_;
  QuicConnectionVisitorInterface* visitor_;
  hash_map<QuicStreamId, ReliableQuicStream*>* streams_;
  set<QuicStreamId> closed_streams_;
};

TEST_F(QuicSessionTest, IsCryptoHandshakeComplete) {
  EXPECT_FALSE(session_.IsCryptoHandshakeComplete());
  CryptoHandshakeMessage message;
  session_.crypto_stream_.OnHandshakeMessage(message);
  EXPECT_TRUE(session_.IsCryptoHandshakeComplete());
}

TEST_F(QuicSessionTest, IsClosedStreamDefault) {
  // Ensure that no streams are initially closed.
  for (int i = kCryptoStreamId; i < 100; i++) {
    EXPECT_FALSE(session_.IsClosedStream(i));
  }
}

TEST_F(QuicSessionTest, IsClosedStreamLocallyCreated) {
  scoped_ptr<TestStream> stream2(session_.CreateOutgoingReliableStream());
  scoped_ptr<TestStream> stream4(session_.CreateOutgoingReliableStream());

  CheckClosedStreams();
  CloseStream(4);
  CheckClosedStreams();
  CloseStream(2);
  CheckClosedStreams();
}

TEST_F(QuicSessionTest, IsClosedStreamPeerCreated) {
  scoped_ptr<ReliableQuicStream> stream3(session_.GetIncomingReliableStream(3));
  scoped_ptr<ReliableQuicStream> stream5(session_.GetIncomingReliableStream(5));

  CheckClosedStreams();
  CloseStream(3);
  CheckClosedStreams();
  CloseStream(5);
  // Create stream id 9, and implicitly 7
  scoped_ptr<ReliableQuicStream> stream9(session_.GetIncomingReliableStream(9));
  CheckClosedStreams();
  // Close 9, but make sure 7 is still not closed
  CloseStream(9);
  CheckClosedStreams();
}

TEST_F(QuicSessionTest, StreamIdTooLarge) {
  scoped_ptr<ReliableQuicStream> stream3(session_.GetIncomingReliableStream(3));
  EXPECT_CALL(*connection_, SendConnectionClose(QUIC_INVALID_STREAM_ID));
  scoped_ptr<ReliableQuicStream> stream5(
      session_.GetIncomingReliableStream(105));
}

TEST_F(QuicSessionTest, OnCanWrite) {
  scoped_ptr<TestStream> stream2(session_.CreateOutgoingReliableStream());
  scoped_ptr<TestStream> stream4(session_.CreateOutgoingReliableStream());
  scoped_ptr<TestStream> stream6(session_.CreateOutgoingReliableStream());

  session_.MarkWriteBlocked(2);
  session_.MarkWriteBlocked(6);
  session_.MarkWriteBlocked(4);

  InSequence s;
  EXPECT_CALL(*stream2.get(), OnCanWrite()).WillOnce(
      // Reregister, to test the loop limit.
      testing::InvokeWithoutArgs(&session_, &TestSession::MarkTwoWriteBlocked));
  EXPECT_CALL(*stream6.get(), OnCanWrite());
  EXPECT_CALL(*stream4.get(), OnCanWrite());

  EXPECT_FALSE(session_.OnCanWrite());
}

TEST_F(QuicSessionTest, OnCanWriteWithClosedStream) {
  scoped_ptr<TestStream> stream2(session_.CreateOutgoingReliableStream());
  scoped_ptr<TestStream> stream4(session_.CreateOutgoingReliableStream());
  scoped_ptr<TestStream> stream6(session_.CreateOutgoingReliableStream());

  session_.MarkWriteBlocked(2);
  session_.MarkWriteBlocked(6);
  session_.MarkWriteBlocked(4);
  CloseStream(6);

  InSequence s;
  EXPECT_CALL(*stream2.get(), OnCanWrite());
  EXPECT_CALL(*stream4.get(), OnCanWrite());
  EXPECT_TRUE(session_.OnCanWrite());
}

}  // namespace
}  // namespace test
}  // namespace net
