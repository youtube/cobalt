// Copyright 2012 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/media/progressive/rbsp_stream.h"

#include "base/logging.h"

namespace cobalt {
namespace media {

RBSPStream::RBSPStream(const uint8* nalu_buffer, size_t nalu_buffer_size)
    : nalu_buffer_(nalu_buffer),
      nalu_buffer_size_(nalu_buffer_size),
      nalu_buffer_byte_offset_(0),
      current_nalu_byte_(0),
      number_consecutive_zeros_(0),
      rbsp_bit_offset_(0) {}

// read unsigned Exp-Golomb coded integer, ISO 14496-10 Section 9.1
bool RBSPStream::ReadUEV(uint32* uev_out) {
  DCHECK(uev_out);
  int leading_zero_bits = -1;
  for (uint8 b = 0; b == 0; leading_zero_bits++) {
    if (!ReadRBSPBit(&b)) {
      return false;
    }
  }
  // we can only fit 31 bits of Exp-Golomb coded data into a 32-bit number
  if (leading_zero_bits >= 32) {
    return false;
  }
  uint32 result = (1 << leading_zero_bits) - 1;
  uint32 remainder = 0;
  if (!ReadBits(leading_zero_bits, &remainder)) {
    return false;
  }
  result += remainder;
  *uev_out = result;
  return true;
}

// read signed Exp-Golomb coded integer, ISO 14496-10 Section 9.1
bool RBSPStream::ReadSEV(int32* sev_out) {
  DCHECK(sev_out);
  // we start off by reading an unsigned Exp-Golomb coded number
  uint32 uev = 0;
  if (!ReadUEV(&uev)) {
    return false;
  }
  // the LSb in this number is treated as the inverted sign bit
  bool is_negative = !(uev & 1);
  int32 result = static_cast<int32>((uev + 1) >> 1);
  if (is_negative) {
    result *= -1;
  }
  *sev_out = result;
  return true;
}

// read and return up to 32 bits, filling from the right, meaning that
// ReadBits(17) on a stream of all 1s would return 0x01ffff
bool RBSPStream::ReadBits(size_t bits, uint32* bits_out) {
  DCHECK(bits_out);
  if (bits > 32) {
    return false;
  }
  if (bits == 0) {
    return true;
  }
  uint32 result = 0;
  size_t bytes = bits >> 3;
  // read bytes first
  for (int i = 0; i < bytes; i++) {
    uint8 new_byte = 0;
    if (!ReadRBSPByte(&new_byte)) {
      return false;
    }
    result = result << 8;
    result = result | static_cast<uint32>(new_byte);
  }
  // scoot any leftover bits in
  bits = bits % 8;
  for (int i = 0; i < bits; i++) {
    uint8 new_bit = 0;
    if (!ReadRBSPBit(&new_bit)) {
      return false;
    }
    result = result << 1;
    result = result | static_cast<uint32>(new_bit);
  }
  *bits_out = result;
  return true;
}

// jump over bytes in the RBSP stream
bool RBSPStream::SkipBytes(size_t bytes) {
  for (int i = 0; i < bytes; ++i) {
    if (!ConsumeNALUByte()) {
      return false;
    }
  }
  return true;
}

// jump over bits in the RBSP stream
bool RBSPStream::SkipBits(size_t bits) {
  // skip bytes first
  size_t bytes = bits >> 3;
  if (bytes > 0) {
    if (!SkipBytes(bytes)) {
      return false;
    }
  }
  // mask off byte skips
  bits = bits & 7;
  // if no bits left to skip just return
  if (bits == 0) {
    return true;
  }
  // obey the convention that if our bit offset is 0 we haven't loaded the
  // current byte, extract it from NALU stream as we are going to advance
  // the bit cursor in to it (or potentially past it)
  if (rbsp_bit_offset_ == 0) {
    if (!ConsumeNALUByte()) {
      return false;
    }
  }
  // add to our bit offset
  rbsp_bit_offset_ += bits;
  // if we jumped in to the next byte advance the NALU stream, respecting the
  // convention that if we're at 8 bits stay on the current byte
  if (rbsp_bit_offset_ >= 9) {
    if (!ConsumeNALUByte()) {
      return false;
    }
  }
  rbsp_bit_offset_ = rbsp_bit_offset_ % 8;
  return true;
}

// advance by one byte through the NALU buffer, respecting the encoding of
// 00 00 03 => 00 00. Updates the state of current_nalu_byte_ to the new value.
bool RBSPStream::ConsumeNALUByte() {
  if (nalu_buffer_byte_offset_ >= nalu_buffer_size_) {
    return false;
  }
  current_nalu_byte_ = nalu_buffer_[nalu_buffer_byte_offset_];
  if (current_nalu_byte_ == 0x03 && number_consecutive_zeros_ >= 2) {
    ++nalu_buffer_byte_offset_;
    current_nalu_byte_ = nalu_buffer_[nalu_buffer_byte_offset_];
    number_consecutive_zeros_ = 0;
  }

  if (current_nalu_byte_ == 0) {
    ++number_consecutive_zeros_;
  } else {
    number_consecutive_zeros_ = 0;
  }
  ++nalu_buffer_byte_offset_;
  return true;
}

// return single bit in the LSb from the RBSP stream. Bits are read from MSb
// to LSb in the stream.
bool RBSPStream::ReadRBSPBit(uint8* bit_out) {
  DCHECK(bit_out);
  // check to see if we need to consume a fresh byte
  if (rbsp_bit_offset_ == 0) {
    if (!ConsumeNALUByte()) {
      return false;
    }
  }
  // since we read from MSb to LSb in stream we shift right
  uint8 bit = (current_nalu_byte_ >> (7 - rbsp_bit_offset_)) & 1;
  // increment bit offset
  rbsp_bit_offset_ = (rbsp_bit_offset_ + 1) % 8;
  *bit_out = bit;
  return true;
}

bool RBSPStream::ReadRBSPByte(uint8* byte_out) {
  DCHECK(byte_out);
  // fast path for byte-aligned access
  if (rbsp_bit_offset_ == 0) {
    if (!ConsumeNALUByte()) {
      return false;
    }
    *byte_out = current_nalu_byte_;
    return true;
  }
  // at least some of the bits in the current byte will be included in this
  // next byte, absorb them
  uint8 upper_part = current_nalu_byte_;
  // read next byte from stream
  if (!ConsumeNALUByte()) {
    return false;
  }
  // form the byte from the two bytes
  *byte_out = (upper_part << rbsp_bit_offset_) |
              (current_nalu_byte_ >> (8 - rbsp_bit_offset_));
  return true;
}

}  // namespace media
}  // namespace cobalt
