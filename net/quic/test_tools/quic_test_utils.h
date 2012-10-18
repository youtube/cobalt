// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Common utilities for Quic tests

#ifndef NET_QUIC_TEST_TOOLS_QUIC_TEST_UTILS_H_
#define NET_QUIC_TEST_TOOLS_QUIC_TEST_UTILS_H_

#include "net/quic/quic_framer.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace net {

namespace test {

void CompareCharArraysWithHexError(const std::string& description,
                                   const char* actual,
                                   const int actual_len,
                                   const char* expected,
                                   const int expected_len);

void CompareQuicDataWithHexError(const std::string& description,
                                 QuicData* actual,
                                 QuicData* expected);

// Constructs a basic crypto handshake message
QuicPacket* ConstructHandshakePacket(QuicGuid guid, CryptoTag tag);

class MockFramerVisitor : public QuicFramerVisitorInterface {
 public:
  MockFramerVisitor();
  ~MockFramerVisitor();

  MOCK_METHOD1(OnError, void(QuicFramer* framer));
  MOCK_METHOD1(OnPacket, void(const IPEndPoint& client_address));
  MOCK_METHOD1(OnPacketHeader, bool(const QuicPacketHeader& header));
  MOCK_METHOD1(OnFecProtectedPayload, void(base::StringPiece payload));
  MOCK_METHOD1(OnStreamFragment, void(const QuicStreamFragment& fragment));
  MOCK_METHOD1(OnAckFragment, void(const QuicAckFragment& fragment));
  MOCK_METHOD1(OnFecData, void(const QuicFecData& fec));
  MOCK_METHOD1(OnRstStreamFragment,
               void(const QuicRstStreamFragment& fragment));
  MOCK_METHOD1(OnConnectionCloseFragment,
               void(const QuicConnectionCloseFragment& fragment));
  MOCK_METHOD0(OnPacketComplete, void());
};

class NoOpFramerVisitor : public QuicFramerVisitorInterface {
 public:
  virtual void OnError(QuicFramer* framer) OVERRIDE {}
  virtual void OnPacket(const IPEndPoint& client_address) OVERRIDE {}
  virtual bool OnPacketHeader(const QuicPacketHeader& header) OVERRIDE;
  virtual void OnFecProtectedPayload(base::StringPiece payload) OVERRIDE {}
  virtual void OnStreamFragment(const QuicStreamFragment& fragment) OVERRIDE {}
  virtual void OnAckFragment(const QuicAckFragment& fragment) OVERRIDE {}
  virtual void OnFecData(const QuicFecData& fec) OVERRIDE {}
  virtual void OnRstStreamFragment(
      const QuicRstStreamFragment& fragment) OVERRIDE {}
  virtual void OnConnectionCloseFragment(
      const QuicConnectionCloseFragment& fragment) OVERRIDE {}
  virtual void OnPacketComplete() OVERRIDE {}
};

}  // namespace test

}  // namespace net

#endif  // NET_QUIC_TEST_TOOLS_QUIC_TEST_UTILS_H_
