// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_PROTOCOL_H_
#define NET_QUIC_QUIC_PROTOCOL_H_

#include <stddef.h>
#include <limits>
#include <map>
#include <ostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/string_piece.h"
#include "net/base/int128.h"
#include "net/base/net_export.h"
#include "net/quic/quic_time.h"

namespace net {

class QuicPacket;

typedef uint64 QuicGuid;
typedef uint32 QuicStreamId;
typedef uint64 QuicStreamOffset;
typedef uint64 QuicPacketSequenceNumber;
typedef uint8 QuicFecGroupNumber;

// TODO(rch): Consider Quic specific names for these constants.
const size_t kMaxPacketSize = 1200;  // Maximum size in bytes of a QUIC packet.

// Maximum number of open streams per connection.
const size_t kDefaultMaxStreamsPerConnection = 100;

// Size in bytes of the packet header common across all packets.
const size_t kPacketHeaderSize = 16;
// Index of the first byte in a QUIC packet of FEC protected data.
const size_t kStartOfFecProtectedData = kPacketHeaderSize;
// Index of the first byte in a QUIC packet of encrypted data.
const size_t kStartOfEncryptedData = kPacketHeaderSize - 1;
// Index of the first byte in a QUIC packet which is hashed.
const size_t kStartOfHashData = 0;
// Index into the sequence number offset in the header.
const int kSequenceNumberOffset = 8;
// Index into the flags offset in the header.
const int kFlagsOffset = 14;
// Index into the fec group offset in the header.
const int kFecGroupOffset = 15;

// Size in bytes of all stream frame fields.
const size_t kMinStreamFrameLength = 15;

// Limit on the delta between stream IDs.
const QuicStreamId kMaxStreamIdDelta = 100;

// Reserved ID for the crypto stream.
// TODO(rch): ensure that this is not usable by any other streams.
const QuicStreamId kCryptoStreamId = 1;

typedef std::pair<QuicPacketSequenceNumber, QuicPacket*> PacketPair;

const int64 kDefaultTimeoutUs = 600000000;  // 10 minutes.

enum QuicFrameType {
  STREAM_FRAME = 0,
  PDU_FRAME,
  ACK_FRAME,
  RST_STREAM_FRAME,
  CONNECTION_CLOSE_FRAME,
  NUM_FRAME_TYPES
};

enum QuicPacketFlags {
  PACKET_FLAGS_NONE = 0,
  PACKET_FLAGS_FEC = 1,  // Payload is FEC as opposed to frames.

  PACKET_FLAGS_MAX = PACKET_FLAGS_FEC
};

enum QuicErrorCode {
  // Stream errors.
  QUIC_NO_ERROR = 0,

  // There were data frames after the a fin or reset.
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
  // Frame data is malformed.
  QUIC_INVALID_FRAME_DATA,
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
  QuicPacketFlags flags;
  QuicFecGroupNumber fec_group;
};

struct NET_EXPORT_PRIVATE QuicStreamFrame {
  QuicStreamFrame();
  QuicStreamFrame(QuicStreamId stream_id,
                  bool fin,
                  uint64 offset,
                  base::StringPiece data);

  QuicStreamId stream_id;
  bool fin;
  uint64 offset;
  base::StringPiece data;
};

// TODO(ianswett): Re-evaluate the trade-offs of hash_set vs set when framing
// is finalized.
typedef std::set<QuicPacketSequenceNumber> SequenceSet;
typedef std::map<QuicPacketSequenceNumber, QuicTime> TimeMap;

struct NET_EXPORT_PRIVATE ReceivedPacketInfo {
  ReceivedPacketInfo();
  ~ReceivedPacketInfo();
  friend std::ostream& operator<<(std::ostream& os,
                                  const ReceivedPacketInfo& s);

  // Records a packet receipt.
  void RecordReceived(QuicPacketSequenceNumber sequence_number, QuicTime time);

  // True if the sequence number is greater than largest_received or is listed
  // as missing.
  // Always returns false for sequence numbers less than least_unacked.
  bool IsAwaitingPacket(QuicPacketSequenceNumber sequence_number) const;

  // Clears all missing packets and ack times less than least_unacked.
  void ClearAcksBefore(QuicPacketSequenceNumber least_unacked);

  // Clears all packet sequence number and time pairs, preventing future
  // transmission.
  void ClearAckTimes();

