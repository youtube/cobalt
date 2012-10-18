// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
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

using base::hash_set;
using base::StringPiece;
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

// The offset of congestion info in our tests, given the size of our usual ack
// fragment.  This does NOT work for all packets.
const int kCongestionInfoOffset = kPacketHeaderSize + 54;

class TestQuicVisitor : public ::net::QuicFramerVisitorInterface {
 public:
  TestQuicVisitor()
      : error_count_(0),
        packet_count_(0),
        fragment_count_(0),
        fec_count_(0),
        complete_packets_(0),
        accept_packet_(true) {
  }

  ~TestQuicVisitor() {
    STLDeleteElements(&stream_fragments_);
    STLDeleteElements(&ack_fragments_);
    STLDeleteElements(&fec_data_);
  }

  virtual void OnError(QuicFramer* f) {
    DLOG(INFO) << "QuicFramer Error: " << QuicUtils::ErrorToString(f->error())
               << " (" << f->error() << ")";
    error_count_++;
  }

  virtual void OnPacket(const IPEndPoint& client_address) {
    address_ = client_address;
  }

  virtual bool OnPacketHeader(const QuicPacketHeader& header) {
    packet_count_++;
    header_.reset(new QuicPacketHeader(header));
    return accept_packet_;
  }

  virtual void OnStreamFragment(const QuicStreamFragment& fragment) {
    fragment_count_++;
    stream_fragments_.push_back(new QuicStreamFragment(fragment));
  }

  virtual void OnFecProtectedPayload(StringPiece payload) {
    fec_protected_payload_ = payload.as_string();
  }

  virtual void OnAckFragment(const QuicAckFragment& fragment) {
    fragment_count_++;
    ack_fragments_.push_back(new QuicAckFragment(fragment));
  }

  virtual void OnFecData(const QuicFecData& fec) {
    fec_count_++;
    fec_data_.push_back(new QuicFecData(fec));
  }

  virtual void OnPacketComplete() {
    complete_packets_++;
  }

  virtual void OnRstStreamFragment(const QuicRstStreamFragment& fragment) {
    rst_stream_fragment_ = fragment;
  }

  virtual void OnConnectionCloseFragment(
      const QuicConnectionCloseFragment& fragment) {
    connection_close_fragment_ = fragment;
  }

  // Counters from the visitor_ callbacks.
  int error_count_;
  int packet_count_;
  int fragment_count_;
  int fec_count_;
  int complete_packets_;
  bool accept_packet_;

  IPEndPoint address_;
  scoped_ptr<QuicPacketHeader> header_;
  vector<QuicStreamFragment*> stream_fragments_;
  vector<QuicAckFragment*> ack_fragments_;
  vector<QuicFecData*> fec_data_;
  string fec_protected_payload_;
  QuicRstStreamFragment rst_stream_fragment_;
  QuicConnectionCloseFragment connection_close_fragment_;
};

class QuicFramerTest : public ::testing::Test {
 public:
  QuicFramerTest()
      : encrypter_(new test::TestEncrypter()),
        decrypter_(new test::TestDecrypter()),
        framer_(decrypter_, encrypter_) {
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

  test::TestEncrypter* encrypter_;
  test::TestDecrypter* decrypter_;
  QuicFramer framer_;
  test::TestQuicVisitor visitor_;
  IPEndPoint address_;
};

TEST_F(QuicFramerTest, EmptyPacket) {
  char packet[] = { 0x00 };
  EXPECT_FALSE(framer_.ProcessPacket(address_,
                                     QuicEncryptedPacket(packet, 0, false)));
  EXPECT_EQ(QUIC_INVALID_PACKET_HEADER, framer_.error());
}

TEST_F(QuicFramerTest, LargePacket) {
  unsigned char packet[kMaxPacketSize + 1] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // retransmission count
    0x01,
    // transmission time
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0xF0,
    // flags
    0x00,
    // fec group
    0x00,
    // fragment count
    0x01,
  };

  memset(packet + kPacketHeaderSize, 0, kMaxPacketSize - kPacketHeaderSize + 1);

  EXPECT_FALSE(framer_.ProcessPacket(
      address_, QuicEncryptedPacket(AsChars(packet),
                                    arraysize(packet), false)));

  ASSERT_TRUE(visitor_.header_.get());
  // Make sure we've parsed the packet header, so we can send an error.
  EXPECT_EQ(GG_UINT64_C(0xFEDCBA9876543210), visitor_.header_->guid);
  // Make sure the correct error is propogated.
  EXPECT_EQ(QUIC_PACKET_TOO_LARGE, framer_.error());
}

TEST_F(QuicFramerTest, PacketHeader) {
  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // retransmission count
    0x01,
    // transmission time
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0xF0,
    // flags
    0x00,
    // fec group
    0x00,
  };

  EXPECT_FALSE(framer_.ProcessPacket(
      address_, QuicEncryptedPacket(AsChars(packet),
                                    arraysize(packet), false)));

  EXPECT_EQ(QUIC_INVALID_FRAGMENT_DATA, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(GG_UINT64_C(0xFEDCBA9876543210), visitor_.header_->guid);
  EXPECT_EQ(0x1, visitor_.header_->retransmission_count);
  EXPECT_EQ(GG_UINT64_C(0x123456789ABC),
            visitor_.header_->packet_sequence_number);
  EXPECT_EQ(GG_UINT64_C(0xF0E1D2C3B4A59687),
            visitor_.header_->transmission_time);
  EXPECT_EQ(0x00, visitor_.header_->flags);
  EXPECT_EQ(0x00, visitor_.header_->fec_group);

  // Now test framing boundaries
  for (int i = 0; i < 25; ++i) {
    string expected_error;
    if (i < 8) {
      expected_error = "Unable to read GUID.";
    } else if (i < 14) {
      expected_error = "Unable to read sequence number.";
    } else if (i < 15) {
      expected_error = "Unable to read retransmission count.";
    } else if (i < 23) {
      expected_error = "Unable to read transmission time.";
    } else if (i < 24) {
      expected_error = "Unable to read flags.";
    } else if (i < 25) {
      expected_error = "Unable to read fec group.";
    }

    EXPECT_FALSE(framer_.ProcessPacket(
        address_, QuicEncryptedPacket(AsChars(packet), i, false)));
    EXPECT_EQ(expected_error, framer_.detailed_error());
    EXPECT_EQ(QUIC_INVALID_PACKET_HEADER, framer_.error());
  }
}

