// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains some protocol structures for use with Spdy.

#ifndef NET_SPDY_SPDY_PROTOCOL_H_
#define NET_SPDY_SPDY_PROTOCOL_H_
#pragma once

#include <limits>

#include "base/basictypes.h"
#include "base/logging.h"
#include "net/base/sys_byteorder.h"
#include "net/spdy/spdy_bitmasks.h"

//  Data Frame Format
//  +----------------------------------+
//  |0|       Stream-ID (31bits)       |
//  +----------------------------------+
//  | flags (8)  |  Length (24 bits)   |
//  +----------------------------------+
//  |               Data               |
//  +----------------------------------+
//
//  Control Frame Format
//  +----------------------------------+
//  |1| Version(15bits) | Type(16bits) |
//  +----------------------------------+
//  | flags (8)  |  Length (24 bits)   |
//  +----------------------------------+
//  |               Data               |
//  +----------------------------------+
//
//  Control Frame: SYN_STREAM
//  +----------------------------------+
//  |1|000000000000001|0000000000000001|
//  +----------------------------------+
//  | flags (8)  |  Length (24 bits)   |  >= 12
//  +----------------------------------+
//  |X|       Stream-ID(31bits)        |
//  +----------------------------------+
//  |X|Associated-To-Stream-ID (31bits)|
//  +----------------------------------+
//  |Pri| unused      | Length (16bits)|
//  +----------------------------------+
//
//  Control Frame: SYN_REPLY
//  +----------------------------------+
//  |1|000000000000001|0000000000000010|
//  +----------------------------------+
//  | flags (8)  |  Length (24 bits)   |  >= 8
//  +----------------------------------+
//  |X|       Stream-ID(31bits)        |
//  +----------------------------------+
//  | unused (16 bits)| Length (16bits)|
//  +----------------------------------+
//
//  Control Frame: RST_STREAM
//  +----------------------------------+
//  |1|000000000000001|0000000000000011|
//  +----------------------------------+
//  | flags (8)  |  Length (24 bits)   |  >= 4
//  +----------------------------------+
//  |X|       Stream-ID(31bits)        |
//  +----------------------------------+
//  |        Status code (32 bits)     |
//  +----------------------------------+
//
//  Control Frame: SETTINGS
//  +----------------------------------+
//  |1|000000000000001|0000000000000100|
//  +----------------------------------+
//  | flags (8)  |  Length (24 bits)   |
//  +----------------------------------+
//  |        # of entries (32)         |
//  +----------------------------------+
//
//  Control Frame: NOOP
//  +----------------------------------+
//  |1|000000000000001|0000000000000101|
//  +----------------------------------+
//  | flags (8)  |  Length (24 bits)   | = 0
//  +----------------------------------+
//
//  Control Frame: PING
//  +----------------------------------+
//  |1|000000000000001|0000000000000110|
//  +----------------------------------+
//  | flags (8)  |  Length (24 bits)   | = 4
//  +----------------------------------+
//  |        Unique id (32 bits)       |
//  +----------------------------------+
//
//  Control Frame: GOAWAY
//  +----------------------------------+
//  |1|000000000000001|0000000000000111|
//  +----------------------------------+
//  | flags (8)  |  Length (24 bits)   | = 4
//  +----------------------------------+
//  |X|  Last-accepted-stream-id       |
//  +----------------------------------+
//
//  Control Frame: HEADERS
//  +----------------------------------+
//  |1|000000000000001|0000000000001000|
//  +----------------------------------+
//  | flags (8)  |  Length (24 bits)   | >= 8
//  +----------------------------------+
//  |X|      Stream-ID (31 bits)       |
//  +----------------------------------+
//  | unused (16 bits)| Length (16bits)|
//  +----------------------------------+
//
//  Control Frame: WINDOW_UPDATE
//  +----------------------------------+
//  |1|000000000000001|0000000000001001|
//  +----------------------------------+
//  | flags (8)  |  Length (24 bits)   | = 8
//  +----------------------------------+
//  |X|      Stream-ID (31 bits)       |
//  +----------------------------------+
//  |   Delta-Window-Size (32 bits)    |
//  +----------------------------------+
//
namespace spdy {

// This implementation of Spdy is version 2; It's like version 1, with some
// minor tweaks.
const int kSpdyProtocolVersion = 2;

// Initial window size for a Spdy stream
const size_t kSpdyStreamInitialWindowSize = 64 * 1024;  // 64 KBytes

// Maximum window size for a Spdy stream
const size_t kSpdyStreamMaximumWindowSize = 0x7FFFFFFF;  // Max signed 32bit int

// HTTP-over-SPDY header constants
const char kMethod[] = "method";
const char kStatus[] = "status";
const char kUrl[] = "url";
const char kVersion[] = "version";
// When we server push, we will add [path: fully/qualified/url] to the server
// push headers so that the client will know what url the data corresponds to.
const char kPath[] = "path";

// Note: all protocol data structures are on-the-wire format.  That means that
//       data is stored in network-normalized order.  Readers must use the
//       accessors provided or call ntohX() functions.

// Types of Spdy Control Frames.
enum SpdyControlType {
  SYN_STREAM = 1,
  SYN_REPLY,
  RST_STREAM,
  SETTINGS,
  NOOP,
  PING,
  GOAWAY,
  HEADERS,
  WINDOW_UPDATE,
  NUM_CONTROL_FRAME_TYPES
};

// Flags on data packets.
enum SpdyDataFlags {
  DATA_FLAG_NONE = 0,
  DATA_FLAG_FIN = 1,
  // TODO(hkhalil): Remove.
  DATA_FLAG_COMPRESSED = 2
};

// Flags on control packets
enum SpdyControlFlags {
  CONTROL_FLAG_NONE = 0,
  CONTROL_FLAG_FIN = 1,
  CONTROL_FLAG_UNIDIRECTIONAL = 2
};

// Flags on the SETTINGS control frame.
enum SpdySettingsControlFlags {
  SETTINGS_FLAG_CLEAR_PREVIOUSLY_PERSISTED_SETTINGS = 0x1
};

// Flags for settings within a SETTINGS frame.
enum SpdySettingsFlags {
  SETTINGS_FLAG_PLEASE_PERSIST = 0x1,
  SETTINGS_FLAG_PERSISTED = 0x2
};

// List of known settings.
enum SpdySettingsIds {
  SETTINGS_UPLOAD_BANDWIDTH = 0x1,
  SETTINGS_DOWNLOAD_BANDWIDTH = 0x2,
  // Network round trip time in milliseconds.
  SETTINGS_ROUND_TRIP_TIME = 0x3,
  SETTINGS_MAX_CONCURRENT_STREAMS = 0x4,
  // TCP congestion window in packets.
  SETTINGS_CURRENT_CWND = 0x5,
  // Downstream byte retransmission rate in percentage.
  SETTINGS_DOWNLOAD_RETRANS_RATE = 0x6,
  // Initial window size in bytes
  SETTINGS_INITIAL_WINDOW_SIZE = 0x7
};

// Status codes, as used in control frames (primarily RST_STREAM).
enum SpdyStatusCodes {
  INVALID = 0,
  PROTOCOL_ERROR = 1,
  INVALID_STREAM = 2,
  REFUSED_STREAM = 3,
  UNSUPPORTED_VERSION = 4,
  CANCEL = 5,
  INTERNAL_ERROR = 6,
  FLOW_CONTROL_ERROR = 7,
  INVALID_ASSOCIATED_STREAM = 8,
  NUM_STATUS_CODES = 9
};

// A SPDY stream id is a 31 bit entity.
typedef uint32 SpdyStreamId;

// A SPDY priority is a number between 0 and 3 (inclusive).
typedef uint8 SpdyPriority;

// SPDY Priorities. (there are only 2 bits)
#define SPDY_PRIORITY_LOWEST 3
#define SPDY_PRIORITY_HIGHEST 0

// -------------------------------------------------------------------------
// These structures mirror the protocol structure definitions.

// For the control data structures, we pack so that sizes match the
// protocol over-the-wire sizes.
#pragma pack(push)
#pragma pack(1)

// A special structure for the 8 bit flags and 24 bit length fields.
union FlagsAndLength {
  uint8 flags_[4];  // 8 bits
  uint32 length_;   // 24 bits
};

// The basic SPDY Frame structure.
struct SpdyFrameBlock {
  union {
    struct {
      uint16 version_;
      uint16 type_;
    } control_;
    struct {
      SpdyStreamId stream_id_;
    } data_;
  };
  FlagsAndLength flags_length_;
};

// A SYN_STREAM Control Frame structure.
struct SpdySynStreamControlFrameBlock : SpdyFrameBlock {
  SpdyStreamId stream_id_;
  SpdyStreamId associated_stream_id_;
  SpdyPriority priority_;
  uint8 unused_;
};

// A SYN_REPLY Control Frame structure.
struct SpdySynReplyControlFrameBlock : SpdyFrameBlock {
  SpdyStreamId stream_id_;
  uint16 unused_;
};

// A RST_STREAM Control Frame structure.
struct SpdyRstStreamControlFrameBlock : SpdyFrameBlock {
  SpdyStreamId stream_id_;
  uint32 status_;
};

// A SETTINGS Control Frame structure.
struct SpdySettingsControlFrameBlock : SpdyFrameBlock {
  uint32 num_entries_;
  // Variable data here.
};

// A NOOP Control Frame structure.
struct SpdyNoopControlFrameBlock : SpdyFrameBlock {
};

// A PING Control Frame structure.
struct SpdyPingControlFrameBlock : SpdyFrameBlock {
  uint32 unique_id_;
};

// A GOAWAY Control Frame structure.
struct SpdyGoAwayControlFrameBlock : SpdyFrameBlock {
  SpdyStreamId last_accepted_stream_id_;
};

// A HEADERS Control Frame structure.
struct SpdyHeadersControlFrameBlock : SpdyFrameBlock {
  SpdyStreamId stream_id_;
  uint16 unused_;
};

// A WINDOW_UPDATE Control Frame structure
struct SpdyWindowUpdateControlFrameBlock : SpdyFrameBlock {
  SpdyStreamId stream_id_;
  uint32 delta_window_size_;
};

// A structure for the 8 bit flags and 24 bit ID fields.
union SettingsFlagsAndId {
  // Sets both flags and id to the value for flags-and-id as sent over the wire
  SettingsFlagsAndId(uint32 val) : id_(val) {}
  uint8 flags() const { return flags_[0]; }
  void set_flags(uint8 flags) { flags_[0] = flags; }
  uint32 id() const { return (ntohl(id_) & kSettingsIdMask); }
  void set_id(uint32 id) {
    DCHECK_EQ(0u, (id & ~kSettingsIdMask));
    id_ = htonl((flags() << 24) | (id & kSettingsIdMask));
  }

