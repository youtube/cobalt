// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SPDY_SPDY_FRAME_BUILDER_H_
#define NET_SPDY_SPDY_FRAME_BUILDER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/string_piece.h"
#include "base/sys_byteorder.h"
#include "net/base/net_export.h"
#include "net/spdy/spdy_protocol.h"

namespace net {

// This class provides facilities for basic binary value packing and unpacking
// into Spdy frames.
//
// The SpdyFrameBuilder supports appending primitive values (int, string, etc)
// to a frame instance.  The SpdyFrameBuilder grows its internal memory buffer
// dynamically to hold the sequence of primitive values.   The internal memory
// buffer is exposed as the "data" of the SpdyFrameBuilder.
class NET_EXPORT_PRIVATE SpdyFrameBuilder {
 public:
  ~SpdyFrameBuilder();

  SpdyFrameBuilder();

  // Initiailizes a SpdyFrameBuilder with a buffer of given size.
  // The buffer will still be resized as necessary.
  explicit SpdyFrameBuilder(size_t size);

  // Returns the size of the SpdyFrameBuilder's data.
  int length() const { return length_; }

  // Takes the buffer from the SpdyFrameBuilder.
  SpdyFrame* take() {
    SpdyFrame* rv = new SpdyFrame(buffer_, true);
    buffer_ = NULL;
    capacity_ = 0;
    length_ = 0;
    return rv;
  }

  // Methods for adding to the payload.  These values are appended to the end
  // of the SpdyFrameBuilder payload. Note - binary integers are converted from
  // host to network form.
  bool WriteUInt8(uint8 value) {
    return WriteBytes(&value, sizeof(value));
  }
  bool WriteUInt16(uint16 value) {
    value = htons(value);
    return WriteBytes(&value, sizeof(value));
  }
  bool WriteUInt32(uint32 value) {
    value = htonl(value);
    return WriteBytes(&value, sizeof(value));
  }
  // TODO(hkhalil) Rename to WriteStringPiece16().
  bool WriteString(const std::string& value);
  bool WriteStringPiece32(const base::StringPiece& value);
  bool WriteBytes(const void* data, uint32 data_len);

  // Write an integer to a particular offset in the data buffer.
  bool WriteUInt32ToOffset(int offset, uint32 value) {
    value = htonl(value);
    return WriteBytesToOffset(offset, &value, sizeof(value));
  }

  // Write to a particular offset in the data buffer.
  bool WriteBytesToOffset(int offset, const void* data, uint32 data_len) {
    if (offset + data_len > length_)
      return false;
    char *ptr = buffer_ + offset;
    memcpy(ptr, data, data_len);
    return true;
  }

  // Returns true if the given iterator could point to data with the given
  // length. If there is no room for the given data before the end of the
  // payload, returns false.
  bool IteratorHasRoomFor(const void* iter, int len) const {
    const char* end_of_region = reinterpret_cast<const char*>(iter) + len;
    if (len < 0 ||
        iter < buffer_ ||
        iter > end_of_payload() ||
        iter > end_of_region ||
        end_of_region > end_of_payload())
      return false;

    // Watch out for overflow in pointer calculation, which wraps.
    return (iter <= end_of_region) && (end_of_region <= end_of_payload());
  }

 protected:
  size_t capacity() const {
    return capacity_;
  }

  const char* end_of_payload() const { return buffer_ + length_; }

  // Resizes the buffer for use when writing the specified amount of data. The
  // location that the data should be written at is returned, or NULL if there
  // was an error. Call EndWrite with the returned offset and the given length
  // to pad out for the next write.
  char* BeginWrite(size_t length);

  // Completes the write operation by padding the data with NULL bytes until it
  // is padded. Should be paired with BeginWrite, but it does not necessarily
  // have to be called after the data is written.
  void EndWrite(char* dest, int length);

  // Resize the capacity, note that the input value should include the size of
  // the header: new_capacity = sizeof(Header) + desired_payload_capacity.
  // A new failure will cause a Resize failure... and caller should check
  // the return result for true (i.e., successful resizing).
  bool Resize(size_t new_capacity);

  // Moves the iterator by the given number of bytes.
  static void UpdateIter(void** iter, int bytes) {
    *iter = static_cast<char*>(*iter) + bytes;
  }

 private:
  char* buffer_;
  size_t capacity_;  // Allocation size of payload (or -1 if buffer is const).
  size_t length_;    // current length of the buffer
  size_t variable_buffer_offset_;  // IF non-zero, then offset to a buffer.
};

}  // namespace net

#endif  // NET_SPDY_SPDY_FRAME_BUILDER_H_