TEST_F(QuicFramerTest, StreamFragment) {
  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // retransmission count
    0x01,
    // transmission time
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0xF0,
    // flags
    0x00,
    // fec group
    0x00,

    // fragment count
    0x01,
    // fragment type (stream fragment)
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

  EXPECT_TRUE(framer_.ProcessPacket(
      address_, QuicEncryptedPacket(AsChars(packet),
                                    arraysize(packet), false)));

  EXPECT_TRUE(CheckDecryption(StringPiece(AsChars(packet), arraysize(packet))));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  ASSERT_EQ(address_, visitor_.address_);

  ASSERT_EQ(1u, visitor_.stream_fragments_.size());
  EXPECT_EQ(0u, visitor_.ack_fragments_.size());
  EXPECT_EQ(static_cast<uint64>(0x01020304),
            visitor_.stream_fragments_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_fragments_[0]->fin);
  EXPECT_EQ(GG_UINT64_C(0xBA98FEDC32107654),
            visitor_.stream_fragments_[0]->offset);
  EXPECT_EQ("hello world!", visitor_.stream_fragments_[0]->data);

  // Now test framing boundaries
  for (size_t i = kPacketHeaderSize; i < kPacketHeaderSize + 29; ++i) {
    string expected_error;
    if (i < kPacketHeaderSize + 1) {
      expected_error = "Unable to read fragment count.";
    } else if (i < kPacketHeaderSize + 2) {
      expected_error = "Unable to read fragment type.";
    } else if (i < kPacketHeaderSize + 6) {
      expected_error = "Unable to read stream_id.";
    } else if (i < kPacketHeaderSize + 7) {
      expected_error = "Unable to read fin.";
    } else if (i < kPacketHeaderSize + 15) {
      expected_error = "Unable to read offset.";
    } else if (i < kPacketHeaderSize + 29) {
      expected_error = "Unable to read fragment data.";
    }

    EXPECT_FALSE(framer_.ProcessPacket(
        address_, QuicEncryptedPacket(AsChars(packet), i, false)));
    EXPECT_EQ(expected_error, framer_.detailed_error());
    EXPECT_EQ(QUIC_INVALID_FRAGMENT_DATA, framer_.error());
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
    // retransmission count
    0x01,
    // transmission time
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0xF0,
    // flags
    0x00,
    // fec group
    0x00,

    // fragment count
    0x01,
    // fragment type (stream fragment)
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

  EXPECT_TRUE(framer_.ProcessPacket(
      address_, QuicEncryptedPacket(AsChars(packet),
                                    arraysize(packet), false)));

  EXPECT_TRUE(CheckDecryption(StringPiece(AsChars(packet), arraysize(packet))));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  ASSERT_EQ(address_, visitor_.address_);

  ASSERT_EQ(0u, visitor_.stream_fragments_.size());
  EXPECT_EQ(0u, visitor_.ack_fragments_.size());
}

TEST_F(QuicFramerTest, RevivedStreamFragment) {
  unsigned char payload[] = {
    // fragment count
    0x01,
    // fragment type (stream fragment)
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
  header.retransmission_count = 0x01;
  header.packet_sequence_number = GG_UINT64_C(0x123456789ABC);
  header.transmission_time = GG_UINT64_C(0xF0E1D2C3B4A59687);
  header.flags = PACKET_FLAGS_NONE;
  header.fec_group = 0;

  // Do not encrypt the payload because the revived payload is post-encryption.
  EXPECT_TRUE(framer_.ProcessRevivedPacket(address_,
                                           header,
                                           StringPiece(AsChars(payload),
                                                       arraysize(payload))));

  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_EQ(address_, visitor_.address_);
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(GG_UINT64_C(0xFEDCBA9876543210), visitor_.header_->guid);
  EXPECT_EQ(0x1, visitor_.header_->retransmission_count);
  EXPECT_EQ(GG_UINT64_C(0x123456789ABC),
            visitor_.header_->packet_sequence_number);
  EXPECT_EQ(GG_UINT64_C(0xF0E1D2C3B4A59687),
            visitor_.header_->transmission_time);
  EXPECT_EQ(0x00, visitor_.header_->flags);
  EXPECT_EQ(0x00, visitor_.header_->fec_group);


  ASSERT_EQ(1u, visitor_.stream_fragments_.size());
  EXPECT_EQ(0u, visitor_.ack_fragments_.size());
  EXPECT_EQ(GG_UINT64_C(0x01020304), visitor_.stream_fragments_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_fragments_[0]->fin);
  EXPECT_EQ(GG_UINT64_C(0xBA98FEDC32107654),
            visitor_.stream_fragments_[0]->offset);
  EXPECT_EQ("hello world!", visitor_.stream_fragments_[0]->data);
}

TEST_F(QuicFramerTest, StreamFragmentInFecGroup) {
  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x12, 0x34,
    // retransmission count
    0x01,
    // transmission time
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0xF0,
    // flags
    0x00,
    // fec group
    0x02,

    // fragment count
    0x01,
    // fragment type (stream fragment)
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

  EXPECT_TRUE(framer_.ProcessPacket(
      address_, QuicEncryptedPacket(AsChars(packet),
                                    arraysize(packet), false)));

  EXPECT_TRUE(CheckDecryption(StringPiece(AsChars(packet), arraysize(packet))));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  EXPECT_EQ(2, visitor_.header_->fec_group);
  EXPECT_EQ(string(AsChars(packet) + kStartOfFecProtectedData,
                   arraysize(packet) - kStartOfFecProtectedData),
            visitor_.fec_protected_payload_);
  ASSERT_EQ(address_, visitor_.address_);

  ASSERT_EQ(1u, visitor_.stream_fragments_.size());
  EXPECT_EQ(0u, visitor_.ack_fragments_.size());
  EXPECT_EQ(GG_UINT64_C(0x01020304), visitor_.stream_fragments_[0]->stream_id);
  EXPECT_TRUE(visitor_.stream_fragments_[0]->fin);
  EXPECT_EQ(GG_UINT64_C(0xBA98FEDC32107654),
            visitor_.stream_fragments_[0]->offset);
  EXPECT_EQ("hello world!", visitor_.stream_fragments_[0]->data);
}

TEST_F(QuicFramerTest, AckFragment) {
  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // retransmission count
    0x01,
    // transmission time
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0xF0,
    // flags
    0x00,
    // fec group
    0x00,

    // fragment count
    0x01,
    // fragment type (ack fragment)
    0x02,
    // largest received packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // time delta
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0xF0,
    // num_unacked_packets
    0x02,
    // unacked packet sequence number
    0xBA, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // unacked packet sequence number
    0xBB, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // least packet sequence number awaiting an ack
    0xA0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // num non retransmitting packets
    0x03,
    // non retransmitting packet sequence number
    0xAE, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // non retransmitting packet sequence number
    0xAF, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // non retransmitting packet sequence number
    0xB0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // congestion feedback type (none)
    0x00,
  };

  EXPECT_TRUE(framer_.ProcessPacket(
      address_, QuicEncryptedPacket(AsChars(packet),
                                    arraysize(packet), false)));

  EXPECT_TRUE(CheckDecryption(StringPiece(AsChars(packet), arraysize(packet))));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());

  EXPECT_EQ(0u, visitor_.stream_fragments_.size());
  ASSERT_EQ(1u, visitor_.ack_fragments_.size());
  const QuicAckFragment& fragment = *visitor_.ack_fragments_[0];
  EXPECT_EQ(GG_UINT64_C(0x0123456789ABC),
            fragment.received_info.largest_received);
  EXPECT_EQ(GG_UINT64_C(0xF0E1D2C3B4A59687),
            fragment.received_info.time_received);

  const hash_set<QuicPacketSequenceNumber>* sequence_nums =
      &fragment.received_info.missing_packets;
  ASSERT_EQ(2u, sequence_nums->size());
  EXPECT_EQ(1u, sequence_nums->count(GG_UINT64_C(0x0123456789ABB)));
  EXPECT_EQ(1u, sequence_nums->count(GG_UINT64_C(0x0123456789ABA)));
  EXPECT_EQ(GG_UINT64_C(0x0123456789AA0), fragment.sent_info.least_unacked);
  ASSERT_EQ(3u, fragment.sent_info.non_retransmiting.size());
  const hash_set<QuicPacketSequenceNumber>* non_retrans =
      &fragment.sent_info.non_retransmiting;
  EXPECT_EQ(1u, non_retrans->count(GG_UINT64_C(0x0123456789AB0)));
  EXPECT_EQ(1u, non_retrans->count(GG_UINT64_C(0x0123456789AAF)));
  EXPECT_EQ(1u, non_retrans->count(GG_UINT64_C(0x0123456789AAE)));
  ASSERT_EQ(kNone, fragment.congestion_info.type);

  // Now test framing boundaries
  for (size_t i = kPacketHeaderSize; i < kPacketHeaderSize + 55; ++i) {
    string expected_error;
    if (i < kPacketHeaderSize + 1) {
      expected_error = "Unable to read fragment count.";
    } else if (i < kPacketHeaderSize + 2) {
      expected_error = "Unable to read fragment type.";
    } else if (i < kPacketHeaderSize + 8) {
      expected_error = "Unable to read largest received.";
    } else if (i < kPacketHeaderSize + 16) {
      expected_error = "Unable to read time received.";
    } else if (i < kPacketHeaderSize + 17) {
      expected_error = "Unable to read num unacked packets.";
    } else if (i < kPacketHeaderSize + 29) {
      expected_error = "Unable to read sequence number in unacked packets.";
    } else if (i < kPacketHeaderSize + 35) {
      expected_error = "Unable to read least unacked.";
    } else if (i < kPacketHeaderSize + 36) {
      expected_error = "Unable to read num non-retransmitting.";
    } else if (i < kPacketHeaderSize + 54) {
      expected_error = "Unable to read sequence number in non-retransmitting.";
    } else if (i < kPacketHeaderSize + 55) {
      expected_error = "Unable to read congestion info type.";
    }

    EXPECT_FALSE(framer_.ProcessPacket(
        address_, QuicEncryptedPacket(AsChars(packet), i, false)));
    EXPECT_EQ(expected_error, framer_.detailed_error());
    EXPECT_EQ(QUIC_INVALID_FRAGMENT_DATA, framer_.error());
  }
}

