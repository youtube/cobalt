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

#ifndef COBALT_MEDIA_BASE_ENDIAN_UTIL_H_
#define COBALT_MEDIA_BASE_ENDIAN_UTIL_H_

#include "base/sys_byteorder.h"
#include "starboard/memory.h"

namespace cobalt {
namespace media {
namespace endian_util {

// The following functions must be able to support storing to/loading from
// non-aligned memory.  Thus, casts like "*reinterpret_cast<uint16_t*>(p)"
// should be avoided as these can cause crashes due to alignment on some
// platforms.

// Load 2 little-endian bytes at |p| and return as a host-endian uint16_t.
inline uint16_t load_uint16_little_endian(const uint8_t* p) {
  uint16_t aligned_p;
  memcpy(&aligned_p, p, sizeof(aligned_p));
  return base::ByteSwapToLE16(aligned_p);
}

// Load 4 little-endian bytes at |p| and return as a host-endian uint32_t.
inline uint32_t load_uint32_little_endian(const uint8_t* p) {
  uint32_t aligned_p;
  memcpy(&aligned_p, p, sizeof(aligned_p));
  return base::ByteSwapToLE32(aligned_p);
}

// Load 8 little-endian bytes at |p| and return as a host-endian uint64_t.
inline uint64_t load_uint64_little_endian(const uint8_t* p) {
  uint64_t aligned_p;
  memcpy(&aligned_p, p, sizeof(aligned_p));
  return base::ByteSwapToLE64(aligned_p);
}

// Load 2 big-endian bytes at |p| and return as a host-endian uint16_t.
inline uint16_t load_uint16_big_endian(const uint8_t* p) {
  uint16_t aligned_p;
  memcpy(&aligned_p, p, sizeof(aligned_p));
  return base::NetToHost16(aligned_p);
}

// Load 4 big-endian bytes at |p| and return as a host-endian uint32_t.
inline uint32_t load_uint32_big_endian(const uint8_t* p) {
  uint32_t aligned_p;
  memcpy(&aligned_p, p, sizeof(aligned_p));
  return base::NetToHost32(aligned_p);
}

// Load 8 big-endian bytes at |p| and return as a host-endian uint64_t.
inline uint64_t load_uint64_big_endian(const uint8_t* p) {
  uint64_t aligned_p;
  memcpy(&aligned_p, p, sizeof(aligned_p));
  return base::NetToHost64(aligned_p);
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
  uint16_t big_d = base::HostToNet16(d);
  memcpy(p, &big_d, sizeof(big_d));
}

// Store 4 host-endian bytes as big-endian at |p|.
inline void store_uint32_big_endian(uint32_t d, uint8_t* p) {
  uint32_t big_d = base::HostToNet32(d);
  memcpy(p, &big_d, sizeof(big_d));
}

// Store 8 host-endian bytes as big-endian at |p|.
inline void store_uint64_big_endian(uint64_t d, uint8_t* p) {
  uint64_t big_d = base::HostToNet64(d);
  memcpy(p, &big_d, sizeof(big_d));
}

// Store 2 host-endian bytes as little-endian at |p|.
inline void store_uint16_little_endian(uint16_t d, uint8_t* p) {
  uint16_t little_d = base::ByteSwapToLE16(d);
  memcpy(p, &little_d, sizeof(little_d));
}

// Store 4 host-endian bytes as little-endian at |p|.
inline void store_uint32_little_endian(uint32_t d, uint8_t* p) {
  uint32_t little_d = base::ByteSwapToLE32(d);
  memcpy(p, &little_d, sizeof(little_d));
}

// Store 8 host-endian bytes as little-endian at |p|.
inline void store_uint64_little_endian(uint64_t d, uint8_t* p) {
  uint64_t little_d = base::ByteSwapToLE64(d);
  memcpy(p, &little_d, sizeof(little_d));
}

}  // namespace endian_util
}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_ENDIAN_UTIL_H_
