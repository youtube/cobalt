// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_packet_creator.h"

#include "base/stl_util.h"
#include "net/quic/crypto/null_encrypter.h"
#include "net/quic/quic_utils.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::InSequence;
using testing::_;
using std::vector;
using std::string;

namespace net {
namespace test {
namespace {

class QuicPacketCreatorTest : public ::testing::Test {
 protected:
  QuicPacketCreatorTest()
      : framer_(QuicDecrypter::Create(kNULL), QuicEncrypter::Create(kNULL)),
        id_(1),
        sequence_number_(0),
        guid_(2),
        data_("foo"),
        utils_(guid_, &framer_) {
    framer_.set_visitor(&framer_visitor_);
  }
  ~QuicPacketCreatorTest() {
    STLDeleteValues(&packets_);
  }

  void ProcessPackets() {
    for (size_t i = 0; i < packets_.size(); ++i) {
      scoped_ptr<QuicEncryptedPacket> encrypted(
          framer_.EncryptPacket(*packets_[i].second));
      framer_.ProcessPacket(IPEndPoint(), IPEndPoint(), *encrypted);
    }
  }

  vector<QuicPacketCreator::PacketPair> packets_;
  QuicFramer framer_;
  testing::StrictMock<MockFramerVisitor> framer_visitor_;
  QuicStreamId id_;
  QuicPacketSequenceNumber sequence_number_;
  QuicGuid guid_;
  string data_;
  QuicPacketCreator utils_;
};

TEST_F(QuicPacketCreatorTest, DataToStreamBasic) {
  size_t bytes_consumed = utils_.DataToStream(id_, data_, 0, true, &packets_);

  ASSERT_EQ(1u, packets_.size());
  ASSERT_EQ(1u, utils_.sequence_number());
  ASSERT_EQ(data_.size(), bytes_consumed);

  InSequence s;
  EXPECT_CALL(framer_visitor_, OnPacket(_, _));
  EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
  EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
  EXPECT_CALL(framer_visitor_, OnPacketComplete());

  ProcessPackets();
}

TEST_F(QuicPacketCreatorTest, DataToStreamFec) {
  utils_.options()->use_fec = true;
  size_t bytes_consumed = utils_.DataToStream(id_, data_, 0, true, &packets_);

  ASSERT_EQ(2u, packets_.size());
  ASSERT_EQ(2u, utils_.sequence_number());
  ASSERT_EQ(data_.size(), bytes_consumed);

  InSequence s;
  EXPECT_CALL(framer_visitor_, OnPacket(_, _));
  EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
  EXPECT_CALL(framer_visitor_, OnFecProtectedPayload(_));
  EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
  EXPECT_CALL(framer_visitor_, OnPacketComplete());

  EXPECT_CALL(framer_visitor_, OnPacket(_, _));
  EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
  EXPECT_CALL(framer_visitor_, OnFecData(_));
  EXPECT_CALL(framer_visitor_, OnPacketComplete());

  ProcessPackets();
}

TEST_F(QuicPacketCreatorTest, DataToStreamFecHandled) {
  utils_.options()->use_fec = true;
  size_t bytes_consumed = utils_.DataToStream(id_, data_, 0, true, &packets_);
  ASSERT_EQ(data_.size(), bytes_consumed);

  ASSERT_EQ(2u, packets_.size());
  ASSERT_EQ(2u, utils_.sequence_number());

  QuicFecData fec_data;
  fec_data.fec_group = 1;
  fec_data.min_protected_packet_sequence_number = 1;
  fec_data.redundancy = packets_[0].second->FecProtectedData();

  InSequence s;
  // Data packet
  EXPECT_CALL(framer_visitor_, OnPacket(_, _));
  EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
  EXPECT_CALL(framer_visitor_, OnFecProtectedPayload(_));
  EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
  EXPECT_CALL(framer_visitor_, OnPacketComplete());

  // FEC packet
  EXPECT_CALL(framer_visitor_, OnPacket(_, _));
  EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
  EXPECT_CALL(framer_visitor_, OnFecData(fec_data));
  EXPECT_CALL(framer_visitor_, OnPacketComplete());

  ProcessPackets();

  // Revived data packet
  EXPECT_CALL(framer_visitor_, OnRevivedPacket());
  EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
  EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
  EXPECT_CALL(framer_visitor_, OnPacketComplete());

  QuicPacketHeader header;
  framer_.ProcessRevivedPacket(header, fec_data.redundancy);
}

TEST_F(QuicPacketCreatorTest, DataToStreamSkipFin) {
  size_t bytes_consumed = utils_.DataToStream(id_, data_, 0, false, &packets_);
  ASSERT_EQ(data_.size(), bytes_consumed);

  ASSERT_EQ(1u, packets_.size());
  ASSERT_EQ(1u, utils_.sequence_number());

  InSequence s;
  EXPECT_CALL(framer_visitor_, OnPacket(_, _));
  EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
  EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
  EXPECT_CALL(framer_visitor_, OnPacketComplete());

  ProcessPackets();
}

TEST_F(QuicPacketCreatorTest, NoData) {
  data_ = "";

  size_t bytes_consumed = utils_.DataToStream(id_, data_, 0, true, &packets_);
  ASSERT_EQ(data_.size(), bytes_consumed);

  ASSERT_EQ(1u, packets_.size());
  ASSERT_EQ(1u, utils_.sequence_number());

  InSequence s;
  EXPECT_CALL(framer_visitor_, OnPacket(_, _));
  EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
  EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
  EXPECT_CALL(framer_visitor_, OnPacketComplete());

  ProcessPackets();
}

TEST_F(QuicPacketCreatorTest, MultiplePackets) {
  size_t ciphertext_size = NullEncrypter().GetCiphertextSize(2);
  utils_.options()->max_packet_length =
      ciphertext_size + QuicUtils::StreamFramePacketOverhead(1);

  size_t bytes_consumed = utils_.DataToStream(id_, data_, 0, true, &packets_);
  ASSERT_EQ(data_.size(), bytes_consumed);

  ASSERT_EQ(2u, packets_.size());
  ASSERT_EQ(2u, utils_.sequence_number());

  InSequence s;
  EXPECT_CALL(framer_visitor_, OnPacket(_, _));
  EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
  EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
  EXPECT_CALL(framer_visitor_, OnPacketComplete());

  EXPECT_CALL(framer_visitor_, OnPacket(_, _));
  EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
  EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
  EXPECT_CALL(framer_visitor_, OnPacketComplete());

  ProcessPackets();
}

TEST_F(QuicPacketCreatorTest, MultiplePacketsWithLimits) {
  const size_t kPayloadBytesPerPacket = 2;

  size_t ciphertext_size = NullEncrypter().GetCiphertextSize(
      kPayloadBytesPerPacket);
  utils_.options()->max_packet_length =
      ciphertext_size + QuicUtils::StreamFramePacketOverhead(1);
  utils_.options()->max_num_packets = 1;

  size_t bytes_consumed = utils_.DataToStream(id_, data_, 0, true, &packets_);
  ASSERT_EQ(kPayloadBytesPerPacket, bytes_consumed);

  ASSERT_EQ(1u, packets_.size());
  ASSERT_EQ(1u, utils_.sequence_number());

  InSequence s;
  EXPECT_CALL(framer_visitor_, OnPacket(_, _));
  EXPECT_CALL(framer_visitor_, OnPacketHeader(_));
  EXPECT_CALL(framer_visitor_, OnStreamFrame(_));
  EXPECT_CALL(framer_visitor_, OnPacketComplete());

  ProcessPackets();
}

}  // namespace
}  // namespace test
}  // namespace net
