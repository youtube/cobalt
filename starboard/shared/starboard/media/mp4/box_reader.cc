// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "starboard/shared/starboard/media/mp4/box_reader.h"

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <type_traits>

// Copied from media/formats/mp4/box_reader.cc, reduced to what
// ValidatePsshInput() needs and ported to C++17 without //base and //media.

namespace starboard::mp4 {

namespace {

// Copied from copied_base/base/big_endian.h, since base::U32FromBigEndian() and
// base::U64FromBigEndian() from base/numerics/byte_conversions.h require C++20.

// Read an integer (signed or unsigned) from |buf| in Big Endian order.
// Note: this loop is unrolled with -O1 and above.
// NOTE(szym): glibc dns-canon.c use ntohs(*(uint16_t*)ptr) which is
// potentially unaligned.
// This would cause SIGBUS on ARMv5 or earlier and ARMv6-M.
template <typename T>
inline void ReadBigEndian(const uint8_t buf[], T* out) {
  static_assert(std::is_integral<T>::value, "T has to be an integral type.");
  // Make an unsigned version of the output type to make shift possible
  // without UB.
  typename std::make_unsigned<T>::type unsigned_result = buf[0];
  for (size_t i = 1; i < sizeof(T); ++i) {
    unsigned_result <<= 8;
    // Must cast to uint8_t to avoid clobbering by sign extension.
    unsigned_result |= buf[i];
  }
  *out = unsigned_result;
}

}  // namespace

Box::~Box() = default;

BufferReader::BufferReader(const uint8_t* buf, const size_t buf_size)
    : buf_(buf, buf_size) {}

BufferReader::~BufferReader() = default;

BufferReader::BufferReader(const BufferReader& other) = default;

bool BufferReader::Read4(uint32_t* v) {
  RCHECK(HasBytes(4));
  ReadBigEndian(&buf_[pos_], v);
  pos_ += 4u;
  return true;
}
bool BufferReader::Read8(uint64_t* v) {
  RCHECK(HasBytes(8));
  ReadBigEndian(&buf_[pos_], v);
  pos_ += 8u;
  return true;
}

bool BufferReader::ReadFourCC(FourCC* v) {
  uint32_t tmp;
  RCHECK(Read4(&tmp));
  *v = static_cast<FourCC>(tmp);
  return true;
}

bool BufferReader::ReadVec(std::vector<uint8_t>* vec, size_t count) {
  RCHECK(HasBytes(count));
  vec->clear();
  auto range = buf_.subspan(pos_, count);
  vec->insert(vec->end(), range.begin(), range.end());
  pos_ += count;
  return true;
}

bool BufferReader::Read4Into8(uint64_t* v) {
  uint32_t tmp;
  RCHECK(Read4(&tmp));
  *v = tmp;
  return true;
}

BoxReader::BoxReader(const uint8_t* buf, const size_t buf_size, bool is_EOS)
    : BufferReader(buf, buf_size),
      box_size_(0),
      box_size_known_(false),
      type_(FOURCC_NULL),
      version_(0),
      flags_(0),
      scanned_(false),
      is_EOS_(is_EOS) {}

BoxReader::BoxReader(const BoxReader& other) = default;

BoxReader::~BoxReader() = default;

// static
BoxReader* BoxReader::ReadConcatentatedBoxes(const uint8_t* buf,
                                             const size_t buf_size) {
  BoxReader* reader = new BoxReader(buf, buf_size, true);

  // Concatenated boxes are passed in without a wrapping parent box. Set
  // |box_size_| to the concatenated buffer length to mimic having already
  // parsed the parent box.
  reader->box_size_ = buf_size;
  reader->box_size_known_ = true;

  return reader;
}

bool BoxReader::ReadFullBoxHeader() {
  uint32_t vflags;
  RCHECK(Read4(&vflags));
  version_ = vflags >> 24;
  flags_ = vflags & 0xffffff;
  return true;
}

ParseResult BoxReader::ReadHeader() {
  uint64_t box_size = 0;

  if (!HasBytes(8)) {
    return is_EOS_ ? ParseResult::kError : ParseResult::kNeedMoreData;
  }
  SB_CHECK(Read4Into8(&box_size));
  SB_CHECK(ReadFourCC(&type_));

  if (box_size == 0) {
    if (is_EOS_) {
      // All the data bytes are expected to be provided.
      box_size = static_cast<uint64_t>(buf_.size());
    } else {
      SB_DLOG(INFO) << "ISO BMFF boxes that run to EOS are not supported";
      return ParseResult::kError;
    }
  } else if (box_size == 1) {
    if (!HasBytes(8)) {
      return is_EOS_ ? ParseResult::kError : ParseResult::kNeedMoreData;
    }
    SB_CHECK(Read8(&box_size));
  }

  // Implementation-specific: support for boxes larger than 2^31 has been
  // removed.
  if (box_size < static_cast<uint64_t>(pos_) ||
      box_size > static_cast<uint64_t>(std::numeric_limits<int32_t>::max())) {
    return ParseResult::kError;
  }

  // Make sure the buffer contains at least the expected number of bytes.
  // Since the data may be appended in pieces, this is only an error if EOS.
  if (box_size > static_cast<uint64_t>(buf_.size())) {
    return is_EOS_ ? ParseResult::kError : ParseResult::kNeedMoreData;
  }

  // Note that the pos_ head has advanced to the byte immediately after the
  // header, which is where we want it.
  box_size_ = static_cast<size_t>(box_size);
  box_size_known_ = true;

  // We don't want future reads to go beyond the box.
  buf_ = buf_.first(box_size_);

  return ParseResult::kOk;
}

}  // namespace starboard::mp4
