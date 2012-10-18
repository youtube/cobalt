// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_data_writer.h"

#include <algorithm>
#include <limits>

#include "base/basictypes.h"
#include "base/logging.h"
#include "net/quic/quic_protocol.h"

using std::numeric_limits;

namespace net {

QuicDataWriter::QuicDataWriter(size_t size)
    : buffer_(new char[size]),
      capacity_(size),
      length_(0) {
}

QuicDataWriter::~QuicDataWriter() {
  delete[] buffer_;
}

char* QuicDataWriter::BeginWrite(size_t length) {
  if (capacity_ - length_ < length) {
    return NULL;
  }

#ifdef ARCH_CPU_64_BITS
  DCHECK_LE(length, numeric_limits<uint32>::max());
#endif

  return buffer_ + length_;
}

bool QuicDataWriter::AdvancePointer(uint32 len) {
  if (!BeginWrite(len)) {
    return false;
  }
  length_ += len;
  return true;
}

bool QuicDataWriter::WriteBytes(const void* data, uint32 data_len) {
  char* dest = BeginWrite(data_len);
  if (!dest) {
    return false;
  }

  memcpy(dest, data, data_len);

  length_ += data_len;
  return true;
}

void QuicDataWriter::WriteUint64ToBuffer(uint64 value, char* buffer) {
  memcpy(buffer, &value, sizeof(value));
}

void QuicDataWriter::WriteUint128ToBuffer(uint128 value, char* buffer) {
  WriteUint64ToBuffer(value.lo, buffer);
  WriteUint64ToBuffer(value.hi, buffer + sizeof(value.lo));
}

}  // namespace net