  uint8 flags_[4];  // 8 bits
  uint32 id_;       // 24 bits
};

#pragma pack(pop)

// -------------------------------------------------------------------------
// Wrapper classes for various SPDY frames.

// All Spdy Frame types derive from this SpdyFrame class.
class SpdyFrame {
 public:
  // Create a SpdyFrame for a given sized buffer.
  explicit SpdyFrame(size_t size) : frame_(NULL), owns_buffer_(true) {
    DCHECK_GE(size, sizeof(struct SpdyFrameBlock));
    char* buffer = new char[size];
    memset(buffer, 0, size);
    frame_ = reinterpret_cast<struct SpdyFrameBlock*>(buffer);
  }

  // Create a SpdyFrame using a pre-created buffer.
  // If |owns_buffer| is true, this class takes ownership of the buffer
  // and will delete it on cleanup.  The buffer must have been created using
  // new char[].
  // If |owns_buffer| is false, the caller retains ownership of the buffer and
  // is responsible for making sure the buffer outlives this frame.  In other
  // words, this class does NOT create a copy of the buffer.
  SpdyFrame(char* data, bool owns_buffer)
      : frame_(reinterpret_cast<struct SpdyFrameBlock*>(data)),
        owns_buffer_(owns_buffer) {
    DCHECK(frame_);
  }

