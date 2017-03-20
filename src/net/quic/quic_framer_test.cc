// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/port.h"
#include "base/stl_util.h"
#include "net/quic/quic_framer.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_utils.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "net/test/disabled_if_big_endian.h"

using base::hash_set;
using base::StringPiece;
using std::map;
using std::string;
using std::vector;

namespace net {
namespace test {

class TestEncrypter : public QuicEncrypter {
 public:
  virtual ~TestEncrypter() {}
  virtual QuicData* Encrypt(StringPiece associated_data,
                            StringPiece plaintext) {
    associated_data_ = associated_data.as_string();
    plaintext_ = plaintext.as_string();
    return new QuicData(plaintext.data(), plaintext.length());
  }
  virtual size_t GetMaxPlaintextSize(size_t ciphertext_size) {
    return ciphertext_size;
  }
  virtual size_t GetCiphertextSize(size_t plaintext_size) {
    return plaintext_size;
  }
  string associated_data_;
  string plaintext_;
};

class TestDecrypter : public QuicDecrypter {
 public:
  virtual ~TestDecrypter() {}
  virtual QuicData* Decrypt(StringPiece associated_data,
                            StringPiece ciphertext) {
    associated_data_ = associated_data.as_string();
    ciphertext_ = ciphertext.as_string();
    return new QuicData(ciphertext.data(), ciphertext.length());
  }
  string associated_data_;
  string ciphertext_;
};

class TestQuicVisitor : public ::net::QuicFramerVisitorInterface {
 public:
  TestQuicVisitor()
      : error_count_(0),
        packet_count_(0),
        frame_count_(0),
        fec_count_(0),
        complete_packets_(0),
        revived_packets_(0),
        accept_packet_(true) {
  }

  ~TestQuicVisitor() {
    STLDeleteElements(&stream_frames_);
    STLDeleteElements(&ack_frames_);
    STLDeleteElements(&congestion_feedback_frames_);
    STLDeleteElements(&fec_data_);
  }

  virtual void OnError(QuicFramer* f) {
    DLOG(INFO) << "QuicFramer Error: " << QuicUtils::ErrorToString(f->error())
               << " (" << f->error() << ")";
    error_count_++;
  }

  virtual void OnPacket(const IPEndPoint& self_address,
                        const IPEndPoint& peer_address) {
    self_address_ = self_address;
    peer_address_ = peer_address;
  }

  virtual void OnRevivedPacket() {
    revived_packets_++;
  }

  virtual bool OnPacketHeader(const QuicPacketHeader& header) {
    packet_count_++;
    header_.reset(new QuicPacketHeader(header));
    return accept_packet_;
  }

  virtual void OnStreamFrame(const QuicStreamFrame& frame) {
    frame_count_++;
    stream_frames_.push_back(new QuicStreamFrame(frame));
  }

  virtual void OnFecProtectedPayload(StringPiece payload) {
    fec_protected_payload_ = payload.as_string();
  }

  virtual void OnAckFrame(const QuicAckFrame& frame) {
    frame_count_++;
    ack_frames_.push_back(new QuicAckFrame(frame));
  }

  virtual void OnCongestionFeedbackFrame(
      const QuicCongestionFeedbackFrame& frame) {
    frame_count_++;
    congestion_feedback_frames_.push_back(
        new QuicCongestionFeedbackFrame(frame));
  }

  virtual void OnFecData(const QuicFecData& fec) {
    fec_count_++;
    fec_data_.push_back(new QuicFecData(fec));
  }

  virtual void OnPacketComplete() {
    complete_packets_++;
  }

  virtual void OnRstStreamFrame(const QuicRstStreamFrame& frame) {
    rst_stream_frame_ = frame;
  }

  virtual void OnConnectionCloseFrame(
      const QuicConnectionCloseFrame& frame) {
    connection_close_frame_ = frame;
  }

  // Counters from the visitor_ callbacks.
  int error_count_;
  int packet_count_;
  int frame_count_;
  int fec_count_;
  int complete_packets_;
  int revived_packets_;
  bool accept_packet_;

  IPEndPoint self_address_;
  IPEndPoint peer_address_;
  scoped_ptr<QuicPacketHeader> header_;
  vector<QuicStreamFrame*> stream_frames_;
  vector<QuicAckFrame*> ack_frames_;
  vector<QuicCongestionFeedbackFrame*> congestion_feedback_frames_;
  vector<QuicFecData*> fec_data_;
  string fec_protected_payload_;
  QuicRstStreamFrame rst_stream_frame_;
  QuicConnectionCloseFrame connection_close_frame_;
};

class QuicFramerTest : public ::testing::Test {
 public:
  QuicFramerTest()
      : encrypter_(new test::TestEncrypter()),
        decrypter_(new test::TestDecrypter()),
        framer_(decrypter_, encrypter_),
        self_address_(IPAddressNumber(), 1),
        peer_address_(IPAddressNumber(), 2) {
    framer_.set_visitor(&visitor_);
  }

  bool CheckEncryption(StringPiece packet) {
    StringPiece associated_data(
        packet.substr(kStartOfHashData,
                      kStartOfEncryptedData - kStartOfHashData));
    if (associated_data != encrypter_->associated_data_) {
      LOG(ERROR) << "Encrypted incorrect associated data.  expected "
                 << associated_data << " actual: "
                 << encrypter_->associated_data_;
      return false;
    }
    StringPiece plaintext(packet.substr(kStartOfEncryptedData));
    if (plaintext != encrypter_->plaintext_) {
      LOG(ERROR) << "Encrypted incorrect plaintext data.  expected "
                 << plaintext << " actual: "
                 << encrypter_->plaintext_;
      return false;
    }
    return true;
  }

