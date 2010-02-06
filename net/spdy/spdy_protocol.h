// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains some protocol structures for use with Flip.

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

namespace flip {

// This implementation of Flip is version 1.
const int kFlipProtocolVersion = 1;

// Note: all protocol data structures are on-the-wire format.  That means that
//       data is stored in network-normalized order.  Readers must use the
//       accessors provided or call ntohX() functions.

// Types of Flip Control Frames.
enum FlipControlType {
  SYN_STREAM = 1,
  SYN_REPLY,
  FIN_STREAM,
  NOOP
};

// Flags on data packets
enum FlipDataFlags {
  DATA_FLAG_NONE = 0,
  DATA_FLAG_FIN = 1,
  DATA_FLAG_COMPRESSED = 2  // TODO(mbelshe): remove me.
};

// Flags on control packets
enum FlipControlFlags {
  CONTROL_FLAG_NONE = 0,
  CONTROL_FLAG_FIN = 1
};

// A FLIP stream id is a 31 bit entity.
typedef uint32 FlipStreamId;

// FLIP Priorities. (there are only 2 bits)
#define FLIP_PRIORITY_LOWEST 3
#define FLIP_PRIORITY_HIGHEST 0

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

// The basic FLIP Frame structure.
struct FlipFrameBlock {
  union {
    struct {
      uint16 version_;
      uint16 type_;
    } control_;
    struct {
      FlipStreamId stream_id_;
    } data_;
  };
  FlagsAndLength flags_length_;
};

// A Control Frame structure.
struct FlipControlFrameBlock : FlipFrameBlock {
  FlipStreamId stream_id_;
};

// A SYN_STREAM Control Frame structure.
struct FlipSynStreamControlFrameBlock : FlipControlFrameBlock {
  uint8 priority_;
  uint8 unused_;
};

// A SYN_REPLY Control Frame structure.
struct FlipSynReplyControlFrameBlock : FlipControlFrameBlock {
  uint16 unused_;
};

// A FNI_STREAM Control Frame structure.
struct FlipFinStreamControlFrameBlock : FlipControlFrameBlock {
  uint32 status_;
};

#pragma pack(pop)

// -------------------------------------------------------------------------
// Wrapper classes for various FLIP frames.

// All Flip Frame types derive from this FlipFrame class.
class FlipFrame {
 public:
  // Create a FlipFrame for a given sized buffer.
  explicit FlipFrame(size_t size) : frame_(NULL), owns_buffer_(true) {
    DCHECK_GE(size, sizeof(struct FlipFrameBlock));
    char* buffer = new char[size];
    memset(buffer, 0, size);
    frame_ = reinterpret_cast<struct FlipFrameBlock*>(buffer);
  }

  // Create a FlipFrame using a pre-created buffer.
  // If |owns_buffer| is true, this class takes ownership of the buffer
  // and will delete it on cleanup.  The buffer must have been created using
  // new char[].
  // If |owns_buffer| is false, the caller retains ownership of the buffer and
  // is responsible for making sure the buffer outlives this frame.  In other
  // words, this class does NOT create a copy of the buffer.
  FlipFrame(char* data, bool owns_buffer)
      : frame_(reinterpret_cast<struct FlipFrameBlock*>(data)),
        owns_buffer_(owns_buffer) {
    DCHECK(frame_);
  }

  virtual ~FlipFrame() {
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

  // Returns the size of the FlipFrameBlock structure.
  // Note: this is not the size of the FlipFrame class.
  // Every FlipFrame* class has a static size() method for accessing
  // the size of the data structure which will be sent over the wire.
  // Note:  this is not the same as sizeof(FlipFrame).
  static size_t size() { return sizeof(struct FlipFrameBlock); }

 protected:
  FlipFrameBlock* frame_;

 private:
  bool owns_buffer_;
  DISALLOW_COPY_AND_ASSIGN(FlipFrame);
};

// A Data Frame.
class FlipDataFrame : public FlipFrame {
 public:
  FlipDataFrame() : FlipFrame(size()) {}
  FlipDataFrame(char* data, bool owns_buffer)
      : FlipFrame(data, owns_buffer) {}
  virtual ~FlipDataFrame() {}

  FlipStreamId stream_id() const {
    return ntohl(frame_->data_.stream_id_) & kStreamIdMask;
  }

  // Note that setting the stream id sets the control bit to false.
  // As stream id should always be set, this means the control bit
  // should always be set correctly.
  void set_stream_id(FlipStreamId id) {
    DCHECK_EQ(0u, (id & ~kStreamIdMask));
    frame_->data_.stream_id_ = htonl(id & kStreamIdMask);
  }

  // Returns the size of the FlipFrameBlock structure.
  // Note: this is not the size of the FlipDataFrame class.
  static size_t size() { return FlipFrame::size(); }