TEST_F(QuicFramerTest, AckFragmentTCP) {
  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // retransmission count
    0x01,
    // transmission time
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0xF0,
    // flags
    0x00,
    // fec group
    0x00,

    // fragment count
    0x01,
    // fragment type (ack fragment)
    0x02,
    // largest received packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // time delta
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0xF0,
    // num_unacked_packets
    0x02,
    // unacked packet sequence number
    0xBA, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // unacked packet sequence number
    0xBB, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // least packet sequence number awaiting an ack
    0xA0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // num non retransmitting packets
    0x03,
    // non retransmitting packet sequence number
    0xAE, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // non retransmitting packet sequence number
    0xAF, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // non retransmitting packet sequence number
    0xB0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // congestion feedback type (tcp)
    0x01,
    // ack_fragment.congestion_info.tcp.accumulated_number_of_lost_packets
    0x01, 0x02,
    // ack_fragment.congestion_info.tcp.receive_window
    0x03, 0x04,
  };

  EXPECT_TRUE(framer_.ProcessPacket(
      address_, QuicEncryptedPacket(AsChars(packet),
                                    arraysize(packet), false)));

  EXPECT_TRUE(CheckDecryption(StringPiece(AsChars(packet), arraysize(packet))));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());

  EXPECT_EQ(0u, visitor_.stream_fragments_.size());
  ASSERT_EQ(1u, visitor_.ack_fragments_.size());
  const QuicAckFragment& fragment = *visitor_.ack_fragments_[0];
  EXPECT_EQ(GG_UINT64_C(0x0123456789ABC),
            fragment.received_info.largest_received);
  EXPECT_EQ(GG_UINT64_C(0xF0E1D2C3B4A59687),
            fragment.received_info.time_received);

  const hash_set<QuicPacketSequenceNumber>* sequence_nums =
      &fragment.received_info.missing_packets;
  ASSERT_EQ(2u, sequence_nums->size());
  EXPECT_EQ(1u, sequence_nums->count(GG_UINT64_C(0x0123456789ABB)));
  EXPECT_EQ(1u, sequence_nums->count(GG_UINT64_C(0x0123456789ABA)));
  EXPECT_EQ(GG_UINT64_C(0x0123456789AA0), fragment.sent_info.least_unacked);
  ASSERT_EQ(3u, fragment.sent_info.non_retransmiting.size());
  const hash_set<QuicPacketSequenceNumber>* non_retrans =
      &fragment.sent_info.non_retransmiting;
  EXPECT_EQ(1u, non_retrans->count(GG_UINT64_C(0x0123456789AB0)));
  EXPECT_EQ(1u, non_retrans->count(GG_UINT64_C(0x0123456789AAF)));
  EXPECT_EQ(1u, non_retrans->count(GG_UINT64_C(0x0123456789AAE)));
  ASSERT_EQ(kTCP, fragment.congestion_info.type);
  EXPECT_EQ(0x0201,
            fragment.congestion_info.tcp.accumulated_number_of_lost_packets);
  EXPECT_EQ(0x0403, fragment.congestion_info.tcp.receive_window);

  // Now test framing boundaries
  for (size_t i = kCongestionInfoOffset; i < kCongestionInfoOffset + 5; ++i) {
    string expected_error;
    if (i < kCongestionInfoOffset + 1) {
      expected_error = "Unable to read congestion info type.";
    } else if (i < kCongestionInfoOffset + 3) {
      expected_error = "Unable to read accumulated number of lost packets.";
    } else if (i < kCongestionInfoOffset + 5) {
      expected_error = "Unable to read receive window.";
    }

    EXPECT_FALSE(framer_.ProcessPacket(
        address_, QuicEncryptedPacket(AsChars(packet), i, false)));
    EXPECT_EQ(expected_error, framer_.detailed_error());
    EXPECT_EQ(QUIC_INVALID_FRAGMENT_DATA, framer_.error());
  }
}

TEST_F(QuicFramerTest, AckFragmentInterArrival) {
  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // retransmission count
    0x01,
    // transmission time
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0xF0,
    // flags
    0x00,
    // fec group
    0x00,

    // fragment count
    0x01,
    // fragment type (ack fragment)
    0x02,
    // largest received packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // time delta
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0xF0,
    // num_unacked_packets
    0x02,
    // unacked packet sequence number
    0xBA, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // unacked packet sequence number
    0xBB, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // least packet sequence number awaiting an ack
    0xA0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // num non retransmitting packets
    0x03,
    // non retransmitting packet sequence number
    0xAE, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // non retransmitting packet sequence number
    0xAF, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // non retransmitting packet sequence number
    0xB0, 0x9A, 0x78, 0x56,
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

  EXPECT_TRUE(framer_.ProcessPacket(
      address_, QuicEncryptedPacket(AsChars(packet),
                                    arraysize(packet), false)));

  EXPECT_TRUE(CheckDecryption(StringPiece(AsChars(packet), arraysize(packet))));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());

  EXPECT_EQ(0u, visitor_.stream_fragments_.size());
  ASSERT_EQ(1u, visitor_.ack_fragments_.size());
  const QuicAckFragment& fragment = *visitor_.ack_fragments_[0];
  EXPECT_EQ(GG_UINT64_C(0x0123456789ABC),
            fragment.received_info.largest_received);
  EXPECT_EQ(GG_UINT64_C(0xF0E1D2C3B4A59687),
            fragment.received_info.time_received);

  const hash_set<QuicPacketSequenceNumber>* sequence_nums =
      &fragment.received_info.missing_packets;
  ASSERT_EQ(2u, sequence_nums->size());
  EXPECT_EQ(1u, sequence_nums->count(GG_UINT64_C(0x0123456789ABB)));
  EXPECT_EQ(1u, sequence_nums->count(GG_UINT64_C(0x0123456789ABA)));
  EXPECT_EQ(GG_UINT64_C(0x0123456789AA0), fragment.sent_info.least_unacked);
  ASSERT_EQ(3u, fragment.sent_info.non_retransmiting.size());
  const hash_set<QuicPacketSequenceNumber>* non_retrans =
      &fragment.sent_info.non_retransmiting;
  EXPECT_EQ(1u, non_retrans->count(GG_UINT64_C(0x0123456789AB0)));
  EXPECT_EQ(1u, non_retrans->count(GG_UINT64_C(0x0123456789AAF)));
  EXPECT_EQ(1u, non_retrans->count(GG_UINT64_C(0x0123456789AAE)));
  ASSERT_EQ(kInterArrival, fragment.congestion_info.type);
  EXPECT_EQ(0x0302, fragment.congestion_info.inter_arrival.
            accumulated_number_of_lost_packets);
  EXPECT_EQ(0x0504, fragment.congestion_info.inter_arrival.offset_time);
  EXPECT_EQ(0x0706, fragment.congestion_info.inter_arrival.delta_time);

  // Now test framing boundaries
  for (size_t i = kCongestionInfoOffset; i < kCongestionInfoOffset + 5; ++i) {
    string expected_error;
    if (i < kCongestionInfoOffset + 1) {
      expected_error = "Unable to read congestion info type.";
    } else if (i < kCongestionInfoOffset + 3) {
      expected_error = "Unable to read accumulated number of lost packets.";
    } else if (i < kCongestionInfoOffset + 5) {
      expected_error = "Unable to read offset time.";
    } else if (i < kCongestionInfoOffset + 7) {
      expected_error = "Unable to read delta time.";
    }
    EXPECT_FALSE(framer_.ProcessPacket(
        address_, QuicEncryptedPacket(AsChars(packet), i, false)));
    EXPECT_EQ(expected_error, framer_.detailed_error());
    EXPECT_EQ(QUIC_INVALID_FRAGMENT_DATA, framer_.error());
  }
}