  bool CheckDecryption(StringPiece packet) {
    StringPiece associated_data(
        packet.substr(kStartOfHashData,
                      kStartOfEncryptedData - kStartOfHashData));
    if (associated_data != decrypter_->associated_data_) {
      LOG(ERROR) << "Decrypted incorrect associated data.  expected "
                 << associated_data << " actual: "
                 << decrypter_->associated_data_;
      return false;
    }
    StringPiece plaintext(packet.substr(kStartOfEncryptedData));
    if (plaintext != decrypter_->ciphertext_) {
      LOG(ERROR) << "Decrypted incorrect chipertext data.  expected "
                 << plaintext << " actual: "
                 << decrypter_->ciphertext_;
      return false;
    }
    return true;
  }

  char* AsChars(unsigned char* data) {
    return reinterpret_cast<char*>(data);
  }

  void CheckProcessingFails(unsigned char* packet,
                            size_t len,
                            string expected_error,
                            QuicErrorCode error_code) {
    QuicEncryptedPacket encrypted(AsChars(packet), len, false);
    EXPECT_FALSE(framer_.ProcessPacket(self_address_, peer_address_,
                                       encrypted)) << "len: " << len;
    EXPECT_EQ(expected_error, framer_.detailed_error()) << "len: " << len;
    EXPECT_EQ(error_code, framer_.error()) << "len: " << len;
  }

  void ValidateTruncatedAck(const QuicAckFrame* ack, int keys) {
    for (int i = 1; i < keys; ++i) {
      EXPECT_TRUE(ContainsKey(ack->received_info.missing_packets, i)) << i;
    }
    // With no gaps in the missing packets, we can't admit to having received
    // anything.
    EXPECT_EQ(0u, ack->received_info.largest_received);
  }

  test::TestEncrypter* encrypter_;
  test::TestDecrypter* decrypter_;
  QuicFramer framer_;
  test::TestQuicVisitor visitor_;
  IPEndPoint self_address_;
  IPEndPoint peer_address_;
};

TEST_F(QuicFramerTest, EmptyPacket) {
  char packet[] = { 0x00 };
  QuicEncryptedPacket encrypted(packet, 0, false);
  EXPECT_FALSE(framer_.ProcessPacket(self_address_, peer_address_, encrypted));
  EXPECT_EQ(QUIC_INVALID_PACKET_HEADER, framer_.error());
}

TEST_F(QuicFramerTest, DISABLED_IF_BIG_ENDIAN(LargePacket)) {
  unsigned char packet[kMaxPacketSize + 1] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // flags
    0x00,
    // fec group
    0x00,
    // frame count
    0x01,
  };

  memset(packet + kPacketHeaderSize, 0, kMaxPacketSize - kPacketHeaderSize + 1);

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(self_address_, peer_address_, encrypted));

  ASSERT_TRUE(visitor_.header_.get());

  // Make sure we've parsed the packet header, so we can send an error.
  EXPECT_EQ(GG_UINT64_C(0xFEDCBA9876543210), visitor_.header_->guid);
  // Make sure the correct error is propogated.
  EXPECT_EQ(QUIC_PACKET_TOO_LARGE, framer_.error());
}

TEST_F(QuicFramerTest, DISABLED_IF_BIG_ENDIAN(PacketHeader)) {
  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // flags
    0x00,
    // fec group
    0x00,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(self_address_, peer_address_, encrypted));

  EXPECT_EQ(QUIC_INVALID_FRAME_DATA, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(GG_UINT64_C(0xFEDCBA9876543210), visitor_.header_->guid);
  EXPECT_EQ(GG_UINT64_C(0x123456789ABC),
            visitor_.header_->packet_sequence_number);
  EXPECT_EQ(0x00, visitor_.header_->flags);
  EXPECT_EQ(0x00, visitor_.header_->fec_group);

  // Now test framing boundaries
  for (int i = 0; i < 16; ++i) {
    string expected_error;
    if (i < 8) {
      expected_error = "Unable to read GUID.";
    } else if (i < 14) {
      expected_error = "Unable to read sequence number.";
    } else if (i < 15) {
      expected_error = "Unable to read flags.";
    } else if (i < 16) {
      expected_error = "Unable to read fec group.";
    }
    CheckProcessingFails(packet, i, expected_error, QUIC_INVALID_PACKET_HEADER);
  }
}

TEST_F(QuicFramerTest, DISABLED_IF_BIG_ENDIAN(StreamFrame)) {
  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // flags
    0x00,
    // fec group
    0x00,

    // frame count
    0x01,
    // frame type (stream frame)
    0x00,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // fin
    0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // data length
    0x0c, 0x00,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(self_address_, peer_address_, encrypted));

  EXPECT_TRUE(CheckDecryption(StringPiece(AsChars(packet), arraysize(packet))));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  ASSERT_EQ(peer_address_, visitor_.peer_address_);
  ASSERT_EQ(self_address_, visitor_.self_address_);

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  EXPECT_EQ(static_cast<uint64>(0x01020304),
            visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(GG_UINT64_C(0xBA98FEDC32107654),
            visitor_.stream_frames_[0]->offset);
  EXPECT_EQ("hello world!", visitor_.stream_frames_[0]->data);

  // Now test framing boundaries
  for (size_t i = 0; i < 29; ++i) {
    string expected_error;
    if (i < 1) {
      expected_error = "Unable to read frame count.";
    } else if (i < 2) {
      expected_error = "Unable to read frame type.";
    } else if (i < 6) {
      expected_error = "Unable to read stream_id.";
    } else if (i < 7) {
      expected_error = "Unable to read fin.";
    } else if (i < 15) {
      expected_error = "Unable to read offset.";
    } else if (i < 29) {
      expected_error = "Unable to read frame data.";
    }
    CheckProcessingFails(packet, i + kPacketHeaderSize, expected_error,
                         QUIC_INVALID_FRAME_DATA);
  }
}

