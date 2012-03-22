// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "net/spdy/spdy_frame_builder.h"
#include "net/spdy/spdy_protocol.h"

namespace net {

SpdyFrameBuilder::SpdyFrameBuilder(size_t size)
    : buffer_(NULL),
      capacity_(0),
      length_(0),
      variable_buffer_offset_(0) {
  Resize(size);
}

SpdyFrameBuilder::~SpdyFrameBuilder() {
  if (buffer_)
    delete[] buffer_;
}

char* SpdyFrameBuilder::BeginWrite(size_t length) {
  size_t offset = length_;
  size_t needed_size = length_ + length;
  if (needed_size > capacity_ && !Resize(std::max(capacity_ * 2, needed_size)))
    return NULL;

#ifdef ARCH_CPU_64_BITS
  DCHECK_LE(length, std::numeric_limits<uint32>::max());
#endif

  return buffer_ + offset;
}

void SpdyFrameBuilder::EndWrite(char* dest, int length) {
}

bool SpdyFrameBuilder::WriteBytes(const void* data, uint32 data_len) {
  if (data_len > kLengthMask) {
    return false;
  }

  char* dest = BeginWrite(data_len);
  if (!dest)
    return false;

  memcpy(dest, data, data_len);

  EndWrite(dest, data_len);
  length_ += data_len;
  return true;
}

bool SpdyFrameBuilder::WriteString(const std::string& value) {
  if (value.size() > 0xffff)
    return false;

  if (!WriteUInt16(static_cast<int>(value.size())))
    return false;

  return WriteBytes(value.data(), static_cast<uint16>(value.size()));
}

bool SpdyFrameBuilder::WriteStringPiece32(const base::StringPiece& value) {
  if (!WriteUInt32(value.size())) {
    return false;
  }

  return WriteBytes(value.data(), value.size());
}

// TODO(hkhalil) Remove Resize() entirely.
bool SpdyFrameBuilder::Resize(size_t new_capacity) {
  DCHECK(new_capacity > 0);
  if (new_capacity < capacity_)
    return true;

  char* p = new char[new_capacity];
  if (buffer_) {
    memcpy(p, buffer_, capacity_);
    delete[] buffer_;
  }
  if (!p && new_capacity > 0)
    return false;
  buffer_ = p;
  capacity_ = new_capacity;
  return true;
}

}  // namespace net
