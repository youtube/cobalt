// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_PROTOCOL_H_
#define NET_QUIC_QUIC_PROTOCOL_H_

#include <limits>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/string_piece.h"
#include "net/base/net_export.h"
#include "net/quic/uint128.h"

namespace net {

class QuicPacket;

typedef uint64 QuicGuid;
typedef uint32 QuicStreamId;
typedef uint64 QuicStreamOffset;
typedef uint64 QuicPacketSequenceNumber;
typedef uint64 QuicTransmissionTime;
typedef uint8 QuicFecGroupNumber;

// TODO(rch): Consider Quic specific names for these constants.
const size_t kMaxPacketSize = 1200;  // Maximum size in bytes of a QUIC packet.

// Maximum number of open streams per connection.
const size_t kDefaultMaxStreamsPerConnection = 100;

// Size in bytes of the packet header common across all packets.
const size_t kPacketHeaderSize = 25;
// Index of the first byte in a QUIC packet of FEC protected data.
const size_t kStartOfFecProtectedData = kPacketHeaderSize;
// Index of the first byte in a QUIC packet of encrypted data.
const size_t kStartOfEncryptedData = kPacketHeaderSize - 1;
// Index of the first byte in a QUIC packet which is hashed.
const size_t kStartOfHashData = 0;
// Index into the retransmission offset in the header.
// (After GUID and sequence number.)
const int kRetransmissionOffset = 14;
// Index into the transmission time offset in the header.
const int kTransmissionTimeOffset = 15;

// Size in bytes of all stream fragment fields.
const size_t kMinStreamFragmentLength = 15;

// Limit on the delta between stream IDs.
const QuicStreamId kMaxStreamIdDelta = 100;

// Reserved ID for the crypto stream.
// TODO(rch): ensure that this is not usable by any other streams.
const QuicStreamId kCryptoStreamId = 1;

typedef std::pair<QuicPacketSequenceNumber, QuicPacket*> PacketPair;

const int64 kDefaultTimeout = 600000000;  // 10 minutes

enum QuicFragmentType {
  STREAM_FRAGMENT = 0,
  PDU_FRAGMENT,
  ACK_FRAGMENT,
  RST_STREAM_FRAGMENT,
  CONNECTION_CLOSE_FRAGMENT,
  NUM_FRAGMENT_TYPES
};

enum QuicPacketFlags {
  PACKET_FLAGS_NONE = 0,
  PACKET_FLAGS_FEC = 1,  // Payload is FEC as opposed to fragments.

  PACKET_FLAGS_MAX = PACKET_FLAGS_FEC
};

enum QuicErrorCode {
  // Stream errors.
  QUIC_NO_ERROR = 0,

  // There were data fragments after the a fin or reset.
  QUIC_STREAM_DATA_AFTER_TERMINATION,
  // There was some server error which halted stream processing.
  QUIC_SERVER_ERROR_PROCESSING_STREAM,
  // We got two fin or reset offsets which did not match.
  QUIC_MULTIPLE_TERMINATION_OFFSETS,
  // We got bad payload and can not respond to it at the protocol level.
  QUIC_BAD_APPLICATION_PAYLOAD,

  // Connection errors.

  // Control frame is malformed.
  QUIC_INVALID_PACKET_HEADER,
  // Fragment data is malformed.
  QUIC_INVALID_FRAGMENT_DATA,
  // FEC data is malformed.
  QUIC_INVALID_FEC_DATA,
  // Stream rst data is malformed
  QUIC_INVALID_RST_STREAM_DATA,
  // Connection close data is malformed.
  QUIC_INVALID_CONNECTION_CLOSE_DATA,
  // Ack data is malformed.
  QUIC_INVALID_ACK_DATA,
  // There was an error decrypting.
  QUIC_DECRYPTION_FAILURE,
  // There was an error encrypting.
  QUIC_ENCRYPTION_FAILURE,
  // The packet exceeded kMaxPacketSize.
  QUIC_PACKET_TOO_LARGE,
  // Data was sent for a stream which did not exist.
  QUIC_PACKET_FOR_NONEXISTENT_STREAM,
  // The client is going away (browser close, etc.)
  QUIC_CLIENT_GOING_AWAY,
  // The server is going away (restart etc.)
  QUIC_SERVER_GOING_AWAY,
  // A stream ID was invalid.
  QUIC_INVALID_STREAM_ID,
  // Too many streams already open.
  QUIC_TOO_MANY_OPEN_STREAMS,

  // We hit our prenegotiated (or default) timeout
  QUIC_CONNECTION_TIMED_OUT,

  // Crypto errors.