TEST_F(QuicFramerTest, RejectPacket) {
  visitor_.accept_packet_ = false;

  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // flags
    0x00,
    // fec group
    0x00,

    // frame count
    0x01,
    // frame type (stream frame)
    0x00,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // fin
    0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // data length
    0x0c, 0x00,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(self_address_, peer_address_, encrypted));

  EXPECT_TRUE(CheckDecryption(StringPiece(AsChars(packet), arraysize(packet))));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  ASSERT_EQ(peer_address_, visitor_.peer_address_);

  ASSERT_EQ(0u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
}

TEST_F(QuicFramerTest, DISABLED_IF_BIG_ENDIAN(RevivedStreamFrame)) {
  unsigned char payload[] = {
    // frame count
    0x01,
    // frame type (stream frame)
    0x00,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // fin
    0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // data length
    0x0c, 0x00,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  QuicPacketHeader header;
  header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.packet_sequence_number = GG_UINT64_C(0x123456789ABC);
  header.flags = PACKET_FLAGS_NONE;
  header.fec_group = 0;

  // Do not encrypt the payload because the revived payload is post-encryption.
  EXPECT_TRUE(framer_.ProcessRevivedPacket(header,
                                           StringPiece(AsChars(payload),
                                                       arraysize(payload))));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_EQ(1, visitor_.revived_packets_);
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(GG_UINT64_C(0xFEDCBA9876543210), visitor_.header_->guid);
  EXPECT_EQ(GG_UINT64_C(0x123456789ABC),
            visitor_.header_->packet_sequence_number);
  EXPECT_EQ(0x00, visitor_.header_->flags);
  EXPECT_EQ(0x00, visitor_.header_->fec_group);


  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  EXPECT_EQ(GG_UINT64_C(0x01020304), visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(GG_UINT64_C(0xBA98FEDC32107654),
            visitor_.stream_frames_[0]->offset);
  EXPECT_EQ("hello world!", visitor_.stream_frames_[0]->data);
}

TEST_F(QuicFramerTest, DISABLED_IF_BIG_ENDIAN(StreamFrameInFecGroup)) {
  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x12, 0x34,
    // flags
    0x00,
    // fec group
    0x02,

    // frame count
    0x01,
    // frame type (stream frame)
    0x00,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // fin
    0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // data length
    0x0c, 0x00,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(self_address_, peer_address_, encrypted));

  EXPECT_TRUE(CheckDecryption(StringPiece(AsChars(packet), arraysize(packet))));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(2, visitor_.header_->fec_group);
  EXPECT_EQ(string(AsChars(packet) + kStartOfFecProtectedData,
                   arraysize(packet) - kStartOfFecProtectedData),
            visitor_.fec_protected_payload_);
  ASSERT_EQ(peer_address_, visitor_.peer_address_);

  ASSERT_EQ(1u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  EXPECT_EQ(GG_UINT64_C(0x01020304), visitor_.stream_frames_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_frames_[0]->fin);
  EXPECT_EQ(GG_UINT64_C(0xBA98FEDC32107654),
            visitor_.stream_frames_[0]->offset);
  EXPECT_EQ("hello world!", visitor_.stream_frames_[0]->data);
}

TEST_F(QuicFramerTest, DISABLED_IF_BIG_ENDIAN(AckFrame)) {
  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // flags
    0x00,
    // fec group
    0x00,

    // frame count
    0x01,
    // frame type (ack frame)
    0x02,
    // least packet sequence number awaiting an ack
    0xA0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // largest received packet sequence number
    0xBF, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // num missing packets
    0x01,
    // missing packet
    0xBE, 0x9A, 0x78, 0x56,
    0x34, 0x12,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(self_address_, peer_address_, encrypted));

  EXPECT_TRUE(CheckDecryption(StringPiece(AsChars(packet), arraysize(packet))));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  ASSERT_EQ(1u, visitor_.ack_frames_.size());
  const QuicAckFrame& frame = *visitor_.ack_frames_[0];
  EXPECT_EQ(GG_UINT64_C(0x0123456789ABF), frame.received_info.largest_received);
  ASSERT_EQ(1u, frame.received_info.missing_packets.size());
  SequenceSet::const_iterator missing_iter =
      frame.received_info.missing_packets.begin();
  EXPECT_EQ(GG_UINT64_C(0x0123456789ABE), *missing_iter);
  EXPECT_EQ(GG_UINT64_C(0x0123456789AA0), frame.sent_info.least_unacked);

  // Now test framing boundaries
  for (size_t i = 0; i < 21; ++i) {
    string expected_error;
    if (i < 1) {
      expected_error = "Unable to read frame count.";
    } else if (i < 2) {
      expected_error = "Unable to read frame type.";
    } else if (i < 8) {
      expected_error = "Unable to read least unacked.";
    } else if (i < 14) {
      expected_error = "Unable to read largest received.";
    } else if (i < 15) {
      expected_error = "Unable to read num missing packets.";
    } else if (i < 21) {
      expected_error = "Unable to read sequence number in missing packets.";
    }    CheckProcessingFails(packet, i + kPacketHeaderSize, expected_error,
                         QUIC_INVALID_FRAME_DATA);
  }
}

TEST_F(QuicFramerTest, DISABLED_IF_BIG_ENDIAN(CongestionFeedbackFrameTCP)) {
  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // flags
    0x00,
    // fec group
    0x00,

    // frame count
    0x01,
    // frame type (congestion feedback frame)
    0x03,
    // congestion feedback type (tcp)
    0x00,
    // ack_frame.feedback.tcp.accumulated_number_of_lost_packets
    0x01, 0x02,
    // ack_frame.feedback.tcp.receive_window
    0x03, 0x04,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(self_address_, peer_address_, encrypted));

  EXPECT_TRUE(CheckDecryption(StringPiece(AsChars(packet), arraysize(packet))));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  ASSERT_EQ(1u, visitor_.congestion_feedback_frames_.size());
  const QuicCongestionFeedbackFrame& frame =
      *visitor_.congestion_feedback_frames_[0];
  ASSERT_EQ(kTCP, frame.type);
  EXPECT_EQ(0x0201,
            frame.tcp.accumulated_number_of_lost_packets);
  EXPECT_EQ(0x0403, frame.tcp.receive_window);

  // Now test framing boundaries
  for (size_t i = 0; i < 7; ++i) {
    string expected_error;
    if (i < 1) {
      expected_error = "Unable to read frame count.";
    } else if (i < 2) {
      expected_error = "Unable to read frame type.";
    } else if (i < 3) {
      expected_error = "Unable to read congestion feedback type.";
    } else if (i < 5) {
      expected_error = "Unable to read accumulated number of lost packets.";
    } else if (i < 7) {
      expected_error = "Unable to read receive window.";
    }
    CheckProcessingFails(packet, i + kPacketHeaderSize, expected_error,
                         QUIC_INVALID_FRAME_DATA);
  }
}