TEST_F(QuicFramerTest, AckFragmentFixRate) {
  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // retransmission count
    0x01,
    // transmission time
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0xF0,
    // flags
    0x00,
    // fec group
    0x00,

    // fragment count
    0x01,
    // fragment type (ack fragment)
    0x02,
    // largest received packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // time delta
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0xF0,
    // num_unacked_packets
    0x02,
    // unacked packet sequence number
    0xBA, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // unacked packet sequence number
    0xBB, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // least packet sequence number awaiting an ack
    0xA0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // num non retransmitting packets
    0x03,
    // non retransmitting packet sequence number
    0xAE, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // non retransmitting packet sequence number
    0xAF, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // non retransmitting packet sequence number
    0xB0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // congestion feedback type (fix rate)
    0x03,
    // bitrate_in_bytes_per_second;
    0x01, 0x02, 0x03, 0x04,
  };

  EXPECT_TRUE(framer_.ProcessPacket(
      address_, QuicEncryptedPacket(AsChars(packet),
                                    arraysize(packet), false)));

  EXPECT_TRUE(CheckDecryption(StringPiece(AsChars(packet), arraysize(packet))));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());

  EXPECT_EQ(0u, visitor_.stream_fragments_.size());
  ASSERT_EQ(1u, visitor_.ack_fragments_.size());
  const QuicAckFragment& fragment = *visitor_.ack_fragments_[0];
  EXPECT_EQ(GG_UINT64_C(0x0123456789ABC),
            fragment.received_info.largest_received);
  EXPECT_EQ(GG_UINT64_C(0xF0E1D2C3B4A59687),
            fragment.received_info.time_received);

  const hash_set<QuicPacketSequenceNumber>* sequence_nums =
      &fragment.received_info.missing_packets;
  ASSERT_EQ(2u, sequence_nums->size());
  EXPECT_EQ(1u, sequence_nums->count(GG_UINT64_C(0x0123456789ABB)));
  EXPECT_EQ(1u, sequence_nums->count(GG_UINT64_C(0x0123456789ABA)));
  EXPECT_EQ(GG_UINT64_C(0x0123456789AA0), fragment.sent_info.least_unacked);
  ASSERT_EQ(3u, fragment.sent_info.non_retransmiting.size());
  const hash_set<QuicPacketSequenceNumber>* non_retrans =
      &fragment.sent_info.non_retransmiting;
  EXPECT_EQ(1u, non_retrans->count(GG_UINT64_C(0x0123456789AB0)));
  EXPECT_EQ(1u, non_retrans->count(GG_UINT64_C(0x0123456789AAF)));
  EXPECT_EQ(1u, non_retrans->count(GG_UINT64_C(0x0123456789AAE)));
  ASSERT_EQ(kFixRate, fragment.congestion_info.type);
  EXPECT_EQ(static_cast<uint32>(0x04030201),
            fragment.congestion_info.fix_rate.bitrate_in_bytes_per_second);

  // Now test framing boundaries
  for (size_t i = kCongestionInfoOffset; i < kCongestionInfoOffset + 5; ++i) {
    string expected_error;
    if (i < kCongestionInfoOffset + 1) {
      expected_error = "Unable to read congestion info type.";
    } else if (i < kCongestionInfoOffset + 5) {
      expected_error = "Unable to read bitrate.";
    }
    EXPECT_FALSE(framer_.ProcessPacket(
        address_, QuicEncryptedPacket(AsChars(packet), i, false)));
    EXPECT_EQ(expected_error, framer_.detailed_error());
    EXPECT_EQ(QUIC_INVALID_FRAGMENT_DATA, framer_.error());
  }
}


TEST_F(QuicFramerTest, AckFragmentInvalidFeedback) {
  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // retransmission count
    0x01,
    // transmission time
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0xF0,
    // flags
    0x00,
    // fec group
    0x00,

    // fragment count
    0x01,
    // fragment type (ack fragment)
    0x02,
    // largest received packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // time delta
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0xF0,
    // num_unacked_packets
    0x02,
    // unacked packet sequence number
    0xBA, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // unacked packet sequence number
    0xBB, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // least packet sequence number awaiting an ack
    0xA0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // num non retransmitting packets
    0x03,
    // non retransmitting packet sequence number
    0xAE, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // non retransmitting packet sequence number
    0xAF, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // non retransmitting packet sequence number
    0xB0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // congestion feedback type (invalid)
    0x04,
  };

  EXPECT_FALSE(framer_.ProcessPacket(
      address_, QuicEncryptedPacket(AsChars(packet),
                                    arraysize(packet), false)));
  EXPECT_TRUE(CheckDecryption(StringPiece(AsChars(packet), arraysize(packet))));
  EXPECT_EQ(QUIC_INVALID_FRAGMENT_DATA, framer_.error());
}

TEST_F(QuicFramerTest, RstStreamFragment) {
  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // retransmission count
    0x01,
    // transmission time
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0xF0,
    // flags
    0x00,
    // fec group
    0x00,

    // fragment count
    0x01,
    // fragment type (rst stream fragment)
    0x03,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // details
    0x08, 0x07, 0x06, 0x05,
  };

  EXPECT_TRUE(framer_.ProcessPacket(
      address_, QuicEncryptedPacket(AsChars(packet),
                                    arraysize(packet), false)));

  EXPECT_TRUE(CheckDecryption(StringPiece(AsChars(packet), arraysize(packet))));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());
  ASSERT_EQ(address_, visitor_.address_);

  EXPECT_EQ(GG_UINT64_C(0x01020304), visitor_.rst_stream_fragment_.stream_id);
  EXPECT_EQ(0x05060708, visitor_.rst_stream_fragment_.details);
  EXPECT_EQ(GG_UINT64_C(0xBA98FEDC32107654),
            visitor_.rst_stream_fragment_.offset);

  // Now test framing boundaries
  for (size_t i = kPacketHeaderSize + 3; i < kPacketHeaderSize + 18; ++i) {
    string expected_error;
    if (i < kPacketHeaderSize + 6) {
      expected_error = "Unable to read stream_id.";
    } else if (i < kPacketHeaderSize + 14) {
      expected_error = "Unable to read offset in rst fragment.";
    } else if (i < kPacketHeaderSize + 18) {
      expected_error = "Unable to read rst stream details.";
    }
    EXPECT_FALSE(framer_.ProcessPacket(
        address_, QuicEncryptedPacket(AsChars(packet), i, false)));
    EXPECT_EQ(expected_error, framer_.detailed_error());
    EXPECT_EQ(QUIC_INVALID_RST_STREAM_DATA, framer_.error());
  }
}

