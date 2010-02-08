// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains some protocol structures for use with Spdy.

#ifndef NET_SPDY_SPDY_PROTOCOL_H_
#define NET_SPDY_SPDY_PROTOCOL_H_

#ifdef WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include "base/basictypes.h"
#include "base/logging.h"
#include "spdy_bitmasks.h"  // cross-google3 directory naming.

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
//  | flags (8)  |  Length (24 bits)   |  >= 8
//  +----------------------------------+
//  |X|       Stream-ID(31bits)        |
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
//  Control Frame: FIN_STREAM
//  +----------------------------------+
//  |1|000000000000001|0000000000000011|
//  +----------------------------------+
//  | flags (8)  |  Length (24 bits)   |  >= 4
//  +----------------------------------+
//  |X|       Stream-ID(31bits)        |
//  +----------------------------------+
//  |             Status (32 bits)     |
//  +----------------------------------+
//
//  Control Frame: SetMaxStreams
//  +----------------------------------+
//  |1|000000000000001|0000000000000100|
//  +----------------------------------+
//  | flags (8)  |  Length (24 bits)   |  >= 4
//  +----------------------------------+
//  |X|       Stream-ID(31bits)        |
//  +----------------------------------+

// TODO(fenix): add ChangePriority support.

namespace spdy {

// This implementation of Spdy is version 1.
const int kSpdyProtocolVersion = 1;

// Note: all protocol data structures are on-the-wire format.  That means that
//       data is stored in network-normalized order.  Readers must use the
//       accessors provided or call ntohX() functions.

// Types of Spdy Control Frames.
enum SpdyControlType {
  SYN_STREAM = 1,
  SYN_REPLY,
  FIN_STREAM,
  NOOP
};

// Flags on data packets
enum SpdyDataFlags {
  DATA_FLAG_NONE = 0,
  DATA_FLAG_FIN = 1,
  DATA_FLAG_COMPRESSED = 2  // TODO(mbelshe): remove me.
};

// Flags on control packets
enum SpdyControlFlags {
  CONTROL_FLAG_NONE = 0,
  CONTROL_FLAG_FIN = 1
};

// A Spdy stream id is a 31 bit entity.
typedef uint32 SpdyStreamId;

// Spdy Priorities. (there are only 2 bits)
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

// The basic Spdy Frame structure.
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

// A Control Frame structure.
struct SpdyControlFrameBlock : SpdyFrameBlock {
  SpdyStreamId stream_id_;
};

// A SYN_STREAM Control Frame structure.
struct SpdySynStreamControlFrameBlock : SpdyControlFrameBlock {
  uint8 priority_;
  uint8 unused_;
};

// A SYN_REPLY Control Frame structure.
struct SpdySynReplyControlFrameBlock : SpdyControlFrameBlock {
  uint16 unused_;
};

// A FNI_STREAM Control Frame structure.
struct SpdyFinStreamControlFrameBlock : SpdyControlFrameBlock {
  uint32 status_;
};

#pragma pack(pop)

// -------------------------------------------------------------------------
// Wrapper classes for various Spdy frames.

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

  virtual ~SpdyFrame() {
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
  // Note: this is not the size of the SpdyFrame class.
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
  virtual ~SpdyDataFrame() {}

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

 private:
  DISALLOW_COPY_AND_ASSIGN(SpdyDataFrame);
};

// A Control Frame.
class SpdyControlFrame : public SpdyFrame {
 public:
  explicit SpdyControlFrame(size_t size) : SpdyFrame(size) {}
  SpdyControlFrame(char* data, bool owns_buffer)
      : SpdyFrame(data, owns_buffer) {}
  virtual ~SpdyControlFrame() {}

  uint16 version() const {
    const int kVersionMask = 0x7fff;
    return ntohs(block()->control_.version_) & kVersionMask;
  }
  SpdyControlType type() const {
    uint16 type = ntohs(block()->control_.type_);
    DCHECK(type >= SYN_STREAM && type <= NOOP);
    return static_cast<SpdyControlType>(type);
  }
  SpdyStreamId stream_id() const {
    return ntohl(block()->stream_id_) & kStreamIdMask;
  }

  void set_stream_id(SpdyStreamId id) {
    block()->stream_id_ = htonl(id & kStreamIdMask);
  }

  // Returns the size of the SpdyControlFrameBlock structure.
  // Note: this is not the size of the SpdyControlFrame class.
  static size_t size() { return sizeof(SpdyControlFrameBlock); }

 private:
  struct SpdyControlFrameBlock* block() const {
    return static_cast<SpdyControlFrameBlock*>(frame_);
  }
  DISALLOW_COPY_AND_ASSIGN(SpdyControlFrame);
};

// A SYN_STREAM frame.
class SpdySynStreamControlFrame : public SpdyControlFrame {
 public:
  SpdySynStreamControlFrame() : SpdyControlFrame(size()) {}
  SpdySynStreamControlFrame(char* data, bool owns_buffer)
      : SpdyControlFrame(data, owns_buffer) {}
  virtual ~SpdySynStreamControlFrame() {}

  uint8 priority() const { return (block()->priority_ & kPriorityMask) >> 6; }

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
  struct SpdySynStreamControlFrameBlock* block() const {
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
  virtual ~SpdySynReplyControlFrame() {}

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
  struct SpdySynReplyControlFrameBlock* block() const {
    return static_cast<SpdySynReplyControlFrameBlock*>(frame_);
  }
  DISALLOW_COPY_AND_ASSIGN(SpdySynReplyControlFrame);
};

// A FIN_STREAM frame.
class SpdyFinStreamControlFrame : public SpdyControlFrame {
 public:
  SpdyFinStreamControlFrame() : SpdyControlFrame(size()) {}
  SpdyFinStreamControlFrame(char* data, bool owns_buffer)
      : SpdyControlFrame(data, owns_buffer) {}
  virtual ~SpdyFinStreamControlFrame() {}

  uint32 status() const { return ntohl(block()->status_); }
  void set_status(uint32 status) { block()->status_ = htonl(status); }

  // Returns the size of the SpdyFinStreamControlFrameBlock structure.
  // Note: this is not the size of the SpdyFinStreamControlFrame class.
  static size_t size() { return sizeof(SpdyFinStreamControlFrameBlock); }

 private:
  struct SpdyFinStreamControlFrameBlock* block() const {
    return static_cast<SpdyFinStreamControlFrameBlock*>(frame_);
  }
  DISALLOW_COPY_AND_ASSIGN(SpdyFinStreamControlFrame);
};

}  // namespace spdy

#endif  // NET_SPDY_SPDY_PROTOCOL_H_