  ~SpdyFrame() {
    if (owns_buffer_) {
      char* buffer = reinterpret_cast<char*>(frame_);
      delete [] buffer;
    }
    frame_ = NULL;
  }

  // Provides access to the frame bytes, which is a buffer containing
  // the frame packed as expected for sending over the wire.
  char* data() const { return reinterpret_cast<char*>(frame_); }

  uint8 flags() const { return frame_->flags_length_.flags_[0]; }
  void set_flags(uint8 flags) { frame_->flags_length_.flags_[0] = flags; }

  uint32 length() const {
    return ntohl(frame_->flags_length_.length_) & kLengthMask;
  }

  void set_length(uint32 length) {
    DCHECK_EQ(0u, (length & ~kLengthMask));
    length = htonl(length & kLengthMask);
    frame_->flags_length_.length_ = flags() | length;
  }

  bool is_control_frame() const {
    return (ntohs(frame_->control_.version_) & kControlFlagMask) ==
        kControlFlagMask;
  }

  // Returns the size of the SpdyFrameBlock structure.
  // Every SpdyFrame* class has a static size() method for accessing
  // the size of the data structure which will be sent over the wire.
  // Note:  this is not the same as sizeof(SpdyFrame).
  static size_t size() { return sizeof(struct SpdyFrameBlock); }

