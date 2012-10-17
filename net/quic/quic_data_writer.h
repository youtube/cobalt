// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_DATA_WRITER_H_
#define NET_QUIC_QUIC_DATA_WRITER_H_

#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string_piece.h"
#include "net/base/net_export.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/uint128.h"

namespace net {

// This class provides facilities for packing QUIC data.
//
// The QuicDataWriter supports appending primitive values (int, string, etc)
// to a frame instance.  The QuicDataWriter grows its internal memory buffer
// dynamically to hold the sequence of primitive values.   The internal memory
// buffer is exposed as the "data" of the QuicDataWriter.
class NET_EXPORT_PRIVATE QuicDataWriter {
 public:
  explicit QuicDataWriter(size_t length);
  ~QuicDataWriter();

  // Returns the size of the QuicDataWriter's data.
  size_t length() const { return length_; }

  // Takes the buffer from the QuicDataWriter.
  // TODO(rch): move non-trivial methods into .cc file.
  char* take() {
    char* rv = buffer_;
    buffer_ = NULL;
    capacity_ = 0;
    length_ = 0;
    return rv;
  }

  // Methods for adding to the payload.  These values are appended to the end
  // of the QuicDataWriter payload. Note - binary integers are written in
  // host byte order (little endian) not network byte order (big endian).
  bool WriteUInt8(uint8 value) {
    return WriteBytes(&value, sizeof(value));
  }
  bool WriteUInt16(uint16 value) {
    return WriteBytes(&value, sizeof(value));
  }
  bool WriteUInt32(uint32 value) {
    return WriteBytes(&value, sizeof(value));
  }
  bool WriteUInt48(uint64 value) {
    uint32 hi = value >> 32;
    uint32 lo = value & 0x00000000FFFFFFFF;
    return WriteUInt32(lo) && WriteUInt16(hi);
  }
  bool WriteUInt64(uint64 value) {
    return WriteBytes(&value, sizeof(value));
  }
  bool WriteUInt128(uint128 value) {
    return WriteUInt64(value.lo) && WriteUInt64(value.hi);
  }

  bool AdvancePointer(uint32 len);

  bool WriteBytes(const void* data, uint32 data_len);

  static void WriteUint64ToBuffer(uint64 value, char* buffer);
  static void WriteUint128ToBuffer(uint128 value, char* buffer);

  size_t capacity() const {
    return capacity_;
  }

 protected:
  const char* end_of_payload() const { return buffer_ + length_; }

 private:
  // Returns the location that the data should be written at, or NULL if there
  // is not enough room. Call EndWrite with the returned offset and the given
  // length to pad out for the next write.
  char* BeginWrite(size_t length);

  char* buffer_;
  size_t capacity_;  // Allocation size of payload (or -1 if buffer is const).
  size_t length_;    // Current length of the buffer.
};

}  // namespace net

#endif  // NET_QUIC_QUIC_DATA_WRITER_H_
