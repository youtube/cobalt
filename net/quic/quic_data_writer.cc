// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_data_writer.h"

#include <algorithm>
#include <limits>

#include "base/basictypes.h"
#include "base/logging.h"

using base::StringPiece;
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

char* QuicDataWriter::take() {
  char* rv = buffer_;
  buffer_ = NULL;
  capacity_ = 0;
  length_ = 0;
  return rv;
}

bool QuicDataWriter::WriteUInt8(uint8 value) {
  return WriteBytes(&value, sizeof(value));
}

bool QuicDataWriter::WriteUInt16(uint16 value) {
  return WriteBytes(&value, sizeof(value));
}

bool QuicDataWriter::WriteUInt32(uint32 value) {
  return WriteBytes(&value, sizeof(value));
}

bool QuicDataWriter::WriteUInt48(uint64 value) {
  uint32 hi = value >> 32;
  uint32 lo = value & GG_UINT64_C(0x00000000FFFFFFFF);
  return WriteUInt32(lo) && WriteUInt16(hi);
}

bool QuicDataWriter::WriteUInt64(uint64 value) {
  return WriteBytes(&value, sizeof(value));
}

bool QuicDataWriter::WriteUInt128(uint128 value) {
  return WriteUInt64(value.lo) && WriteUInt64(value.hi);
}

bool QuicDataWriter::WriteStringPiece16(StringPiece val) {
  if (val.length() > numeric_limits<uint16>::max()) {
    return false;
  }
  if (!WriteUInt16(val.size())) {
    return false;
  }
  return WriteBytes(val.data(), val.size());
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

bool QuicDataWriter::WriteBytes(const void* data, uint32 data_len) {
  char* dest = BeginWrite(data_len);
  if (!dest) {
    return false;
  }

  memcpy(dest, data, data_len);

  length_ += data_len;
  return true;
}

void QuicDataWriter::WriteUint8ToBuffer(uint8 value, char* buffer) {
  memcpy(buffer, &value, sizeof(value));
}

void QuicDataWriter::WriteUint16ToBuffer(uint16 value, char* buffer) {
  memcpy(buffer, &value, sizeof(value));
}

void QuicDataWriter::WriteUint32ToBuffer(uint32 value, char* buffer) {
  memcpy(buffer, &value, sizeof(value));
}

void QuicDataWriter::WriteUint48ToBuffer(uint64 value, char* buffer) {
  uint16 hi = value >> 32;
  uint32 lo = value & 0x00000000FFFFFFFF;
  WriteUint32ToBuffer(lo, buffer);
  WriteUint16ToBuffer(hi, buffer + sizeof(lo));
}

void QuicDataWriter::WriteUint64ToBuffer(uint64 value, char* buffer) {
  memcpy(buffer, &value, sizeof(value));
}

void QuicDataWriter::WriteUint128ToBuffer(uint128 value, char* buffer) {
  WriteUint64ToBuffer(value.lo, buffer);
  WriteUint64ToBuffer(value.hi, buffer + sizeof(value.lo));
}

}  // namespace net