 protected:
  SpdyFrameBlock* frame_;

 private:
  bool owns_buffer_;
  DISALLOW_COPY_AND_ASSIGN(SpdyFrame);
};

// A Data Frame.
class SpdyDataFrame : public SpdyFrame {
 public:
  SpdyDataFrame() : SpdyFrame(size()) {}
  SpdyDataFrame(char* data, bool owns_buffer)
      : SpdyFrame(data, owns_buffer) {}

  SpdyStreamId stream_id() const {
    return ntohl(frame_->data_.stream_id_) & kStreamIdMask;
  }

  // Note that setting the stream id sets the control bit to false.
  // As stream id should always be set, this means the control bit
  // should always be set correctly.
  void set_stream_id(SpdyStreamId id) {
    DCHECK_EQ(0u, (id & ~kStreamIdMask));
    frame_->data_.stream_id_ = htonl(id & kStreamIdMask);
  }

  // Returns the size of the SpdyFrameBlock structure.
  // Note: this is not the size of the SpdyDataFrame class.
  static size_t size() { return SpdyFrame::size(); }

  const char* payload() const {
    return reinterpret_cast<const char*>(frame_) + size();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SpdyDataFrame);
};

// A Control Frame.
class SpdyControlFrame : public SpdyFrame {
 public:
  explicit SpdyControlFrame(size_t size) : SpdyFrame(size) {}
  SpdyControlFrame(char* data, bool owns_buffer)
      : SpdyFrame(data, owns_buffer) {}

  // Callers can use this method to check if the frame appears to be a valid
  // frame.  Does not guarantee that there are no errors.
  bool AppearsToBeAValidControlFrame() const {
    // Right now we only check if the frame has an out-of-bounds type.
    uint16 type = ntohs(block()->control_.type_);
    return (type >= SYN_STREAM && type < NUM_CONTROL_FRAME_TYPES);
  }

  uint16 version() const {
    const int kVersionMask = 0x7fff;
    return ntohs(block()->control_.version_) & kVersionMask;
  }

  void set_version(uint16 version) {
    DCHECK_EQ(0u, version & kControlFlagMask);
    mutable_block()->control_.version_ = htons(kControlFlagMask | version);
  }

  SpdyControlType type() const {
    uint16 type = ntohs(block()->control_.type_);
    DCHECK(type >= SYN_STREAM && type < NUM_CONTROL_FRAME_TYPES);
    return static_cast<SpdyControlType>(type);
  }

  void set_type(SpdyControlType type) {
    DCHECK(type >= SYN_STREAM && type < NUM_CONTROL_FRAME_TYPES);
    mutable_block()->control_.type_ = htons(type);
  }

  // Returns true if this control frame is of a type that has a header block,
  // otherwise it returns false.
  bool has_header_block() const {
    return type() == SYN_STREAM || type() == SYN_REPLY || type() == HEADERS;
  }