TEST_F(QuicFramerTest, ConnectionCloseFragment) {
  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // retransmission count
    0x01,
    // transmission time
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0xF0,
    // flags
    0x00,
    // fec group
    0x00,


    // fragment count
    0x01,
    // fragment type (connection close fragment)
    0x04,
    // details
    0x08, 0x07, 0x06, 0x05,

    // Ack fragment.

    // largest received packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // time delta
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0xF0,
    // num_unacked_packets
    0x02,
    // unacked packet sequence number
    0xBA, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // unacked packet sequence number
    0xBB, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // least packet sequence number awaiting an ack
    0xA0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // num non retransmitting packets
    0x03,
    // non retransmitting packet sequence number
    0xAE, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // non retransmitting packet sequence number
    0xAF, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // non retransmitting packet sequence number
    0xB0, 0x9A, 0x78, 0x56,
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

  EXPECT_TRUE(framer_.ProcessPacket(
      address_, QuicEncryptedPacket(AsChars(packet),
                                    arraysize(packet), false)));

  EXPECT_TRUE(CheckDecryption(StringPiece(AsChars(packet), arraysize(packet))));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());

  EXPECT_EQ(0u, visitor_.stream_fragments_.size());

  EXPECT_EQ(0x05060708, visitor_.connection_close_fragment_.details);

  ASSERT_EQ(1u, visitor_.ack_fragments_.size());
  const QuicAckFragment& fragment = *visitor_.ack_fragments_[0];
  EXPECT_EQ(GG_UINT64_C(0x0123456789ABC),
            fragment.received_info.largest_received);
  EXPECT_EQ(GG_UINT64_C(0xF0E1D2C3B4A59687),
            fragment.received_info.time_received);

  const hash_set<QuicPacketSequenceNumber>* sequence_nums =
      &fragment.received_info.missing_packets;
  ASSERT_EQ(2u, sequence_nums->size());
  EXPECT_EQ(1u, sequence_nums->count(GG_UINT64_C(0x0123456789ABB)));
  EXPECT_EQ(1u, sequence_nums->count(GG_UINT64_C(0x0123456789ABA)));
  EXPECT_EQ(GG_UINT64_C(0x0123456789AA0), fragment.sent_info.least_unacked);
  ASSERT_EQ(3u, fragment.sent_info.non_retransmiting.size());
  const hash_set<QuicPacketSequenceNumber>* non_retrans =
      &fragment.sent_info.non_retransmiting;
  EXPECT_EQ(1u, non_retrans->count(GG_UINT64_C(0x0123456789AB0)));
  EXPECT_EQ(1u, non_retrans->count(GG_UINT64_C(0x0123456789AAF)));
  EXPECT_EQ(1u, non_retrans->count(GG_UINT64_C(0x0123456789AAE)));
  ASSERT_EQ(kInterArrival, fragment.congestion_info.type);
  EXPECT_EQ(0x0302, fragment.congestion_info.inter_arrival.
            accumulated_number_of_lost_packets);
  EXPECT_EQ(0x0504,
            fragment.congestion_info.inter_arrival.offset_time);
  EXPECT_EQ(0x0706,
            fragment.congestion_info.inter_arrival.delta_time);

  // Now test framing boundaries
  for (size_t i = kPacketHeaderSize + 3; i < kPacketHeaderSize + 6; ++i) {
    string expected_error;
    if (i < kPacketHeaderSize + 6) {
      expected_error = "Unable to read connection close details.";
    }
    EXPECT_FALSE(framer_.ProcessPacket(
        address_, QuicEncryptedPacket(AsChars(packet), i, false)));
    EXPECT_EQ(expected_error, framer_.detailed_error());
    EXPECT_EQ(QUIC_INVALID_CONNECTION_CLOSE_DATA, framer_.error());
  }
}

TEST_F(QuicFramerTest, FecPacket) {
  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // retransmission count
    0x01,
    // transmission time
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0xF0,
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

  EXPECT_TRUE(framer_.ProcessPacket(
      address_, QuicEncryptedPacket(AsChars(packet),
                                    arraysize(packet), false)));

  EXPECT_TRUE(CheckDecryption(StringPiece(AsChars(packet), arraysize(packet))));
  EXPECT_EQ(QUIC_NO_ERROR, framer_.error());
  ASSERT_TRUE(visitor_.header_.get());

  EXPECT_EQ(0u, visitor_.stream_fragments_.size());
  EXPECT_EQ(0u, visitor_.ack_fragments_.size());
  ASSERT_EQ(1, visitor_.fec_count_);
  const QuicFecData& fec_data = *visitor_.fec_data_[0];
  EXPECT_EQ(GG_UINT64_C(0x0123456789ABB),
            fec_data.first_protected_packet_sequence_number);
  EXPECT_EQ("abcdefghijklmnop", fec_data.redundancy);
}

TEST_F(QuicFramerTest, ConstructStreamFragmentPacket) {
  QuicPacketHeader header;
  header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.retransmission_count = 0x01;
  header.packet_sequence_number = GG_UINT64_C(0x123456789ABC);
  header.transmission_time = GG_UINT64_C(0xF0E1D2C3B4A59687);
  header.flags = PACKET_FLAGS_NONE;
  header.fec_group = 0;

  QuicStreamFragment stream_fragment;
  stream_fragment.stream_id = 0x01020304;
  stream_fragment.fin = true;
  stream_fragment.offset = GG_UINT64_C(0xBA98FEDC32107654);
  stream_fragment.data = "hello world!";

  QuicFragment fragment;
  fragment.type = STREAM_FRAGMENT;
  fragment.stream_fragment = &stream_fragment;

  QuicFragments fragments;
  fragments.push_back(fragment);

  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // retransmission count
    0x01,
    // transmission time
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0xF0,
    // flags
    0x00,
    // fec group
    0x00,

    // fragment count
    0x01,
    // fragment type (stream fragment)
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

  QuicPacket* data;
  ASSERT_TRUE(framer_.ConstructFragementDataPacket(header, fragments, &data));

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));

  delete data;
}