  // The highest packet sequence number we've received from the peer.
  QuicPacketSequenceNumber largest_received;

  // The set of packets which we're expecting and have not received.
  SequenceSet missing_packets;

  // The set of received packets since the last ack was sent and their arrival
  // times.
  std::map<QuicPacketSequenceNumber, QuicTime> received_packet_times;
};

struct NET_EXPORT_PRIVATE SentPacketInfo {
  SentPacketInfo();
  ~SentPacketInfo();
  friend std::ostream& operator<<(std::ostream& os,
                                  const SentPacketInfo& s);
  // The lowest packet we've sent which is unacked, and we expect an ack for.
  QuicPacketSequenceNumber least_unacked;
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
  friend std::ostream& operator<<(std::ostream& os, const CongestionInfo& c);
  union {
    CongestionFeedbackMessageTCP tcp;
    CongestionFeedbackMessageInterArrival inter_arrival;
    CongestionFeedbackMessageFixRate fix_rate;
  };
};

struct NET_EXPORT_PRIVATE QuicAckFrame {
  QuicAckFrame() {}
  // Testing convenience method to construct a QuicAckFrame with all packets
  // from least_unacked to largest_received acked at time_received.
  QuicAckFrame(QuicPacketSequenceNumber largest_received,
               QuicTime time_received,
               QuicPacketSequenceNumber least_unacked);

  NET_EXPORT_PRIVATE friend std::ostream& operator<<(std::ostream& os,
                                                     const QuicAckFrame& s);

  SentPacketInfo sent_info;
  ReceivedPacketInfo received_info;
  CongestionInfo congestion_info;
};

struct NET_EXPORT_PRIVATE QuicRstStreamFrame {
  QuicRstStreamFrame() {}
  QuicRstStreamFrame(QuicStreamId stream_id, uint64 offset,
                     QuicErrorCode error_code)
      : stream_id(stream_id), offset(offset), error_code(error_code) {
    DCHECK_LE(error_code, std::numeric_limits<uint8>::max());
  }

  QuicStreamId stream_id;
  uint64 offset;
  QuicErrorCode error_code;
  std::string error_details;
};

struct NET_EXPORT_PRIVATE QuicConnectionCloseFrame {
  QuicErrorCode error_code;
  QuicAckFrame ack_frame;
  std::string error_details;
};

struct NET_EXPORT_PRIVATE QuicFrame {
  QuicFrame() {}
  explicit QuicFrame(QuicStreamFrame* stream_frame)
      : type(STREAM_FRAME), stream_frame(stream_frame) {
  }
  explicit QuicFrame(QuicAckFrame* frame)
      : type(ACK_FRAME), ack_frame(frame) {
  }
  explicit QuicFrame(QuicRstStreamFrame* frame)
      : type(RST_STREAM_FRAME),
        rst_stream_frame(frame) {
  }
  explicit QuicFrame(QuicConnectionCloseFrame* frame)
      : type(CONNECTION_CLOSE_FRAME),
        connection_close_frame(frame) {
  }

  QuicFrameType type;
  union {
    QuicStreamFrame* stream_frame;
    QuicAckFrame* ack_frame;
    QuicRstStreamFrame* rst_stream_frame;
    QuicConnectionCloseFrame* connection_close_frame;
  };
};

typedef std::vector<QuicFrame> QuicFrames;

struct NET_EXPORT_PRIVATE QuicFecData {
  QuicFecData();

  bool operator==(const QuicFecData& other) const;

  QuicFecGroupNumber fec_group;
  QuicPacketSequenceNumber min_protected_packet_sequence_number;
  // The last protected packet's sequence number will be one
  // less than the sequence number of the FEC packet.
  base::StringPiece redundancy;
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
  QuicPacket(
      char* buffer, size_t length, bool owns_buffer, QuicPacketFlags flags)
      : QuicData(buffer, length, owns_buffer),
        buffer_(buffer),
        flags_(flags) { }

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

  bool IsFecPacket() const {
    return flags_ == PACKET_FLAGS_FEC;
  }

  char* mutable_data() { return buffer_; }

 private:
  char* buffer_;
  const QuicPacketFlags flags_;

  DISALLOW_COPY_AND_ASSIGN(QuicPacket);
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

 private:
  DISALLOW_COPY_AND_ASSIGN(QuicEncryptedPacket);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_PROTOCOL_H_