  // Returns the size of the SpdyFrameBlock structure.
  // Note: this is not the size of the SpdyControlFrame class.
  static size_t size() { return sizeof(SpdyFrameBlock); }

  // The size of the 'Number of Name/Value pairs' field in a Name/Value block.
  static const size_t kNumNameValuePairsSize = 2;

  // The size of the 'Length of a name' field in a Name/Value block.
  static const size_t kLengthOfNameSize = 2;

  // The size of the 'Length of a value' field in a Name/Value block.
  static const size_t kLengthOfValueSize = 2;

 private:
  const struct SpdyFrameBlock* block() const {
    return frame_;
  }
  struct SpdyFrameBlock* mutable_block() {
    return frame_;
  }
  DISALLOW_COPY_AND_ASSIGN(SpdyControlFrame);
};

// A SYN_STREAM frame.
class SpdySynStreamControlFrame : public SpdyControlFrame {
 public:
  SpdySynStreamControlFrame() : SpdyControlFrame(size()) {}
  SpdySynStreamControlFrame(char* data, bool owns_buffer)
      : SpdyControlFrame(data, owns_buffer) {}

  SpdyStreamId stream_id() const {
    return ntohl(block()->stream_id_) & kStreamIdMask;
  }

  void set_stream_id(SpdyStreamId id) {
    mutable_block()->stream_id_ = htonl(id & kStreamIdMask);
  }

  SpdyStreamId associated_stream_id() const {
    return ntohl(block()->associated_stream_id_) & kStreamIdMask;
  }

  void set_associated_stream_id(SpdyStreamId id) {
    mutable_block()->associated_stream_id_ = htonl(id & kStreamIdMask);
  }

  SpdyPriority priority() const {
    return (block()->priority_ & kPriorityMask) >> 6;
  }

  // The number of bytes in the header block beyond the frame header length.
  int header_block_len() const {
    return length() - (size() - SpdyFrame::size());
  }

  const char* header_block() const {
    return reinterpret_cast<const char*>(block()) + size();
  }

  // Returns the size of the SpdySynStreamControlFrameBlock structure.
  // Note: this is not the size of the SpdySynStreamControlFrame class.
  static size_t size() { return sizeof(SpdySynStreamControlFrameBlock); }

 private:
  const struct SpdySynStreamControlFrameBlock* block() const {
    return static_cast<SpdySynStreamControlFrameBlock*>(frame_);
  }
  struct SpdySynStreamControlFrameBlock* mutable_block() {
    return static_cast<SpdySynStreamControlFrameBlock*>(frame_);
  }
  DISALLOW_COPY_AND_ASSIGN(SpdySynStreamControlFrame);
};

// A SYN_REPLY frame.
class SpdySynReplyControlFrame : public SpdyControlFrame {
 public:
  SpdySynReplyControlFrame() : SpdyControlFrame(size()) {}
  SpdySynReplyControlFrame(char* data, bool owns_buffer)
      : SpdyControlFrame(data, owns_buffer) {}

  SpdyStreamId stream_id() const {
    return ntohl(block()->stream_id_) & kStreamIdMask;
  }

  void set_stream_id(SpdyStreamId id) {
    mutable_block()->stream_id_ = htonl(id & kStreamIdMask);
  }

  int header_block_len() const {
    return length() - (size() - SpdyFrame::size());
  }

  const char* header_block() const {
    return reinterpret_cast<const char*>(block()) + size();
  }

  // Returns the size of the SpdySynReplyControlFrameBlock structure.
  // Note: this is not the size of the SpdySynReplyControlFrame class.
  static size_t size() { return sizeof(SpdySynReplyControlFrameBlock); }

 private:
  const struct SpdySynReplyControlFrameBlock* block() const {
    return static_cast<SpdySynReplyControlFrameBlock*>(frame_);
  }
  struct SpdySynReplyControlFrameBlock* mutable_block() {
    return static_cast<SpdySynReplyControlFrameBlock*>(frame_);
  }
  DISALLOW_COPY_AND_ASSIGN(SpdySynReplyControlFrame);
};

// A RST_STREAM frame.
class SpdyRstStreamControlFrame : public SpdyControlFrame {
 public:
  SpdyRstStreamControlFrame() : SpdyControlFrame(size()) {}
  SpdyRstStreamControlFrame(char* data, bool owns_buffer)
      : SpdyControlFrame(data, owns_buffer) {}