TEST_F(QuicFramerTest, ConstructAckFragmentPacket) {
  QuicPacketHeader header;
  header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.retransmission_count = 0x01;
  header.packet_sequence_number = GG_UINT64_C(0x123456789ABC);
  header.transmission_time = GG_UINT64_C(0xF0E1D2C3B4A59687);
  header.flags = PACKET_FLAGS_NONE;
  header.fec_group = 0;

  QuicAckFragment ack_fragment;
  ack_fragment.received_info.largest_received = GG_UINT64_C(0x0123456789ABC);
  ack_fragment.received_info.time_received = GG_UINT64_C(0xF0E1D2C3B4A59687);
  ack_fragment.received_info.missing_packets.insert(
      GG_UINT64_C(0x0123456789ABB));
  ack_fragment.received_info.missing_packets.insert(
      GG_UINT64_C(0x0123456789ABA));
  ack_fragment.sent_info.least_unacked = GG_UINT64_C(0x0123456789AA0);
  ack_fragment.sent_info.non_retransmiting.insert(
      GG_UINT64_C(0x0123456789AB0));
  ack_fragment.sent_info.non_retransmiting.insert(
      GG_UINT64_C(0x0123456789AAF));
  ack_fragment.sent_info.non_retransmiting.insert(
      GG_UINT64_C(0x0123456789AAE));
  ack_fragment.congestion_info.type = kNone;

  QuicFragment fragment;
  fragment.type = ACK_FRAGMENT;
  fragment.ack_fragment = &ack_fragment;

  QuicFragments fragments;
  fragments.push_back(fragment);

  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // retransmission count
    0x01,
    // transmission time
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0xF0,
    // flags
    0x00,
    // fec group
    0x00,

    // fragment count
    0x01,
    // fragment type (ack fragment)
    0x02,
    // largest received packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // time delta
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0xF0,
    // num_unacked_packets
    0x02,
#if defined(OS_WIN)
    // unacked packet sequence number
    0xBB, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // unacked packet sequence number
    0xBA, 0x9A, 0x78, 0x56,
    0x34, 0x12,
#else
    // unacked packet sequence number
    0xBA, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // unacked packet sequence number
    0xBB, 0x9A, 0x78, 0x56,
    0x34, 0x12,
#endif
    // least packet sequence number awaiting an ack
    0xA0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // num non retransmitting packets
    0x03,
#if defined(OS_WIN)
    // non retransmitting packet sequence number
    0xB0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // non retransmitting packet sequence number
    0xAF, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // non retransmitting packet sequence number
    0xAE, 0x9A, 0x78, 0x56,
    0x34, 0x12,
#else
    // non retransmitting packet sequence number
    0xAE, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // non retransmitting packet sequence number
    0xAF, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // non retransmitting packet sequence number
    0xB0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
#endif
    // congestion feedback type (none)
    0x00,
  };

  QuicPacket* data;
  EXPECT_TRUE(framer_.ConstructFragementDataPacket(header, fragments, &data));

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));

  delete data;
}

TEST_F(QuicFramerTest, ConstructAckFragmentPacketTCP) {
  QuicPacketHeader header;
  header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.retransmission_count = 0x01;
  header.packet_sequence_number = GG_UINT64_C(0x123456789ABC);
  header.transmission_time = GG_UINT64_C(0xF0E1D2C3B4A59687);
  header.flags = PACKET_FLAGS_NONE;
  header.fec_group = 0;

  QuicAckFragment ack_fragment;
  ack_fragment.received_info.largest_received = GG_UINT64_C(0x0123456789ABC);
  ack_fragment.received_info.time_received = GG_UINT64_C(0xF0E1D2C3B4A59687);
  ack_fragment.received_info.missing_packets.insert(
      GG_UINT64_C(0x0123456789ABB));
  ack_fragment.received_info.missing_packets.insert(
      GG_UINT64_C(0x0123456789ABA));
  ack_fragment.sent_info.least_unacked = GG_UINT64_C(0x0123456789AA0);
  ack_fragment.sent_info.non_retransmiting.insert(
      GG_UINT64_C(0x0123456789AB0));
  ack_fragment.sent_info.non_retransmiting.insert(
      GG_UINT64_C(0x0123456789AAF));
  ack_fragment.sent_info.non_retransmiting.insert(
      GG_UINT64_C(0x0123456789AAE));
  ack_fragment.congestion_info.type = kTCP;
  ack_fragment.congestion_info.tcp.accumulated_number_of_lost_packets = 0x0201;
  ack_fragment.congestion_info.tcp.receive_window = 0x0403;

  QuicFragment fragment;
  fragment.type = ACK_FRAGMENT;
  fragment.ack_fragment = &ack_fragment;

  QuicFragments fragments;
  fragments.push_back(fragment);

  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // retransmission count
    0x01,
    // transmission time
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0xF0,
    // flags
    0x00,
    // fec group
    0x00,

    // fragment count
    0x01,
    // fragment type (ack fragment)
    0x02,
    // largest received packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // time delta
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0xF0,
    // num_unacked_packets
    0x02,
#if defined(OS_WIN)
    // unacked packet sequence number
    0xBB, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // unacked packet sequence number
    0xBA, 0x9A, 0x78, 0x56,
    0x34, 0x12,
#else
    // unacked packet sequence number
    0xBA, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // unacked packet sequence number
    0xBB, 0x9A, 0x78, 0x56,
    0x34, 0x12,
#endif
    // least packet sequence number awaiting an ack
    0xA0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // num non retransmitting packets
    0x03,
#if defined(OS_WIN)
    // non retransmitting packet sequence number
    0xB0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // non retransmitting packet sequence number
    0xAF, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // non retransmitting packet sequence number
    0xAE, 0x9A, 0x78, 0x56,
    0x34, 0x12,
#else
    // non retransmitting packet sequence number
    0xAE, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // non retransmitting packet sequence number
    0xAF, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // non retransmitting packet sequence number
    0xB0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
#endif
    // congestion feedback type (tcp)
    0x01,
    // ack_fragment.congestion_info.tcp.accumulated_number_of_lost_packets
    0x01, 0x02,
    // ack_fragment.congestion_info.tcp.receive_window
    0x03, 0x04,
  };

  QuicPacket* data;
  EXPECT_TRUE(framer_.ConstructFragementDataPacket(header, fragments, &data));

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));

  delete data;
}

TEST_F(QuicFramerTest, ConstructAckFragmentPacketInterArrival) {
  QuicPacketHeader header;
  header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.retransmission_count = 0x01;
  header.packet_sequence_number = GG_UINT64_C(0x123456789ABC);
  header.transmission_time = GG_UINT64_C(0xF0E1D2C3B4A59687);
  header.flags = PACKET_FLAGS_NONE;
  header.fec_group = 0;

  QuicAckFragment ack_fragment;
  ack_fragment.received_info.largest_received = GG_UINT64_C(0x0123456789ABC);
  ack_fragment.received_info.time_received = GG_UINT64_C(0xF0E1D2C3B4A59687);
  ack_fragment.received_info.missing_packets.insert(
      GG_UINT64_C(0x0123456789ABB));
  ack_fragment.received_info.missing_packets.insert(
      GG_UINT64_C(0x0123456789ABA));
  ack_fragment.sent_info.least_unacked = GG_UINT64_C(0x0123456789AA0);
  ack_fragment.sent_info.non_retransmiting.insert(
      GG_UINT64_C(0x0123456789AB0));
  ack_fragment.sent_info.non_retransmiting.insert(
      GG_UINT64_C(0x0123456789AAF));
  ack_fragment.sent_info.non_retransmiting.insert(
      GG_UINT64_C(0x0123456789AAE));
  ack_fragment.congestion_info.type = kInterArrival;
  ack_fragment.congestion_info.inter_arrival.accumulated_number_of_lost_packets
      = 0x0302;
  ack_fragment.congestion_info.inter_arrival.offset_time = 0x0504;
  ack_fragment.congestion_info.inter_arrival.delta_time = 0x0706;

  QuicFragment fragment;
  fragment.type = ACK_FRAGMENT;
  fragment.ack_fragment = &ack_fragment;

  QuicFragments fragments;
  fragments.push_back(fragment);

  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // retransmission count
    0x01,
    // transmission time
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0xF0,
    // flags
    0x00,
    // fec group
    0x00,

    // fragment count
    0x01,
    // fragment type (ack fragment)
    0x02,
    // largest received packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // time delta
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0xF0,
    // num_unacked_packets
    0x02,
#if defined(OS_WIN)
    // unacked packet sequence number
    0xBB, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // unacked packet sequence number
    0xBA, 0x9A, 0x78, 0x56,
    0x34, 0x12,
#else
    // unacked packet sequence number
    0xBA, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // unacked packet sequence number
    0xBB, 0x9A, 0x78, 0x56,
    0x34, 0x12,
#endif
    // least packet sequence number awaiting an ack
    0xA0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // num non retransmitting packets
    0x03,
#if defined(OS_WIN)
    // non retransmitting packet sequence number
    0xB0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // non retransmitting packet sequence number
    0xAF, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // non retransmitting packet sequence number
    0xAE, 0x9A, 0x78, 0x56,
    0x34, 0x12,
#else
    // non retransmitting packet sequence number
    0xAE, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // non retransmitting packet sequence number
    0xAF, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // non retransmitting packet sequence number
    0xB0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
#endif
    // congestion feedback type (inter arrival)
    0x02,
    // accumulated_number_of_lost_packets
    0x02, 0x03,
    // offset_time
    0x04, 0x05,
    // delta_time
    0x06, 0x07,
  };

  QuicPacket* data;
  EXPECT_TRUE(framer_.ConstructFragementDataPacket(header, fragments, &data));

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));

  delete data;
}

