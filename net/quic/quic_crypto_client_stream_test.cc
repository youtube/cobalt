// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_crypto_client_stream.h"

#include "net/quic/quic_utils.h"
#include "net/quic/test_tools/quic_test_utils.h"

namespace net {
namespace test {
namespace {

class QuicCryptoClientStreamTest : public ::testing::Test {
 public:
  QuicCryptoClientStreamTest()
      : connection_(new MockConnection(1, addr_)),
        session_(connection_, true),
        stream_(&session_) {
    message_.tag = kSHLO;
    message_.tag_value_map[1] = "abc";
    message_.tag_value_map[2] = "def";
    ConstructHandshakeMessage();
  }

  void ConstructHandshakeMessage() {
    CryptoFramer framer;
    message_data_.reset(framer.ConstructHandshakeMessage(message_));
  }

  IPEndPoint addr_;
  MockConnection* connection_;
  MockSession session_;
  QuicCryptoClientStream stream_;
  CryptoHandshakeMessage message_;
  scoped_ptr<QuicData> message_data_;
};

TEST_F(QuicCryptoClientStreamTest, NotInitiallyConected) {
  EXPECT_FALSE(stream_.handshake_complete());
}

TEST_F(QuicCryptoClientStreamTest, ConnectedAfterSHLO) {
  stream_.ProcessData(message_data_->data(), message_data_->length());
  EXPECT_TRUE(stream_.handshake_complete());
}

TEST_F(QuicCryptoClientStreamTest, MessageAfterHandshake) {
  stream_.ProcessData(message_data_->data(), message_data_->length());

  EXPECT_CALL(*connection_, SendConnectionClose(
      QUIC_CRYPTO_MESSAGE_AFTER_HANDSHAKE_COMPLETE));
  stream_.ProcessData(message_data_->data(), message_data_->length());
}

TEST_F(QuicCryptoClientStreamTest, BadMessageType) {
  message_.tag = kCHLO;
  ConstructHandshakeMessage();

  EXPECT_CALL(*connection_,
              SendConnectionClose(QUIC_INVALID_CRYPTO_MESSAGE_TYPE));
  stream_.ProcessData(message_data_->data(), message_data_->length());
}

}  // namespace
}  // namespace test
}  // namespace net
