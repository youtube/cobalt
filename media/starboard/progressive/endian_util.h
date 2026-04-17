// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#ifndef MEDIA_STARBOARD_PROGRESSIVE_ENDIAN_UTIL_H_
#define MEDIA_STARBOARD_PROGRESSIVE_ENDIAN_UTIL_H_

#include "base/numerics/byte_conversions.h"

// TODO: Consider Starboardize functions in this file.

namespace media {
namespace endian_util {

// The following functions must be able to support storing to/loading from
// non-aligned memory.  Thus, casts like "*reinterpret_cast<uint16_t*>(p)"
// should be avoided as these can cause crashes due to alignment on some
// platforms.

// Load 2 little-endian bytes at |p| and return as a host-endian uint16_t.
inline uint16_t load_uint16_little_endian(const uint8_t* p) {
  std::array<uint8_t, 2> bytes;
  memcpy(bytes.data(), p, 2);
  return base::U16FromLittleEndian(bytes);
}

// Load 4 little-endian bytes at |p| and return as a host-endian uint32_t.
inline uint32_t load_uint32_little_endian(const uint8_t* p) {
  std::array<uint8_t, 4> bytes;
  memcpy(bytes.data(), p, 4);
  return base::U32FromLittleEndian(bytes);
}

// Load 8 little-endian bytes at |p| and return as a host-endian uint64_t.
inline uint64_t load_uint64_little_endian(const uint8_t* p) {
  std::array<uint8_t, 8> bytes;
  memcpy(bytes.data(), p, 8);
  return base::U64FromLittleEndian(bytes);
}

// Load 2 big-endian bytes at |p| and return as a host-endian uint16_t.
inline uint16_t load_uint16_big_endian(const uint8_t* p) {
  std::array<uint8_t, 2> bytes;
  memcpy(bytes.data(), p, 2);
  return base::U16FromBigEndian(bytes);
}

// Load 4 big-endian bytes at |p| and return as a host-endian uint32_t.
inline uint32_t load_uint32_big_endian(const uint8_t* p) {
  std::array<uint8_t, 4> bytes;
  memcpy(bytes.data(), p, 4);
  return base::U32FromBigEndian(bytes);
}

// Load 8 big-endian bytes at |p| and return as a host-endian uint64_t.
inline uint64_t load_uint64_big_endian(const uint8_t* p) {
  std::array<uint8_t, 8> bytes;
  memcpy(bytes.data(), p, 8);
  return base::U64FromBigEndian(bytes);
}

// Load 2 big-endian bytes at |p| and return as an host-endian int16_t.
inline int16_t load_int16_big_endian(const uint8_t* p) {
  return static_cast<int16_t>(load_uint16_big_endian(p));
}

// Load 4 big-endian bytes at |p| and return as an host-endian int32_t.
inline int32_t load_int32_big_endian(const uint8_t* p) {
  return static_cast<int32_t>(load_uint32_big_endian(p));
}

// Load 8 big-endian bytes at |p| and return as an host-endian int64_t.
inline int64_t load_int64_big_endian(const uint8_t* p) {
  return static_cast<int64_t>(load_uint64_big_endian(p));
}

// Load 2 little-endian bytes at |p| and return as a host-endian int16_t.
inline int16_t load_int16_little_endian(const uint8_t* p) {
  return static_cast<int16_t>(load_uint16_little_endian(p));
}

// Load 4 little-endian bytes at |p| and return as a host-endian int32_t.
inline int32_t load_int32_little_endian(const uint8_t* p) {
  return static_cast<int32_t>(load_uint32_little_endian(p));
}

// Load 8 little-endian bytes at |p| and return as a host-endian int64_t.
inline int64_t load_int64_little_endian(const uint8_t* p) {
  return static_cast<int64_t>(load_uint64_little_endian(p));
}

// Store 2 host-endian bytes as big-endian at |p|.
inline void store_uint16_big_endian(uint16_t d, uint8_t* p) {
  auto bytes = base::U16ToBigEndian(d);
  memcpy(p, bytes.data(), bytes.size());
}

// Store 4 host-endian bytes as big-endian at |p|.
inline void store_uint32_big_endian(uint32_t d, uint8_t* p) {
  auto bytes = base::U32ToBigEndian(d);
  memcpy(p, bytes.data(), bytes.size());
}

// Store 8 host-endian bytes as big-endian at |p|.
inline void store_uint64_big_endian(uint64_t d, uint8_t* p) {
  auto bytes = base::U64ToBigEndian(d);
  memcpy(p, bytes.data(), bytes.size());
}

// Store 2 host-endian bytes as little-endian at |p|.
inline void store_uint16_little_endian(uint16_t d, uint8_t* p) {
  auto bytes = base::U16ToLittleEndian(d);
  memcpy(p, bytes.data(), bytes.size());
}

// Store 4 host-endian bytes as little-endian at |p|.
inline void store_uint32_little_endian(uint32_t d, uint8_t* p) {
  auto bytes = base::U32ToLittleEndian(d);
  memcpy(p, bytes.data(), bytes.size());
}

// Store 8 host-endian bytes as little-endian at |p|.
inline void store_uint64_little_endian(uint64_t d, uint8_t* p) {
  auto bytes = base::U64ToLittleEndian(d);
  memcpy(p, bytes.data(), bytes.size());
}

}  // namespace endian_util
}  // namespace media

#endif  // MEDIA_STARBOARD_PROGRESSIVE_ENDIAN_UTIL_H_