TEST_F(QuicFramerTest, ConstructAckFragmentPacketFixRate) {
  QuicPacketHeader header;
  header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.retransmission_count = 0x01;
  header.packet_sequence_number = GG_UINT64_C(0x123456789ABC);
  header.transmission_time = GG_UINT64_C(0xF0E1D2C3B4A59687);
  header.flags = PACKET_FLAGS_NONE;
  header.fec_group = 0;

  QuicAckFragment ack_fragment;
  ack_fragment.received_info.largest_received = GG_UINT64_C(0x0123456789ABC);
  ack_fragment.received_info.time_received = GG_UINT64_C(0xF0E1D2C3B4A59687);
  ack_fragment.received_info.missing_packets.insert(
      GG_UINT64_C(0x0123456789ABB));
  ack_fragment.received_info.missing_packets.insert(
      GG_UINT64_C(0x0123456789ABA));
  ack_fragment.sent_info.least_unacked = GG_UINT64_C(0x0123456789AA0);
  ack_fragment.sent_info.non_retransmiting.insert(
      GG_UINT64_C(0x0123456789AB0));
  ack_fragment.sent_info.non_retransmiting.insert(
      GG_UINT64_C(0x0123456789AAF));
  ack_fragment.sent_info.non_retransmiting.insert(
      GG_UINT64_C(0x0123456789AAE));
  ack_fragment.congestion_info.type = kFixRate;
  ack_fragment.congestion_info.fix_rate.bitrate_in_bytes_per_second
      = 0x04030201;

  QuicFragment fragment;
  fragment.type = ACK_FRAGMENT;
  fragment.ack_fragment = &ack_fragment;

  QuicFragments fragments;
  fragments.push_back(fragment);

  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // retransmission count
    0x01,
    // transmission time
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0xF0,
    // flags
    0x00,
    // fec group
    0x00,

    // fragment count
    0x01,
    // fragment type (ack fragment)
    0x02,
    // largest received packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // time delta
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0xF0,
    // num_unacked_packets
    0x02,
#if defined(OS_WIN)
    // unacked packet sequence number
    0xBB, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // unacked packet sequence number
    0xBA, 0x9A, 0x78, 0x56,
    0x34, 0x12,
#else
    // unacked packet sequence number
    0xBA, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // unacked packet sequence number
    0xBB, 0x9A, 0x78, 0x56,
    0x34, 0x12,
#endif
    // least packet sequence number awaiting an ack
    0xA0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // num non retransmitting packets
    0x03,
#if defined(OS_WIN)
    // non retransmitting packet sequence number
    0xB0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // non retransmitting packet sequence number
    0xAF, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // non retransmitting packet sequence number
    0xAE, 0x9A, 0x78, 0x56,
    0x34, 0x12,
#else
    // non retransmitting packet sequence number
    0xAE, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // non retransmitting packet sequence number
    0xAF, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // non retransmitting packet sequence number
    0xB0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
#endif
    // congestion feedback type (fix rate)
    0x03,
    // bitrate_in_bytes_per_second;
    0x01, 0x02, 0x03, 0x04,
  };

  QuicPacket* data;
  EXPECT_TRUE(framer_.ConstructFragementDataPacket(header, fragments, &data));

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));

  delete data;
}

TEST_F(QuicFramerTest, ConstructAckFragmentPacketInvalidFeedback) {
  QuicPacketHeader header;
  header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.retransmission_count = 0x01;
  header.packet_sequence_number = GG_UINT64_C(0x123456789ABC);
  header.transmission_time = GG_UINT64_C(0xF0E1D2C3B4A59687);
  header.flags = PACKET_FLAGS_NONE;
  header.fec_group = 0;

  QuicAckFragment ack_fragment;
  ack_fragment.received_info.largest_received = GG_UINT64_C(0x0123456789ABC);
  ack_fragment.received_info.time_received = GG_UINT64_C(0xF0E1D2C3B4A59687);
  ack_fragment.received_info.missing_packets.insert(
      GG_UINT64_C(0x0123456789ABB));
  ack_fragment.received_info.missing_packets.insert(
      GG_UINT64_C(0x0123456789ABA));
  ack_fragment.sent_info.least_unacked = GG_UINT64_C(0x0123456789AA0);
  ack_fragment.sent_info.non_retransmiting.insert(
      GG_UINT64_C(0x0123456789AB0));
  ack_fragment.sent_info.non_retransmiting.insert(
      GG_UINT64_C(0x0123456789AAF));
  ack_fragment.sent_info.non_retransmiting.insert(
      GG_UINT64_C(0x0123456789AAE));
  ack_fragment.congestion_info.type =
      static_cast<CongestionFeedbackType>(kFixRate + 1);

  QuicFragment fragment;
  fragment.type = ACK_FRAGMENT;
  fragment.ack_fragment = &ack_fragment;

  QuicFragments fragments;
  fragments.push_back(fragment);

  QuicPacket* data;
  EXPECT_FALSE(framer_.ConstructFragementDataPacket(header, fragments, &data));
}

TEST_F(QuicFramerTest, ConstructRstFragmentPacket) {
  QuicPacketHeader header;
  header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.retransmission_count = 0x01;
  header.packet_sequence_number = GG_UINT64_C(0x123456789ABC);
  header.transmission_time = GG_UINT64_C(0xF0E1D2C3B4A59687);
  header.flags = PACKET_FLAGS_NONE;
  header.fec_group = 0;

  QuicRstStreamFragment rst_fragment;
  rst_fragment.stream_id = 0x01020304;
  rst_fragment.details = static_cast<QuicErrorCode>(0x05060708);
  rst_fragment.offset = GG_UINT64_C(0xBA98FEDC32107654);

  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // retransmission count
    0x01,
    // transmission time
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0xF0,
    // flags
    0x00,
    // fec group
    0x00,

    // fragment count
    0x01,
    // fragment type (rst stream fragment)
    0x03,
    // stream id
    0x04, 0x03, 0x02, 0x01,
    // offset
    0x54, 0x76, 0x10, 0x32,
    0xDC, 0xFE, 0x98, 0xBA,
    // details
    0x08, 0x07, 0x06, 0x05,
  };

  QuicFragment fragment(&rst_fragment);

  QuicFragments fragments;
  fragments.push_back(fragment);

  QuicPacket* data;
  EXPECT_TRUE(framer_.ConstructFragementDataPacket(header, fragments, &data));

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));

  delete data;
}