TEST_F(QuicFramerTest,
       DISABLED_IF_BIG_ENDIAN(CongestionFeedbackFrameInterArrival)) {
  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // flags
    0x00,
    // fec group
    0x00,

    // frame count
    0x01,
    // frame type (congestion feedback frame)
    0x03,
    // congestion feedback type (inter arrival)
    0x01,
    // accumulated_number_of_lost_packets
    0x02, 0x03,
    // offset_time
    0x04, 0x05,
    // delta_time
    0x06, 0x07,
    // num received packets
    0x03,
    // smallest ack sequence number
    0xBA, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // ack time
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0x07,
    // sequence delta
    0x01, 0x00,
    // time delta
    0x01, 0x00, 0x00, 0x00,
    // sequence delta (skip one packet)
    0x03, 0x00,
    // time delta
    0x02, 0x00, 0x00, 0x00,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(self_address_, peer_address_, encrypted));

  EXPECT_TRUE(CheckDecryption(StringPiece(AsChars(packet), arraysize(packet))));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  ASSERT_EQ(1u, visitor_.congestion_feedback_frames_.size());
  const QuicCongestionFeedbackFrame& frame =
      *visitor_.congestion_feedback_frames_[0];
  ASSERT_EQ(kInterArrival, frame.type);
  EXPECT_EQ(0x0302, frame.inter_arrival.
            accumulated_number_of_lost_packets);
  EXPECT_EQ(0x0504, frame.inter_arrival.offset_time);
  EXPECT_EQ(0x0706, frame.inter_arrival.delta_time);
  ASSERT_EQ(3u, frame.inter_arrival.received_packet_times.size());
  TimeMap::const_iterator iter =
      frame.inter_arrival.received_packet_times.begin();
  EXPECT_EQ(GG_UINT64_C(0x0123456789ABA), iter->first);
  EXPECT_EQ(QuicTime::FromMicroseconds(GG_UINT64_C(0x07E1D2C3B4A59687)),
            iter->second);
  ++iter;
  EXPECT_EQ(GG_UINT64_C(0x0123456789ABB), iter->first);
  EXPECT_EQ(QuicTime::FromMicroseconds(GG_UINT64_C(0x07E1D2C3B4A59688)),
            iter->second);
  ++iter;
  EXPECT_EQ(GG_UINT64_C(0x0123456789ABD), iter->first);
  EXPECT_EQ(QuicTime::FromMicroseconds(GG_UINT64_C(0x07E1D2C3B4A59689)),
            iter->second);

  // Now test framing boundaries
  for (size_t i = 0; i < 36; ++i) {
    string expected_error;
    if (i < 1) {
      expected_error = "Unable to read frame count.";
    } else if (i < 2) {
      expected_error = "Unable to read frame type.";
    } else if (i < 3) {
      expected_error = "Unable to read congestion feedback type.";
    } else if (i < 5) {
      expected_error = "Unable to read accumulated number of lost packets.";
    } else if (i < 7) {
      expected_error = "Unable to read offset time.";
    } else if (i < 9) {
      expected_error = "Unable to read delta time.";
    } else if (i < 10) {
      expected_error = "Unable to read num received packets.";
    } else if (i < 16) {
      expected_error = "Unable to read smallest received.";
    } else if (i < 24) {
      expected_error = "Unable to read time received.";
    } else if (i < 26) {
      expected_error = "Unable to read sequence delta in received packets.";
    } else if (i < 30) {
      expected_error = "Unable to read time delta in received packets.";
    } else if (i < 32) {
      expected_error = "Unable to read sequence delta in received packets.";
    } else if (i < 36) {
      expected_error = "Unable to read time delta in received packets.";
    }
    CheckProcessingFails(packet, i + kPacketHeaderSize, expected_error,
                         QUIC_INVALID_FRAME_DATA);
  }
}

TEST_F(QuicFramerTest, DISABLED_IF_BIG_ENDIAN(CongestionFeedbackFrameFixRate)) {
  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // flags
    0x00,
    // fec group
    0x00,

    // frame count
    0x01,
    // frame type (congestion feedback frame)
    0x03,
    // congestion feedback type (fix rate)
    0x02,
    // bitrate_in_bytes_per_second;
    0x01, 0x02, 0x03, 0x04,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(self_address_, peer_address_, encrypted));

  EXPECT_TRUE(CheckDecryption(StringPiece(AsChars(packet), arraysize(packet))));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  ASSERT_EQ(1u, visitor_.congestion_feedback_frames_.size());
  const QuicCongestionFeedbackFrame& frame =
      *visitor_.congestion_feedback_frames_[0];
  ASSERT_EQ(kFixRate, frame.type);
  EXPECT_EQ(static_cast<uint32>(0x04030201),
            frame.fix_rate.bitrate_in_bytes_per_second);

  // Now test framing boundaries
  for (size_t i = 0; i < 7; ++i) {
    string expected_error;
    if (i < 1) {
      expected_error = "Unable to read frame count.";
    } else if (i < 2) {
      expected_error = "Unable to read frame type.";
    } else if (i < 3) {
      expected_error = "Unable to read congestion feedback type.";
    } else if (i < 7) {
      expected_error = "Unable to read bitrate.";
    }
    CheckProcessingFails(packet, i + kPacketHeaderSize, expected_error,
                         QUIC_INVALID_FRAME_DATA);
  }
}


