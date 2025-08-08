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

#ifndef COBALT_MEDIA_PROGRESSIVE_RBSP_STREAM_H_
#define COBALT_MEDIA_PROGRESSIVE_RBSP_STREAM_H_

#include "base/basictypes.h"

namespace cobalt {
namespace media {

// ISO 14496-10 describes a byte encoding format for NALUs (network abstraction
// layer units) and rules to convert it into a RBSP stream, which is the format
// that some other atoms are defined. This class takes a non-owning reference
// to a buffer and extract various types from the stream while silently
// consuming the extra encoding bytes and advancing a bit stream pointer.
class RBSPStream {
 public:
  // NON-OWNING pointer to buffer. It is assumed the client will dispose of
  // this buffer.
  RBSPStream(const uint8* nalu_buffer, size_t nalu_buffer_size);
  // all Read/Skip methods return the value by reference and return true
  // on success, false on read error/EOB. Once the object has returned
  // false the consistency of the data is not guaranteed.
  // read unsigned Exp-Golomb coded integer, ISO 14496-10 Section 9.1
  bool ReadUEV(uint32* uev_out);
  // read signed Exp-Golomb coded integer, ISO 14496-10 Section 9.1
  bool ReadSEV(int32* sev_out);
  // read and return up to 32 bits, filling from the right, meaning that
  // ReadBits(17) on a stream of all 1s would return 0x01ffff
  bool ReadBits(size_t bits, uint32* bits_out);
  bool ReadByte(uint8* byte_out) { return ReadRBSPByte(byte_out); }
  bool ReadBit(uint8* bit_out) { return ReadRBSPBit(bit_out); }
  // jump over bytes in the RBSP stream
  bool SkipBytes(size_t bytes);
  // jump over bits in the RBSP stream
  bool SkipBits(size_t bits);

 private:
  // advance by one byte through the NALU buffer, respecting the encoding of
  // 00 00 03 => 00 00. Updates the state of current_nalu_byte_ to the new
  // value.
  // returns false if we have moved past the end of the buffer.
  bool ConsumeNALUByte();
  // return single bit in the LSb from the RBSP stream. Bits are read from MSb
  // to LSb in the stream.
  bool ReadRBSPBit(uint8* bit_out);
  bool ReadRBSPByte(uint8* byte_out);

  const uint8* nalu_buffer_;
  size_t nalu_buffer_size_;
  size_t nalu_buffer_byte_offset_;
  uint8 current_nalu_byte_;
  int number_consecutive_zeros_;
  // location of rbsp bit cursor within current_nalu_byte_
  size_t rbsp_bit_offset_;
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_PROGRESSIVE_RBSP_STREAM_H_