TEST_F(QuicFramerTest, ConstructCloseFragmentPacket) {
  QuicPacketHeader header;
  header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.retransmission_count = 0x01;
  header.packet_sequence_number = GG_UINT64_C(0x123456789ABC);
  header.transmission_time = GG_UINT64_C(0xF0E1D2C3B4A59687);
  header.flags = PACKET_FLAGS_NONE;
  header.fec_group = 0;

  QuicConnectionCloseFragment close_fragment;
  close_fragment.details = static_cast<QuicErrorCode>(0x05060708);

  QuicAckFragment* ack_fragment = &close_fragment.ack_fragment;
  ack_fragment->received_info.largest_received = GG_UINT64_C(0x0123456789ABC);
  ack_fragment->received_info.time_received = GG_UINT64_C(0xF0E1D2C3B4A59687);
  ack_fragment->received_info.missing_packets.insert(
      GG_UINT64_C(0x0123456789ABB));
  ack_fragment->received_info.missing_packets.insert(
      GG_UINT64_C(0x0123456789ABA));
  ack_fragment->sent_info.least_unacked = GG_UINT64_C(0x0123456789AA0);
  ack_fragment->sent_info.non_retransmiting.insert(
      GG_UINT64_C(0x0123456789AB0));
  ack_fragment->sent_info.non_retransmiting.insert(
      GG_UINT64_C(0x0123456789AAF));
  ack_fragment->sent_info.non_retransmiting.insert(
      GG_UINT64_C(0x0123456789AAE));
  ack_fragment->congestion_info.type = kInterArrival;
  ack_fragment->congestion_info.inter_arrival.accumulated_number_of_lost_packets
      = 0x0302;
  ack_fragment->congestion_info.inter_arrival.offset_time = 0x0504;
  ack_fragment->congestion_info.inter_arrival.delta_time = 0x0706;

  QuicFragment fragment(&close_fragment);

  QuicFragments fragments;
  fragments.push_back(fragment);

  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // retransmission count
    0x01,
    // transmission time
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0xF0,
    // flags
    0x00,
    // fec group
    0x00,

    // fragment count
    0x01,
    // fragment type (connection close fragment)
    0x04,
    // details
    0x08, 0x07, 0x06, 0x05,

    // Ack fragment.

    // largest received packet sequence number
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // time delta
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0xF0,
    // num_unacked_packets
    0x02,
#if defined(OS_WIN)
    // unacked packet sequence number
    0xBB, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // unacked packet sequence number
    0xBA, 0x9A, 0x78, 0x56,
    0x34, 0x12,
#else
    // unacked packet sequence number
    0xBA, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // unacked packet sequence number
    0xBB, 0x9A, 0x78, 0x56,
    0x34, 0x12,
#endif
    // least packet sequence number awaiting an ack
    0xA0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // num non retransmitting packets
    0x03,
#if defined(OS_WIN)
    // non retransmitting packet sequence number
    0xB0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // non retransmitting packet sequence number
    0xAF, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // non retransmitting packet sequence number
    0xAE, 0x9A, 0x78, 0x56,
    0x34, 0x12,
#else
    // non retransmitting packet sequence number
    0xAE, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // non retransmitting packet sequence number
    0xAF, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // non retransmitting packet sequence number
    0xB0, 0x9A, 0x78, 0x56,
    0x34, 0x12,
#endif
    // congestion feedback type (inter arrival)
    0x02,
    // accumulated_number_of_lost_packets
    0x02, 0x03,
    // offset_time
    0x04, 0x05,
    // delta_time
    0x06, 0x07,
  };

  QuicPacket* data;
  EXPECT_TRUE(framer_.ConstructFragementDataPacket(header, fragments, &data));

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));

  delete data;
}

TEST_F(QuicFramerTest, ConstructFecPacket) {
  QuicPacketHeader header;
  header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.retransmission_count = 0x01;
  header.packet_sequence_number = (GG_UINT64_C(0x123456789ABC));
  header.transmission_time = GG_UINT64_C(0xF0E1D2C3B4A59687);
  header.flags = PACKET_FLAGS_FEC;
  header.fec_group = 1;

  QuicFecData fec_data;
  fec_data.fec_group = 1;
  fec_data.first_protected_packet_sequence_number =
      GG_UINT64_C(0x123456789ABB);
  fec_data.redundancy = "abcdefghijklmnop";

  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // retransmission count
    0x01,
    // transmission time
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0xF0,
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

  QuicPacket* data;
  EXPECT_TRUE(framer_.ConstructFecPacket(header, fec_data, &data));

  test::CompareCharArraysWithHexError("constructed packet",
                                      data->data(), data->length(),
                                      AsChars(packet), arraysize(packet));

  delete data;
}

TEST_F(QuicFramerTest, IncrementRetransmitCount) {
  QuicPacketHeader header;
  header.guid = GG_UINT64_C(0xFEDCBA9876543210);
  header.retransmission_count = 1;
  header.packet_sequence_number = GG_UINT64_C(0x123456789ABC);
  header.transmission_time = GG_UINT64_C(0xF0E1D2C3B4A59687);
  header.flags = PACKET_FLAGS_NONE;
  header.fec_group = 0;

  QuicStreamFragment stream_fragment;
  stream_fragment.stream_id = 0x01020304;
  stream_fragment.fin = true;
  stream_fragment.offset = GG_UINT64_C(0xBA98FEDC32107654);
  stream_fragment.data = "hello world!";

  QuicFragment fragment;
  fragment.type = STREAM_FRAGMENT;
  fragment.stream_fragment = &stream_fragment;

  QuicFragments fragments;
  fragments.push_back(fragment);

  QuicPacket *original;
  ASSERT_TRUE(framer_.ConstructFragementDataPacket(
      header, fragments, &original));
  EXPECT_EQ(header.retransmission_count, framer_.GetRetransmitCount(original));

  header.retransmission_count = 2;
  QuicPacket *retransmitted;
  ASSERT_TRUE(framer_.ConstructFragementDataPacket(
      header, fragments, &retransmitted));

  framer_.IncrementRetransmitCount(original);
  EXPECT_EQ(header.retransmission_count, framer_.GetRetransmitCount(original));

  test::CompareCharArraysWithHexError(
      "constructed packet", original->data(), original->length(),
      retransmitted->data(), retransmitted->length());
  delete original;
  delete retransmitted;
}

TEST_F(QuicFramerTest, EncryptPacket) {
  unsigned char packet[] = {
    // guid
    0x10, 0x32, 0x54, 0x76,
    0x98, 0xBA, 0xDC, 0xFE,
    // packet id
    0xBC, 0x9A, 0x78, 0x56,
    0x34, 0x12,
    // retransmission count
    0x01,
    // transmission time
    0x87, 0x96, 0xA5, 0xB4,
    0xC3, 0xD2, 0xE1, 0xF0,
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

  scoped_ptr<QuicEncryptedPacket> encrypted(framer_.EncryptPacket(
      QuicPacket(AsChars(packet), arraysize(packet), false)));
  ASSERT_TRUE(encrypted.get() != NULL);
  EXPECT_TRUE(CheckEncryption(StringPiece(AsChars(packet), arraysize(packet))));
}

}  // namespace test

}  // namespace net