TEST_F(QuicFramerTest, CongestionFeedbackFrameInvalidFeedback) {
  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // flags
    0x00,
    // fec group
    0x00,

    // frame count
    0x01,
    // frame type (congestion feedback frame)
    0x03,
    // congestion feedback type (invalid)
    0x03,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_FALSE(framer_.ProcessPacket(self_address_, peer_address_, encrypted));
  EXPECT_TRUE(CheckDecryption(StringPiece(AsChars(packet), arraysize(packet))));
  EXPECT_EQ(QUIC_INVALID_FRAME_DATA, framer_.error());
}

TEST_F(QuicFramerTest, DISABLED_IF_BIG_ENDIAN(RstStreamFrame)) {
  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // flags
    0x00,
    // fec group
    0x00,

    // frame count
    0x01,
    // frame type (rst stream frame)
    0x04,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // error code
    0x08, 0x07, 0x06, 0x05,

    // error details length
    0x0d, 0x00,
    // error details
    'b',  'e',  'c',  'a',
    'u',  's',  'e',  ' ',
    'I',  ' ',  'c',  'a',
    'n',
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(self_address_, peer_address_, encrypted));

  EXPECT_TRUE(CheckDecryption(StringPiece(AsChars(packet), arraysize(packet))));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  ASSERT_EQ(peer_address_, visitor_.peer_address_);

  EXPECT_EQ(GG_UINT64_C(0x01020304), visitor_.rst_stream_frame_.stream_id);
  EXPECT_EQ(0x05060708, visitor_.rst_stream_frame_.error_code);
  EXPECT_EQ(GG_UINT64_C(0xBA98FEDC32107654),
            visitor_.rst_stream_frame_.offset);
  EXPECT_EQ("because I can", visitor_.rst_stream_frame_.error_details);

  // Now test framing boundaries
  for (size_t i = 3; i < 33; ++i) {
    string expected_error;
    if (i < 6) {
      expected_error = "Unable to read stream_id.";
    } else if (i < 14) {
      expected_error = "Unable to read offset in rst frame.";
    } else if (i < 18) {
      expected_error = "Unable to read rst stream error code.";
    } else if (i < 33) {
      expected_error = "Unable to read rst stream error details.";
    }
    CheckProcessingFails(packet, i + kPacketHeaderSize, expected_error,
                         QUIC_INVALID_RST_STREAM_DATA);
  }
}

TEST_F(QuicFramerTest, DISABLED_IF_BIG_ENDIAN(ConnectionCloseFrame)) {
  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // flags
    0x00,
    // fec group
    0x00,


    // frame count
    0x01,
    // frame type (connection close frame)
    0x05,
    // error code
    0x08, 0x07, 0x06, 0x05,

    // error details length
    0x0d, 0x00,
    // error details
    'b',  'e',  'c',  'a',
    'u',  's',  'e',  ' ',
    'I',  ' ',  'c',  'a',
    'n',

    // Ack frame.
    // least packet sequence number awaiting an ack
    0xA0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // largest received packet sequence number
    0xBF, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // num missing packets
    0x01,
    // missing packet
    0xBE, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // congestion feedback type (inter arrival)
    0x02,
    // accumulated_number_of_lost_packets
    0x02, 0x03,
    // offset_time
    0x04, 0x05,
    // delta_time
    0x06, 0x07,
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(self_address_, peer_address_, encrypted));

  EXPECT_TRUE(CheckDecryption(StringPiece(AsChars(packet), arraysize(packet))));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());

  EXPECT_EQ(0u, visitor_.stream_frames_.size());

  EXPECT_EQ(0x05060708, visitor_.connection_close_frame_.error_code);
  EXPECT_EQ("because I can", visitor_.connection_close_frame_.error_details);

  ASSERT_EQ(1u, visitor_.ack_frames_.size());
  const QuicAckFrame& frame = *visitor_.ack_frames_[0];
  EXPECT_EQ(GG_UINT64_C(0x0123456789ABF), frame.received_info.largest_received);
  ASSERT_EQ(1u, frame.received_info.missing_packets.size());
  SequenceSet::const_iterator missing_iter =
      frame.received_info.missing_packets.begin();
  EXPECT_EQ(GG_UINT64_C(0x0123456789ABE), *missing_iter);
  EXPECT_EQ(GG_UINT64_C(0x0123456789AA0), frame.sent_info.least_unacked);

  // Now test framing boundaries
  for (size_t i = 3; i < 21; ++i) {
    string expected_error;
    if (i < 6) {
      expected_error = "Unable to read connection close error code.";
    } else if (i < 21) {
      expected_error = "Unable to read connection close error details.";
    }
    CheckProcessingFails(packet, i + kPacketHeaderSize, expected_error,
                         QUIC_INVALID_CONNECTION_CLOSE_DATA);
  }
}

TEST_F(QuicFramerTest, DISABLED_IF_BIG_ENDIAN(FecPacket)) {
  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // flags (FEC)
    0x01,
    // fec group
    0x01,

    // first protected packet
    0xBB, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // redundancy
    'a',  'b',  'c',  'd',
    'e',  'f',  'g',  'h',
    'i',  'j',  'k',  'l',
    'm',  'n',  'o',  'p',
  };

  QuicEncryptedPacket encrypted(AsChars(packet), arraysize(packet), false);
  EXPECT_TRUE(framer_.ProcessPacket(self_address_, peer_address_, encrypted));

  EXPECT_TRUE(CheckDecryption(StringPiece(AsChars(packet), arraysize(packet))));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());

  EXPECT_EQ(0u, visitor_.stream_frames_.size());
  EXPECT_EQ(0u, visitor_.ack_frames_.size());
  ASSERT_EQ(1, visitor_.fec_count_);
  const QuicFecData& fec_data = *visitor_.fec_data_[0];
  EXPECT_EQ(GG_UINT64_C(0x0123456789ABB),
            fec_data.min_protected_packet_sequence_number);
  EXPECT_EQ("abcdefghijklmnop", fec_data.redundancy);
}