  // Handshake message contained out of order tags.
  QUIC_CRYPTO_TAGS_OUT_OF_ORDER,
  // Handshake message contained too many entires.
  QUIC_CRYPTO_TOO_MANY_ENTRIES,
  // Handshake message contained an invalid value length.
  QUIC_CRYPTO_INVALID_VALUE_LENGTH,
  // A crypto message was received after the handshake was complete.
  QUIC_CRYPTO_MESSAGE_AFTER_HANDSHAKE_COMPLETE,
  // A crypto message was receieved with an illegal message tag.
  QUIC_INVALID_CRYPTO_MESSAGE_TYPE,

};

struct NET_EXPORT_PRIVATE QuicPacketHeader {
  // Includes the ConnectionHeader and CongestionMonitoredHeader
  // from the design docs, as well as some elements of DecryptedData.
  QuicGuid guid;
  QuicPacketSequenceNumber packet_sequence_number;
  uint8 retransmission_count;
  QuicTransmissionTime transmission_time;
  QuicPacketFlags flags;
  QuicFecGroupNumber fec_group;
};

struct NET_EXPORT_PRIVATE QuicStreamFragment {
  QuicStreamFragment();
  QuicStreamFragment(QuicStreamId stream_id,
                     bool fin,
                     uint64 offset,
                     base::StringPiece data);

  QuicStreamId stream_id;
  bool fin;
  uint64 offset;
  base::StringPiece data;
};

struct NET_EXPORT_PRIVATE ReceivedPacketInfo {
  ReceivedPacketInfo();
  ~ReceivedPacketInfo();
  // The highest packet sequence number we've received from the peer.
  QuicPacketSequenceNumber largest_received;
  // The time at which we received the above packet.
  QuicTransmissionTime time_received;
  // The set of packets which we're expecting and have not received.
  // This includes any packets between the lowest and largest_received
  // which we have neither seen nor been informed are non-retransmitting.
  base::hash_set<QuicPacketSequenceNumber> missing_packets;
};

struct NET_EXPORT_PRIVATE SentPacketInfo {
  SentPacketInfo();
  ~SentPacketInfo();
  // The lowest packet we've sent which is unacked, and we expect an ack for.
  QuicPacketSequenceNumber least_unacked;
  // The set of packets between least_unacked and the last packet we have sent
  // which we will not resend.
  base::hash_set<QuicPacketSequenceNumber> non_retransmiting;
};

// Defines for all types of congestion feedback that will be negotiated in QUIC,
// kTCP MUST be supported by all QUIC implementations to guarentee 100%
// compatibility.
enum CongestionFeedbackType {
  kNone = 0,  // No feedback provided
  kTCP,  // Used to mimic TCP.
  kInterArrival,  // Use additional inter arrival information.
  kFixRate,  // Provided for testing.
};

struct NET_EXPORT_PRIVATE CongestionFeedbackMessageTCP {
  uint16 accumulated_number_of_lost_packets;
  uint16 receive_window;  // Number of bytes >> 4.
};

struct NET_EXPORT_PRIVATE CongestionFeedbackMessageInterArrival {
  uint16 accumulated_number_of_lost_packets;
  int16 offset_time;
  uint16 delta_time;  // delta time is described below.
};

/*
 * Description of delta time.
 *
 * 0                   1
 * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |D|S|       offset_time         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * Where:
 *   D is the time domain. If set time domain is in milliseconds, else in
 *    microseconds.
 *   S is the sign bit.
 *   offset_time is the time offset where the relative packet size is equal to
 *    0.
 */

struct NET_EXPORT_PRIVATE CongestionFeedbackMessageFixRate {
  uint32 bitrate_in_bytes_per_second;
};

struct NET_EXPORT_PRIVATE CongestionInfo {
  CongestionFeedbackType type;
  union {
    CongestionFeedbackMessageTCP tcp;
    CongestionFeedbackMessageInterArrival inter_arrival;
    CongestionFeedbackMessageFixRate fix_rate;
  };
};

struct NET_EXPORT_PRIVATE QuicAckFragment {
  QuicAckFragment() {}
  QuicAckFragment(QuicPacketSequenceNumber largest_received,
                  QuicTransmissionTime time_received,
                  QuicPacketSequenceNumber least_unacked) {
    received_info.largest_received = largest_received;
    received_info.time_received = time_received;
    sent_info.least_unacked = least_unacked;
    congestion_info.type = kNone;
  }

  SentPacketInfo sent_info;
  ReceivedPacketInfo received_info;
  CongestionInfo congestion_info;

  friend std::ostream& operator<<(std::ostream& os, const QuicAckFragment& s) {
    os << "largest_received: " << s.received_info.largest_received
       << " time: " << s.received_info.time_received
       << " missing: ";
    for (base::hash_set<QuicPacketSequenceNumber>::const_iterator it =
           s.received_info.missing_packets.begin();
           it != s.received_info.missing_packets.end(); ++it) {
      os << *it << " ";
    }

    os << " least_waiting: " << s.sent_info.least_unacked
       << " no_retransmit: ";
    for (base::hash_set<QuicPacketSequenceNumber>::const_iterator it =
           s.sent_info.non_retransmiting.begin();
         it != s.sent_info.non_retransmiting.end(); ++it) {
      os << *it << " ";
    }
    os << "\n";
    return os;
  }
};

struct NET_EXPORT_PRIVATE QuicRstStreamFragment {
  QuicRstStreamFragment() {}
  QuicRstStreamFragment(QuicStreamId stream_id, uint64 offset,
                        QuicErrorCode details)
      : stream_id(stream_id), offset(offset), details(details) {
    DCHECK_LE(details, std::numeric_limits<uint8>::max());
  }

