// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_reliable_client_stream.h"

#include "net/base/net_errors.h"
#include "net/quic/quic_client_session.h"
#include "net/quic/quic_utils.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::Return;
using testing::StrEq;

namespace net {
namespace test {
namespace {

class MockDelegate : public QuicReliableClientStream::Delegate {
 public:
  MockDelegate() {}

  MOCK_METHOD0(OnSendData, int());
  MOCK_METHOD2(OnSendDataComplete, int(int, bool*));
  MOCK_METHOD2(OnDataReceived, int(const char*, int));
  MOCK_METHOD1(OnClose, void(QuicErrorCode));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDelegate);
};

class QuicReliableClientStreamTest : public ::testing::Test {
 public:
  QuicReliableClientStreamTest()
      : session_(new MockConnection(1, IPEndPoint()), false),
        stream_(1, &session_) {
    stream_.SetDelegate(&delegate_);
  }

  testing::StrictMock<MockDelegate> delegate_;
  MockSession session_;
  QuicReliableClientStream stream_;
};

TEST_F(QuicReliableClientStreamTest, TerminateFromPeer) {
  EXPECT_CALL(delegate_, OnClose(QUIC_NO_ERROR));

  stream_.TerminateFromPeer(true);
}

TEST_F(QuicReliableClientStreamTest, ProcessData) {
  const char data[] = "hello world!";
  EXPECT_CALL(delegate_, OnDataReceived(StrEq(data), arraysize(data)));
  EXPECT_CALL(delegate_, OnClose(QUIC_NO_ERROR));

  EXPECT_EQ(arraysize(data), stream_.ProcessData(data, arraysize(data)));
}

TEST_F(QuicReliableClientStreamTest, ProcessDataWithError) {
  const char data[] = "hello world!";
  EXPECT_CALL(delegate_,
              OnDataReceived(StrEq(data),
                             arraysize(data))).WillOnce(Return(ERR_UNEXPECTED));
  EXPECT_CALL(delegate_, OnClose(QUIC_BAD_APPLICATION_PAYLOAD));


  EXPECT_EQ(0u, stream_.ProcessData(data, arraysize(data)));
}

}  // namespace
}  // namespace test
}  // namespace net
