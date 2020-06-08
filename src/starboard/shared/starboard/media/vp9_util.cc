// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/media/vp9_util.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {

namespace {

// Reading size as u_bytes(n):
//   u_bytes(n) {
//     value = 0
//     for (i = 0; i < n; ++i)
//       value |= byte << (i * 8) u(8)
//     return value
//   }
size_t ReadSize(const uint8_t* data, size_t bytes_of_size) {
  size_t size = 0;
  for (size_t i = 0; i < bytes_of_size; ++i) {
    size |= static_cast<size_t>(data[i]) << (i * 8);
  }
  return size;
}

}  // namespace

Vp9FrameParser::Vp9FrameParser(const void* vp9_frame, size_t size) {
  const uint8_t* frame_in_uint8 = static_cast<const uint8_t*>(vp9_frame);

  // To determine whether a chunk of data has a superframe index, check the last
  // byte in the chunk for the marker 3 bits (0b110xxxxx).
  uint8_t last_byte = size == 0 ? 0 : frame_in_uint8[size - 1];
  if ((last_byte >> 5) == 0b110 && ParseSuperFrame(frame_in_uint8, size)) {
    return;
  }
  // Not a superframe, or ParseSuperFrame() fails, treat the whole input as a
  // single subframe.
  subframes_[0].address = frame_in_uint8;
  subframes_[0].size = size;
  number_of_subframes_ = 1;
}

bool Vp9FrameParser::ParseSuperFrame(const uint8_t* frame,
                                     size_t size_of_superframe) {
  uint8_t last_byte = frame[size_of_superframe - 1];
  // mask 2 bits (0bxxx11xxx) and add 1 to get the size in bytes for each frame
  // size.
  size_t bytes_of_size = ((last_byte & 0b00011000) >> 3) + 1;
  // mask 3 bits (0bxxxxx111) and add 1 to get the # of frame sizes encoded in
  // the index.
  number_of_subframes_ = (last_byte & 0b00000111) + 1;
  // Calculate the total size of the index as follows:
  // 2 + number of frame sizes encoded * number of bytes to hold each frame
  // size.
  size_t total_bytes_of_superframe_index =
      2 + number_of_subframes_ * bytes_of_size;

  SB_DCHECK(number_of_subframes_ <= kMaxNumberOfSubFrames);

  if (total_bytes_of_superframe_index > size_of_superframe) {
    SB_LOG(WARNING) << "Size of vp9 superframe index is less than the frame"
                    << " size.";
    return false;
  }

  // Go to the first byte of the superframe index, and check that it matches the
  // last byte of the superframe index.
  const uint8_t* first_byte =
      frame + (size_of_superframe - total_bytes_of_superframe_index);
  if (*first_byte != last_byte) {
    SB_LOG(WARNING) << "Vp9 superframe marker bytes doesn't match.";
    return false;
  }

  size_t accumulated_size_of_subframes = 0;
  // Skip the first byte, which is a marker.
  const uint8_t* address_of_sizes = first_byte + 1;
  for (size_t i = 0; i < number_of_subframes_; ++i) {
    subframes_[i].size = ReadSize(address_of_sizes, bytes_of_size);
    subframes_[i].address = frame + accumulated_size_of_subframes;
    address_of_sizes += bytes_of_size;
    accumulated_size_of_subframes += subframes_[i].size;
  }

  if (accumulated_size_of_subframes + total_bytes_of_superframe_index >
      size_of_superframe) {
    SB_LOG(WARNING) << "Accumulated size of vp9 subframes is too big.";
    return false;
  }

  if (accumulated_size_of_subframes + total_bytes_of_superframe_index !=
      size_of_superframe) {
    // There are some empty space before the superframe index, that's probably
    // still valid but let's issue a warning.
    SB_LOG(WARNING) << "Accumulated size of vp9 subframes doesn't match.";
  }

  // Now we can finally confirm that it is a valid superframe.
  return true;
}

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