  SpdyStreamId stream_id() const {
    return ntohl(block()->stream_id_) & kStreamIdMask;
  }

  void set_stream_id(SpdyStreamId id) {
    mutable_block()->stream_id_ = htonl(id & kStreamIdMask);
  }

  SpdyStatusCodes status() const {
    SpdyStatusCodes status =
        static_cast<SpdyStatusCodes>(ntohl(block()->status_));
    if (status < INVALID || status >= NUM_STATUS_CODES) {
      status = INVALID;
    }
    return status;
  }
  void set_status(SpdyStatusCodes status) {
    mutable_block()->status_ = htonl(static_cast<uint32>(status));
  }

  // Returns the size of the SpdyRstStreamControlFrameBlock structure.
  // Note: this is not the size of the SpdyRstStreamControlFrame class.
  static size_t size() { return sizeof(SpdyRstStreamControlFrameBlock); }

 private:
  const struct SpdyRstStreamControlFrameBlock* block() const {
    return static_cast<SpdyRstStreamControlFrameBlock*>(frame_);
  }
  struct SpdyRstStreamControlFrameBlock* mutable_block() {
    return static_cast<SpdyRstStreamControlFrameBlock*>(frame_);
  }
  DISALLOW_COPY_AND_ASSIGN(SpdyRstStreamControlFrame);
};

class SpdySettingsControlFrame : public SpdyControlFrame {
 public:
  SpdySettingsControlFrame() : SpdyControlFrame(size()) {}
  SpdySettingsControlFrame(char* data, bool owns_buffer)
      : SpdyControlFrame(data, owns_buffer) {}

  uint32 num_entries() const {
    return ntohl(block()->num_entries_);
  }

  void set_num_entries(int val) {
    mutable_block()->num_entries_ = htonl(val);
  }

  int header_block_len() const {
    return length() - (size() - SpdyFrame::size());
  }

  const char* header_block() const {
    return reinterpret_cast<const char*>(block()) + size();
  }

  // Returns the size of the SpdySettingsControlFrameBlock structure.
  // Note: this is not the size of the SpdySettingsControlFrameBlock class.
  static size_t size() { return sizeof(SpdySettingsControlFrameBlock); }

 private:
  const struct SpdySettingsControlFrameBlock* block() const {
    return static_cast<SpdySettingsControlFrameBlock*>(frame_);
  }
  struct SpdySettingsControlFrameBlock* mutable_block() {
    return static_cast<SpdySettingsControlFrameBlock*>(frame_);
  }
  DISALLOW_COPY_AND_ASSIGN(SpdySettingsControlFrame);
};

class SpdyNoOpControlFrame : public SpdyControlFrame {
 public:
  SpdyNoOpControlFrame() : SpdyControlFrame(size()) {}
  SpdyNoOpControlFrame(char* data, bool owns_buffer)
      : SpdyControlFrame(data, owns_buffer) {}

  static size_t size() { return sizeof(SpdyNoopControlFrameBlock); }
};

class SpdyPingControlFrame : public SpdyControlFrame {
 public:
  SpdyPingControlFrame() : SpdyControlFrame(size()) {}
  SpdyPingControlFrame(char* data, bool owns_buffer)
      : SpdyControlFrame(data, owns_buffer) {}

  uint32 unique_id() const {
    return ntohl(block()->unique_id_);
  }

  void set_unique_id(uint32 unique_id) {
    mutable_block()->unique_id_ = htonl(unique_id);
  }

  static size_t size() { return sizeof(SpdyPingControlFrameBlock); }

 private:
  const struct SpdyPingControlFrameBlock* block() const {
    return static_cast<SpdyPingControlFrameBlock*>(frame_);
  }
  struct SpdyPingControlFrameBlock* mutable_block() {
    return static_cast<SpdyPingControlFrameBlock*>(frame_);
  }
};

class SpdyGoAwayControlFrame : public SpdyControlFrame {
 public:
  SpdyGoAwayControlFrame() : SpdyControlFrame(size()) {}
  SpdyGoAwayControlFrame(char* data, bool owns_buffer)
      : SpdyControlFrame(data, owns_buffer) {}