TEST_F(QuicFramerTest, DISABLED_IF_BIG_ENDIAN(ConstructStreamFramePacket)) {
  QuicPacketHeader header;
  header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.packet_sequence_number = GG_UINT64_C(0x123456789ABC);
  header.flags = PACKET_FLAGS_NONE;
  header.fec_group = 0;

  QuicStreamFrame stream_frame;
  stream_frame.stream_id = 0x01020304;
  stream_frame.fin = true;
  stream_frame.offset = GG_UINT64_C(0xBA98FEDC32107654);
  stream_frame.data = "hello world!";

  QuicFrames frames;
  frames.push_back(QuicFrame(&stream_frame));

  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // flags
    0x00,
    // fec group
    0x00,

    // frame count
    0x01,
    // frame type (stream frame)
    0x00,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // fin
    0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // data length
    0x0c, 0x00,
    // data
    'h',  'e',  'l',  'l',
    'o',  ' ',  'w',  'o',
    'r',  'l',  'd',  '!',
  };

  scoped_ptr<QuicPacket> data(framer_.ConstructFrameDataPacket(header, frames));
  ASSERT_TRUE(data != NULL);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST_F(QuicFramerTest, DISABLED_IF_BIG_ENDIAN(ConstructAckFramePacket)) {
  QuicPacketHeader header;
  header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.packet_sequence_number = GG_UINT64_C(0x123456789ABC);
  header.flags = PACKET_FLAGS_NONE;
  header.fec_group = 0;

  QuicAckFrame ack_frame;
  ack_frame.received_info.largest_received = GG_UINT64_C(0x0123456789ABF);
  ack_frame.received_info.missing_packets.insert(GG_UINT64_C(0x0123456789ABE));
  ack_frame.sent_info.least_unacked = GG_UINT64_C(0x0123456789AA0);

  QuicFrames frames;
  frames.push_back(QuicFrame(&ack_frame));

  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // flags
    0x00,
    // fec group
    0x00,

    // frame count
    0x01,
    // frame type (ack frame)
    0x02,
    // least packet sequence number awaiting an ack
    0xA0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // largest received packet sequence number
    0xBF, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // num missing packets
    0x01,
    // missing packet
    0xBE, 0x9A, 0x78, 0x56,
    0x34, 0x12,
  };

  scoped_ptr<QuicPacket> data(framer_.ConstructFrameDataPacket(header, frames));
  ASSERT_TRUE(data != NULL);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST_F(QuicFramerTest,
       DISABLED_IF_BIG_ENDIAN(ConstructCongestionFeedbackFramePacketTCP)) {
  QuicPacketHeader header;
  header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.packet_sequence_number = GG_UINT64_C(0x123456789ABC);
  header.flags = PACKET_FLAGS_NONE;
  header.fec_group = 0;

  QuicCongestionFeedbackFrame congestion_feedback_frame;
  congestion_feedback_frame.type = kTCP;
  congestion_feedback_frame.tcp.accumulated_number_of_lost_packets = 0x0201;
  congestion_feedback_frame.tcp.receive_window = 0x0403;

  QuicFrames frames;
  frames.push_back(QuicFrame(&congestion_feedback_frame));

  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // flags
    0x00,
    // fec group
    0x00,

    // frame count
    0x01,
    // frame type (congestion feedback frame)
    0x03,
    // congestion feedback type (TCP)
    0x00,
    // accumulated number of lost packets
    0x01, 0x02,
    // TCP receive window
    0x03, 0x04,
  };

  scoped_ptr<QuicPacket> data(framer_.ConstructFrameDataPacket(header, frames));
  ASSERT_TRUE(data != NULL);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST_F(QuicFramerTest,
       DISABLED_IF_BIG_ENDIAN(
           ConstructCongestionFeedbackFramePacketInterArrival)) {
  QuicPacketHeader header;
  header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.packet_sequence_number = GG_UINT64_C(0x123456789ABC);
  header.flags = PACKET_FLAGS_NONE;
  header.fec_group = 0;

  QuicCongestionFeedbackFrame frame;
  frame.type = kInterArrival;
  frame.inter_arrival.accumulated_number_of_lost_packets
      = 0x0302;
  frame.inter_arrival.offset_time = 0x0504;
  frame.inter_arrival.delta_time = 0x0706;
  frame.inter_arrival.received_packet_times[GG_UINT64_C(0x0123456789ABA)] =
      QuicTime::FromMicroseconds(GG_UINT64_C(0x07E1D2C3B4A59687));
  frame.inter_arrival.received_packet_times[GG_UINT64_C(0x0123456789ABB)] =
      QuicTime::FromMicroseconds(GG_UINT64_C(0x07E1D2C3B4A59688));
  frame.inter_arrival.received_packet_times[GG_UINT64_C(0x0123456789ABD)] =
      QuicTime::FromMicroseconds(GG_UINT64_C(0x07E1D2C3B4A59689));

  QuicFrames frames;
  frames.push_back(QuicFrame(&frame));

  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // flags
    0x00,
    // fec group
    0x00,

    // frame count
    0x01,
    // frame type (congestion feedback frame)
    0x03,
    // congestion feedback type (inter arrival)
    0x01,
    // accumulated_number_of_lost_packets
    0x02, 0x03,
    // offset_time
    0x04, 0x05,
    // delta_time
    0x06, 0x07,
    // num received packets
    0x03,
    // smallest ack sequence number
    0xBA, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // ack time
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0x07,
    // sequence delta
    0x01, 0x00,
    // time delta
    0x01, 0x00, 0x00, 0x00,
    // sequence delta (skip one packet)
    0x03, 0x00,
    // time delta
    0x02, 0x00, 0x00, 0x00,
  };

  scoped_ptr<QuicPacket> data(framer_.ConstructFrameDataPacket(header, frames));
  ASSERT_TRUE(data != NULL);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST_F(QuicFramerTest,
       DISABLED_IF_BIG_ENDIAN(ConstructCongestionFeedbackFramePacketFixRate)) {
  QuicPacketHeader header;
  header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.packet_sequence_number = GG_UINT64_C(0x123456789ABC);
  header.flags = PACKET_FLAGS_NONE;
  header.fec_group = 0;

  QuicCongestionFeedbackFrame congestion_feedback_frame;
  congestion_feedback_frame.type = kFixRate;
  congestion_feedback_frame.fix_rate.bitrate_in_bytes_per_second
      = 0x04030201;

  QuicFrames frames;
  frames.push_back(QuicFrame(&congestion_feedback_frame));

  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // flags
    0x00,
    // fec group
    0x00,

    // frame count
    0x01,
    // frame type (congestion feedback frame)
    0x03,
    // congestion feedback type (fix rate)
    0x02,
    // bitrate_in_bytes_per_second;
    0x01, 0x02, 0x03, 0x04,
  };

  scoped_ptr<QuicPacket> data(framer_.ConstructFrameDataPacket(header, frames));
  ASSERT_TRUE(data != NULL);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST_F(QuicFramerTest, ConstructCongestionFeedbackFramePacketInvalidFeedback) {
  QuicPacketHeader header;
  header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.packet_sequence_number = GG_UINT64_C(0x123456789ABC);
  header.flags = PACKET_FLAGS_NONE;
  header.fec_group = 0;

  QuicCongestionFeedbackFrame congestion_feedback_frame;
  congestion_feedback_frame.type =
      static_cast<CongestionFeedbackType>(kFixRate + 1);

  QuicFrames frames;
  frames.push_back(QuicFrame(&congestion_feedback_frame));

  scoped_ptr<QuicPacket> data(framer_.ConstructFrameDataPacket(header, frames));
  ASSERT_TRUE(data == NULL);
}

