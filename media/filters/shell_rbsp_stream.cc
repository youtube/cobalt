/*
 * Copyright 2012 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "media/filters/shell_rbsp_stream.h"

#include "base/logging.h"

namespace media {

ShellRBSPStream::ShellRBSPStream(const uint8* nalu_buffer,
                                 size_t nalu_buffer_size)
    : nalu_buffer_(nalu_buffer)
    , nalu_buffer_size_(nalu_buffer_size)
    , nalu_buffer_byte_offset_(1)  // skip first byte, it's a NALU header
    , current_nalu_byte_(0)
    , at_first_coded_zero_(false)
    , rbsp_bit_offset_(0) {
}

// read unsigned Exp-Golomb coded integer, ISO 14496-10 Section 9.1
uint32 ShellRBSPStream::ReadUEV() {
  int leading_zero_bits = -1;
  for (uint8 b = 0; b == 0; leading_zero_bits++) {
    b = ReadRBSPBit();
  }
  // we can only fit 16 bits of Exp-Golomb coded data into a 32-bit number
  DCHECK_LE(leading_zero_bits, 16);
  uint32 result = (1 << leading_zero_bits) - 1;
  result += ReadBits(leading_zero_bits);
  return result;
}

  // read signed Exp-Golomb coded integer, ISO 14496-10 Section 9.1
int32 ShellRBSPStream::ReadSEV() {
  // we start off by reading an unsigned Exp-Golomb coded number
  uint32 uev = ReadUEV();
  // the LSb in this number is treated as the inverted sign bit
  bool is_negative = !(uev & 1);
  int32 result = (int32)(uev >> 1);
  if (is_negative) result *= -1;
  return result;
}

// read and return up to 32 bits, filling from the right, meaning that
// ReadBits(17) on a stream of all 1s would return 0x01ffff
uint32 ShellRBSPStream::ReadBits(size_t bits) {
  DCHECK_LE(bits, 32);
  uint32 result = 0;
  size_t bytes = bits >> 3;
  // read bytes first
  for (int i = 0; i < bytes; i++) {
    result = result << 8;
    result = result | (uint32)ReadRBSPByte();
  }
  // scoot any leftover bits in
  bits = bits % 8;
  for (int i = 0; i < bits; i++) {
    result = result << 1;
    result = result | (uint32)ReadRBSPBit();
  }
  return result;
}

// jump over bytes in the RBSP stream
void ShellRBSPStream::SkipBytes(size_t bytes) {
  for (int i = 0; i < bytes; ++i) {
    ConsumeNALUByte();
  }
}

// jump over bits in the RBSP stream
void ShellRBSPStream::SkipBits(size_t bits) {
  // skip bytes first
  size_t bytes = bits >> 3;
  if (bytes > 0) {
    SkipBytes(bytes);
  }
  // mask off byte skips
  bits = bits & 7;
  // if no bits left to skip just return
  if (bits == 0) {
    return;
  }
  // obey the convention that if our bit offset is 0 we haven't loaded the
  // current byte, extract it from NALU stream as we are going to advance
  // the bit cursor in to it (or potentially past it)
  if (rbsp_bit_offset_ == 0) {
    ConsumeNALUByte();
  }
  // add to our bit offset
  rbsp_bit_offset_ += bits;
  // if we jumped in to the next byte advance the NALU stream, respecting the
  // convention that if we're at 8 bits stay on the current byte
  if (rbsp_bit_offset_ >= 9) {
    ConsumeNALUByte();
  }
  rbsp_bit_offset_ = rbsp_bit_offset_ % 8;
}

// advance by one byte through the NALU buffer, respecting the encoding of
// 00 00 03 => 00 00. Updates the state of current_nalu_byte_ to the new value.
void ShellRBSPStream::ConsumeNALUByte() {
  DCHECK_LT(nalu_buffer_byte_offset_, nalu_buffer_size_);
  if (at_first_coded_zero_) {
    current_nalu_byte_ = 0;
    at_first_coded_zero_ = false;
  } else if (nalu_buffer_byte_offset_ + 2 < nalu_buffer_size_   &&
             nalu_buffer_[nalu_buffer_byte_offset_]     == 0x00 &&
             nalu_buffer_[nalu_buffer_byte_offset_ + 1] == 0x00 &&
             nalu_buffer_[nalu_buffer_byte_offset_ + 2] == 0x03) {
    nalu_buffer_byte_offset_ += 3;
    current_nalu_byte_ = 0;
    at_first_coded_zero_ = true;
  } else {
    current_nalu_byte_ = nalu_buffer_[nalu_buffer_byte_offset_];
    nalu_buffer_byte_offset_++;
  }
}

// return single bit in the LSb from the RBSP stream. Bits are read from MSb
// to LSb in the stream.
uint8 ShellRBSPStream::ReadRBSPBit() {
  // check to see if we need to consume a fresh byte
  if (rbsp_bit_offset_ == 0) {
    ConsumeNALUByte();
  }
  // since we read from MSb to LSb in stream we shift right
  uint8 bit = (current_nalu_byte_ >> (7 - rbsp_bit_offset_)) & 1;
  // increment bit offset
  rbsp_bit_offset_ = (rbsp_bit_offset_ + 1) % 8;
  return bit;
}

uint8 ShellRBSPStream::ReadRBSPByte() {
  // fast path for byte-aligned access
  if (rbsp_bit_offset_ == 0) {
    ConsumeNALUByte();
    return current_nalu_byte_;
  }
  // at least some of the bits in the current byte will be included in this
  // next byte, absorb them
  uint8 upper_part = current_nalu_byte_;
  // read next byte from stream
  ConsumeNALUByte();
  // form the byte from the two bytes
  return (upper_part << rbsp_bit_offset_) |
         (current_nalu_byte_ >> (8 - rbsp_bit_offset_));
}

}  // namespace media
