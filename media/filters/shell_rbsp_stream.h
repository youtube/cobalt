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

#ifndef MEDIA_FILTERS_SHELL_RBSP_STREAM_H_
#define MEDIA_FILTERS_SHELL_RBSP_STREAM_H_

#include "base/basictypes.h"

namespace media {

// ISO 14496-10 describes a byte encoding format for NALUs (network abstraction
// layer units) and rules to convert it into a RBSP stream, which is the format
// that some other atoms are defined. This class takes a non-owning reference
// to a buffer and extract various types from the stream while silently
// consuming the extra encoding bytes and advancing a bit stream pointer.
class ShellRBSPStream {
 public:
  // NON-OWNING pointer to buffer. It is assumed the client will dispose of
  // this buffer.
  ShellRBSPStream(const uint8* nalu_buffer, size_t nalu_buffer_size);
  // read unsigned Exp-Golomb coded integer, ISO 14496-10 Section 9.1
  uint32 ReadUEV();
  // read signed Exp-Golomb coded integer, ISO 14496-10 Section 9.1
  int32 ReadSEV();
  // read and return up to 32 bits, filling from the right, meaning that
  // ReadBits(17) on a stream of all 1s would return 0x01ff
  uint32 ReadBits(size_t bits);
  uint8 ReadByte() { return ReadRBSPByte(); }
  uint8 ReadBit() { return ReadRBSPBit(); }
  // jump over bytes in the RBSP stream
  void SkipBytes(size_t bytes);
  // jump over bits in the RBSP stream
  void SkipBits(size_t bits);

 private:
  // advance by one byte through the NALU buffer, respecting the encoding of
  // 00 00 03 => 00 00. Updates the state of current_nalu_byte_ to the new value.
  void ConsumeNALUByte();
  // return single bit in the LSb from the RBSP stream. Bits are read from MSb
  // to LSb in the stream.
  uint8 ReadRBSPBit();
  uint8 ReadRBSPByte();

  const uint8* nalu_buffer_;
  size_t nalu_buffer_size_;
  size_t nalu_buffer_byte_offset_;
  uint8 current_nalu_byte_;
  bool at_first_coded_zero_;
  // location of rbsp bit cursor within current_nalu_byte_
  size_t rbsp_bit_offset_;
};

};

#endif  // MEDIA_FILTERS_SHELL_RBSP_STREAM_H_