TEST_F(QuicFramerTest, DISABLED_IF_BIG_ENDIAN(ConstructRstFramePacket)) {
  QuicPacketHeader header;
  header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.packet_sequence_number = GG_UINT64_C(0x123456789ABC);
  header.flags = PACKET_FLAGS_NONE;
  header.fec_group = 0;

  QuicRstStreamFrame rst_frame;
  rst_frame.stream_id = 0x01020304;
  rst_frame.error_code = static_cast<QuicErrorCode>(0x05060708);
  rst_frame.offset = GG_UINT64_C(0xBA98FEDC32107654);
  rst_frame.error_details = "because I can";

  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // flags
    0x00,
    // fec group
    0x00,

    // frame count
    0x01,
    // frame type (rst stream frame)
    0x04,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // error code
    0x08, 0x07, 0x06, 0x05,
    // error details length
    0x0d, 0x00,
    // error details
    'b',  'e',  'c',  'a',
    'u',  's',  'e',  ' ',
    'I',  ' ',  'c',  'a',
    'n',
  };

  QuicFrames frames;
  frames.push_back(QuicFrame(&rst_frame));

  scoped_ptr<QuicPacket> data(framer_.ConstructFrameDataPacket(header, frames));
  ASSERT_TRUE(data != NULL);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST_F(QuicFramerTest, DISABLED_IF_BIG_ENDIAN(ConstructCloseFramePacket)) {
  QuicPacketHeader header;
  header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.packet_sequence_number = GG_UINT64_C(0x123456789ABC);
  header.flags = PACKET_FLAGS_NONE;
  header.fec_group = 0;

  QuicConnectionCloseFrame close_frame;
  close_frame.error_code = static_cast<QuicErrorCode>(0x05060708);
  close_frame.error_details = "because I can";

  QuicAckFrame* ack_frame = &close_frame.ack_frame;
  ack_frame->received_info.largest_received = GG_UINT64_C(0x0123456789ABF);
  ack_frame->received_info.missing_packets.insert(GG_UINT64_C(0x0123456789ABE));
  ack_frame->sent_info.least_unacked = GG_UINT64_C(0x0123456789AA0);

  QuicFrames frames;
  frames.push_back(QuicFrame(&close_frame));

  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // flags
    0x00,
    // fec group
    0x00,

    // frame count
    0x01,
    // frame type (connection close frame)
    0x05,
    // error code
    0x08, 0x07, 0x06, 0x05,
    // error details length
    0x0d, 0x00,
    // error details
    'b',  'e',  'c',  'a',
    'u',  's',  'e',  ' ',
    'I',  ' ',  'c',  'a',
    'n',

    // Ack frame.
    // least packet sequence number awaiting an ack
    0xA0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // largest received packet sequence number
    0xBF, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // num missing packets
    0x01,
    // missing packet
    0xBE, 0x9A, 0x78, 0x56,
    0x34, 0x12,
  };

  scoped_ptr<QuicPacket> data(framer_.ConstructFrameDataPacket(header, frames));
  ASSERT_TRUE(data != NULL);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST_F(QuicFramerTest, DISABLED_IF_BIG_ENDIAN(ConstructFecPacket)) {
  QuicPacketHeader header;
  header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.packet_sequence_number = (GG_UINT64_C(0x123456789ABC));
  header.flags = PACKET_FLAGS_FEC;
  header.fec_group = 1;

  QuicFecData fec_data;
  fec_data.fec_group = 1;
  fec_data.min_protected_packet_sequence_number =
      GG_UINT64_C(0x123456789ABB);
  fec_data.redundancy = "abcdefghijklmnop";

  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // flags
    0x01,
    // fec group
    0x01,
    // first protected packet
    0xBB, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // redundancy
    'a',  'b',  'c',  'd',
    'e',  'f',  'g',  'h',
    'i',  'j',  'k',  'l',
    'm',  'n',  'o',  'p',
  };

  scoped_ptr<QuicPacket> data(framer_.ConstructFecPacket(header, fec_data));
  ASSERT_TRUE(data != NULL);

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));
}