  QuicStreamId stream_id;
  uint64 offset;
  QuicErrorCode details;
};

struct NET_EXPORT_PRIVATE QuicConnectionCloseFragment {
  QuicErrorCode details;
  QuicAckFragment ack_fragment;
};

struct NET_EXPORT_PRIVATE QuicFragment {
  QuicFragment() {}
  explicit QuicFragment(QuicStreamFragment* stream_fragment)
      : type(STREAM_FRAGMENT), stream_fragment(stream_fragment) {
  }
  explicit QuicFragment(QuicAckFragment* fragment)
      : type(ACK_FRAGMENT), ack_fragment(fragment) {
  }
  explicit QuicFragment(QuicRstStreamFragment* fragment)
      : type(RST_STREAM_FRAGMENT),
        rst_stream_fragment(fragment) {
  }
  explicit QuicFragment(QuicConnectionCloseFragment* fragment)
      : type(CONNECTION_CLOSE_FRAGMENT),
        connection_close_fragment(fragment) {
  }

  QuicFragmentType type;
  union {
    QuicStreamFragment* stream_fragment;
    QuicAckFragment* ack_fragment;
    QuicRstStreamFragment* rst_stream_fragment;
    QuicConnectionCloseFragment* connection_close_fragment;
  };
};

typedef std::vector<QuicFragment> QuicFragments;

struct NET_EXPORT_PRIVATE QuicFecData {
  QuicFecData();
  QuicFecGroupNumber fec_group;
  QuicPacketSequenceNumber first_protected_packet_sequence_number;
  // The last protected packet's sequence number will be one
  // less than the sequence number of the FEC packet.
  base::StringPiece redundancy;
  bool operator==(const QuicFecData& other) const {
    if (fec_group != other.fec_group) {
      return false;
    }
    if (first_protected_packet_sequence_number !=
        other.first_protected_packet_sequence_number) {
      return false;
    }
    if (redundancy != other.redundancy) {
      return false;
    }
    return true;
  }
};

struct NET_EXPORT_PRIVATE QuicPacketData {
  std::string data;
};

class NET_EXPORT_PRIVATE QuicData {
 public:
  QuicData(const char* buffer, size_t length)
      : buffer_(buffer),
        length_(length),
        owns_buffer_(false) { }

  QuicData(char* buffer, size_t length, bool owns_buffer)
      : buffer_(buffer),
        length_(length),
        owns_buffer_(owns_buffer) { }

  virtual ~QuicData();

  base::StringPiece AsStringPiece() const {
    return base::StringPiece(data(), length());
  }

  const char* data() const { return buffer_; }
  size_t length() const { return length_; }

 private:
  const char* buffer_;
  size_t length_;
  bool owns_buffer_;

  DISALLOW_COPY_AND_ASSIGN(QuicData);
};

class NET_EXPORT_PRIVATE QuicPacket : public QuicData {
 public:
  QuicPacket(char* buffer, size_t length, bool owns_buffer)
      : QuicData(buffer, length, owns_buffer),
        buffer_(buffer) { }

  base::StringPiece FecProtectedData() const {
    return base::StringPiece(data() + kStartOfFecProtectedData,
                             length() - kStartOfFecProtectedData);
  }

  base::StringPiece AssociatedData() const {
    return base::StringPiece(data() + kStartOfHashData, kStartOfEncryptedData);
  }

  base::StringPiece Plaintext() const {
    return base::StringPiece(data() + kStartOfEncryptedData,
                             length() - kStartOfEncryptedData);
  }
  char* mutable_data() { return buffer_; }

 private:
  char* buffer_;

  // TODO(rch): DISALLOW_COPY_AND_ASSIGN
};

class NET_EXPORT_PRIVATE QuicEncryptedPacket : public QuicData {
 public:
  QuicEncryptedPacket(const char* buffer, size_t length)
      : QuicData(buffer, length) { }

  QuicEncryptedPacket(char* buffer, size_t length, bool owns_buffer)
      : QuicData(buffer, length, owns_buffer) { }

  base::StringPiece AssociatedData() const {
    return base::StringPiece(data() + kStartOfHashData, kStartOfEncryptedData);
  }

  // TODO(rch): DISALLOW_COPY_AND_ASSIGN
};

}  // namespace net

#endif  // NET_QUIC_QUIC_PROTOCOL_H_