 private:
  DISALLOW_COPY_AND_ASSIGN(FlipDataFrame);
};

// A Control Frame.
class FlipControlFrame : public FlipFrame {
 public:
  explicit FlipControlFrame(size_t size) : FlipFrame(size) {}
  FlipControlFrame(char* data, bool owns_buffer)
      : FlipFrame(data, owns_buffer) {}
  virtual ~FlipControlFrame() {}

  uint16 version() const {
    const int kVersionMask = 0x7fff;
    return ntohs(block()->control_.version_) & kVersionMask;
  }
  FlipControlType type() const {
    uint16 type = ntohs(block()->control_.type_);
    DCHECK(type >= SYN_STREAM && type <= NOOP);
    return static_cast<FlipControlType>(type);
  }
  FlipStreamId stream_id() const {
    return ntohl(block()->stream_id_) & kStreamIdMask;
  }

  void set_stream_id(FlipStreamId id) {
    block()->stream_id_ = htonl(id & kStreamIdMask);
  }

  // Returns the size of the FlipControlFrameBlock structure.
  // Note: this is not the size of the FlipControlFrame class.
  static size_t size() { return sizeof(FlipControlFrameBlock); }

 private:
  struct FlipControlFrameBlock* block() const {
    return static_cast<FlipControlFrameBlock*>(frame_);
  }
  DISALLOW_COPY_AND_ASSIGN(FlipControlFrame);
};

// A SYN_STREAM frame.
class FlipSynStreamControlFrame : public FlipControlFrame {
 public:
  FlipSynStreamControlFrame() : FlipControlFrame(size()) {}
  FlipSynStreamControlFrame(char* data, bool owns_buffer)
      : FlipControlFrame(data, owns_buffer) {}
  virtual ~FlipSynStreamControlFrame() {}

  uint8 priority() const { return (block()->priority_ & kPriorityMask) >> 6; }

  // The number of bytes in the header block beyond the frame header length.
  int header_block_len() const {
    return length() - (size() - FlipFrame::size());
  }

  const char* header_block() const {
    return reinterpret_cast<const char*>(block()) + size();
  }

  // Returns the size of the FlipSynStreamControlFrameBlock structure.
  // Note: this is not the size of the FlipSynStreamControlFrame class.
  static size_t size() { return sizeof(FlipSynStreamControlFrameBlock); }

 private:
  struct FlipSynStreamControlFrameBlock* block() const {
    return static_cast<FlipSynStreamControlFrameBlock*>(frame_);
  }
  DISALLOW_COPY_AND_ASSIGN(FlipSynStreamControlFrame);
};

// A SYN_REPLY frame.
class FlipSynReplyControlFrame : public FlipControlFrame {
 public:
  FlipSynReplyControlFrame() : FlipControlFrame(size()) {}
  FlipSynReplyControlFrame(char* data, bool owns_buffer)
      : FlipControlFrame(data, owns_buffer) {}
  virtual ~FlipSynReplyControlFrame() {}

  int header_block_len() const {
    return length() - (size() - FlipFrame::size());
  }

  const char* header_block() const {
    return reinterpret_cast<const char*>(block()) + size();
  }

  // Returns the size of the FlipSynReplyControlFrameBlock structure.
  // Note: this is not the size of the FlipSynReplyControlFrame class.
  static size_t size() { return sizeof(FlipSynReplyControlFrameBlock); }

 private:
  struct FlipSynReplyControlFrameBlock* block() const {
    return static_cast<FlipSynReplyControlFrameBlock*>(frame_);
  }
  DISALLOW_COPY_AND_ASSIGN(FlipSynReplyControlFrame);
};

// A FIN_STREAM frame.
class FlipFinStreamControlFrame : public FlipControlFrame {
 public:
  FlipFinStreamControlFrame() : FlipControlFrame(size()) {}
  FlipFinStreamControlFrame(char* data, bool owns_buffer)
      : FlipControlFrame(data, owns_buffer) {}
  virtual ~FlipFinStreamControlFrame() {}

  uint32 status() const { return ntohl(block()->status_); }
  void set_status(uint32 status) { block()->status_ = htonl(status); }

  // Returns the size of the FlipFinStreamControlFrameBlock structure.
  // Note: this is not the size of the FlipFinStreamControlFrame class.
  static size_t size() { return sizeof(FlipFinStreamControlFrameBlock); }

 private:
  struct FlipFinStreamControlFrameBlock* block() const {
    return static_cast<FlipFinStreamControlFrameBlock*>(frame_);
  }
  DISALLOW_COPY_AND_ASSIGN(FlipFinStreamControlFrame);
};

}  // namespace flip

#endif  // NET_SPDY_SPDY_PROTOCOL_H_