TEST_F(QuicFramerTest, EncryptPacket) {
  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // flags
    0x01,
    // fec group
    0x01,
    // first protected packet
    0xBB, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // redundancy
    'a',  'b',  'c',  'd',
    'e',  'f',  'g',  'h',
    'i',  'j',  'k',  'l',
    'm',  'n',  'o',  'p',
  };

  QuicPacket raw(AsChars(packet), arraysize(packet), false, PACKET_FLAGS_NONE);
  scoped_ptr<QuicEncryptedPacket> encrypted(framer_.EncryptPacket(raw));

  ASSERT_TRUE(encrypted.get() != NULL);
  EXPECT_TRUE(CheckEncryption(StringPiece(AsChars(packet), arraysize(packet))));
}

TEST_F(QuicFramerTest, CalculateLargestReceived) {
  SequenceSet missing;
  missing.insert(1);
  missing.insert(5);
  missing.insert(7);
  missing.insert(8);
  missing.insert(9);

  // These two we just walk to the next gap, and return the largest seen.
  EXPECT_EQ(4u, QuicFramer::CalculateLargestReceived(missing, missing.find(1)));
  EXPECT_EQ(6u, QuicFramer::CalculateLargestReceived(missing, missing.find(5)));

  // 7+ are fun because there is no subsequent gap: we have to walk before
  // them to find 6.
  EXPECT_EQ(6u, QuicFramer::CalculateLargestReceived(missing, missing.find(7)));
  EXPECT_EQ(6u, QuicFramer::CalculateLargestReceived(missing, missing.find(8)));
  EXPECT_EQ(6u, QuicFramer::CalculateLargestReceived(missing, missing.find(9)));

  // Fill in the gap of 6 to make sure we handle a gap of more than 1 by
  // returning 4 rather than 3.
  missing.insert(6);
  EXPECT_EQ(4u, QuicFramer::CalculateLargestReceived(missing, missing.find(9)));

  // Fill in the rest of the gaps: we should walk to 1 and return 0.
  missing.insert(4);
  missing.insert(3);
  missing.insert(2);
  EXPECT_EQ(0u, QuicFramer::CalculateLargestReceived(missing, missing.find(7)));

  // If we add a gap after 7-9, we will return that.
  missing.insert(11);
  EXPECT_EQ(10u, QuicFramer::CalculateLargestReceived(missing,
                                                      missing.find(7)));
  EXPECT_EQ(10u, QuicFramer::CalculateLargestReceived(missing,
                                                      missing.find(8)));
  EXPECT_EQ(10u, QuicFramer::CalculateLargestReceived(missing,
                                                      missing.find(9)));
}

TEST_F(QuicFramerTest, Truncation) {
  QuicPacketHeader header;
  header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.packet_sequence_number = GG_UINT64_C(0x123456789ABC);
  header.flags = PACKET_FLAGS_NONE;
  header.fec_group = 0;

  QuicConnectionCloseFrame close_frame;
  QuicAckFrame* ack_frame = &close_frame.ack_frame;
  close_frame.error_code = static_cast<QuicErrorCode>(0x05060708);
  close_frame.error_details = "because I can";
  ack_frame->received_info.largest_received = 201;
  for (uint64 i = 1; i < ack_frame->received_info.largest_received; ++i) {
    ack_frame->received_info.missing_packets.insert(i);
  }

  // Create a packet with just the ack
  QuicFrame frame;
  frame.type = ACK_FRAME;
  frame.ack_frame = ack_frame;
  QuicFrames frames;
  frames.push_back(frame);

  scoped_ptr<QuicPacket> raw_ack_packet(
      framer_.ConstructFrameDataPacket(header, frames));
  ASSERT_TRUE(raw_ack_packet != NULL);

  scoped_ptr<QuicEncryptedPacket> ack_packet(
      framer_.EncryptPacket(*raw_ack_packet));

  // Create a packet with just connection close.
  frames.clear();
  frame.type = CONNECTION_CLOSE_FRAME;
  frame.connection_close_frame = &close_frame;
  frames.push_back(frame);

  scoped_ptr<QuicPacket> raw_close_packet(
      framer_.ConstructFrameDataPacket(header, frames));
  ASSERT_TRUE(raw_close_packet != NULL);

  scoped_ptr<QuicEncryptedPacket> close_packet(
      framer_.EncryptPacket(*raw_close_packet));

  // Now make sure we can turn our ack packet back into an ack frame
  ASSERT_TRUE(framer_.ProcessPacket(self_address_, peer_address_, *ack_packet));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  ASSERT_EQ(peer_address_, visitor_.peer_address_);
  ASSERT_EQ(self_address_, visitor_.self_address_);
  EXPECT_EQ(1u, visitor_.ack_frames_.size());

  // And do the same for the close frame.
  ASSERT_TRUE(framer_.ProcessPacket(self_address_, peer_address_,
                                    *close_packet));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  EXPECT_EQ(0x05060708, visitor_.connection_close_frame_.error_code);
  EXPECT_EQ("because I can", visitor_.connection_close_frame_.error_details);

  ValidateTruncatedAck(visitor_.ack_frames_[0], 190);
  ValidateTruncatedAck(&visitor_.connection_close_frame_.ack_frame, 180);
}

}  // namespace test
}  // namespace net