  SpdyStreamId last_accepted_stream_id() const {
    return ntohl(block()->last_accepted_stream_id_) & kStreamIdMask;
  }

  void set_last_accepted_stream_id(SpdyStreamId id) {
    mutable_block()->last_accepted_stream_id_ = htonl(id & kStreamIdMask);
  }

  static size_t size() { return sizeof(SpdyGoAwayControlFrameBlock); }

 private:
  const struct SpdyGoAwayControlFrameBlock* block() const {
    return static_cast<SpdyGoAwayControlFrameBlock*>(frame_);
  }
  struct SpdyGoAwayControlFrameBlock* mutable_block() {
    return static_cast<SpdyGoAwayControlFrameBlock*>(frame_);
  }
  DISALLOW_COPY_AND_ASSIGN(SpdyGoAwayControlFrame);
};

// A HEADERS frame.
class SpdyHeadersControlFrame : public SpdyControlFrame {
 public:
  SpdyHeadersControlFrame() : SpdyControlFrame(size()) {}
  SpdyHeadersControlFrame(char* data, bool owns_buffer)
      : SpdyControlFrame(data, owns_buffer) {}

  SpdyStreamId stream_id() const {
    return ntohl(block()->stream_id_) & kStreamIdMask;
  }

  void set_stream_id(SpdyStreamId id) {
    mutable_block()->stream_id_ = htonl(id & kStreamIdMask);
  }

  // The number of bytes in the header block beyond the frame header length.
  int header_block_len() const {
    return length() - (size() - SpdyFrame::size());
  }

  const char* header_block() const {
    return reinterpret_cast<const char*>(block()) + size();
  }

  // Returns the size of the SpdyHeadersControlFrameBlock structure.
  // Note: this is not the size of the SpdyHeadersControlFrame class.
  static size_t size() { return sizeof(SpdyHeadersControlFrameBlock); }

 private:
  const struct SpdyHeadersControlFrameBlock* block() const {
    return static_cast<SpdyHeadersControlFrameBlock*>(frame_);
  }
  struct SpdyHeadersControlFrameBlock* mutable_block() {
    return static_cast<SpdyHeadersControlFrameBlock*>(frame_);
  }
  DISALLOW_COPY_AND_ASSIGN(SpdyHeadersControlFrame);
};

// A WINDOW_UPDATE frame.
class SpdyWindowUpdateControlFrame : public SpdyControlFrame {
 public:
  SpdyWindowUpdateControlFrame() : SpdyControlFrame(size()) {}
  SpdyWindowUpdateControlFrame(char* data, bool owns_buffer)
      : SpdyControlFrame(data, owns_buffer) {}

  SpdyStreamId stream_id() const {
    return ntohl(block()->stream_id_) & kStreamIdMask;
  }

  void set_stream_id(SpdyStreamId id) {
    mutable_block()->stream_id_ = htonl(id & kStreamIdMask);
  }

  uint32 delta_window_size() const {
    return ntohl(block()->delta_window_size_);
  }

  void set_delta_window_size(uint32 delta_window_size) {
    mutable_block()->delta_window_size_ = htonl(delta_window_size);
  }

  // Returns the size of the SpdyWindowUpdateControlFrameBlock structure.
  // Note: this is not the size of the SpdyWindowUpdateControlFrame class.
  static size_t size() { return sizeof(SpdyWindowUpdateControlFrameBlock); }

 private:
  const struct SpdyWindowUpdateControlFrameBlock* block() const {
    return static_cast<SpdyWindowUpdateControlFrameBlock*>(frame_);
  }
  struct SpdyWindowUpdateControlFrameBlock* mutable_block() {
    return static_cast<SpdyWindowUpdateControlFrameBlock*>(frame_);
  }

  DISALLOW_COPY_AND_ASSIGN(SpdyWindowUpdateControlFrame);
};

}  // namespace spdy

#endif  // NET_SPDY_SPDY_PROTOCOL_H_
