/* Copyright 2013 Google Inc. All Rights Reserved.

   Distributed under MIT license.
   See file LICENSE for detail or copy at https://opensource.org/licenses/MIT
*/

/* The parts of ots.h & opentype-sanitiser.h that we need, taken from the
   https://code.google.com/p/ots/ project. */

#ifndef WOFF2_BUFFER_H_
#define WOFF2_BUFFER_H_

#if defined(STARBOARD)
#include "starboard/byte_swap.h"
#define woff2_ntohl(x) SB_NET_TO_HOST_U32(x)
#define woff2_ntohs(x) SB_NET_TO_HOST_U16(x)
#define woff2_htonl(x) SB_HOST_TO_NET_U32(x)
#define woff2_htons(x) SB_HOST_TO_NET_U16(x)
#elif defined(_WIN32)
#include <stdlib.h>
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef int int32_t;
typedef unsigned int uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#define woff2_ntohl(x) _byteswap_ulong (x)
#define woff2_ntohs(x) _byteswap_ushort (x)
#define woff2_htonl(x) _byteswap_ulong (x)
#define woff2_htons(x) _byteswap_ushort (x)
#else
#include <arpa/inet.h>
#include <stdint.h>
#define woff2_ntohl(x) ntohl (x)
#define woff2_ntohs(x) ntohs (x)
#define woff2_htonl(x) htonl (x)
#define woff2_htons(x) htons (x)
#endif

#if !defined(STARBOARD)
#include <cstring>
#define MEMCPY_BUFFER std::memcpy
#else
#include "starboard/memory.h"
#define MEMCPY_BUFFER SbMemoryCopy
#endif

#include <cstdio>
#include <cstdlib>
#include <limits>

namespace woff2 {

#if defined(_MSC_VER) || !defined(FONT_COMPRESSION_DEBUG)
#define FONT_COMPRESSION_FAILURE() false
#else
#define FONT_COMPRESSION_FAILURE() \
  woff2::Failure(__FILE__, __LINE__, __PRETTY_FUNCTION__)
inline bool Failure(const char *f, int l, const char *fn) {
  fprintf(stderr, "ERROR at %s:%d (%s)\n", f, l, fn);
  fflush(stderr);
  return false;
}
#endif

// -----------------------------------------------------------------------------
// Buffer helper class
//
// This class perform some trival buffer operations while checking for
// out-of-bounds errors. As a family they return false if anything is amiss,
// updating the current offset otherwise.
// -----------------------------------------------------------------------------
class Buffer {
 public:
  Buffer(const uint8_t *data, size_t len)
      : buffer_(data),
        length_(len),
        offset_(0) { }

  bool Skip(size_t n_bytes) {
    return Read(NULL, n_bytes);
  }

  bool Read(uint8_t *data, size_t n_bytes) {
    if (n_bytes > 1024 * 1024 * 1024) {
      return FONT_COMPRESSION_FAILURE();
    }
    if ((offset_ + n_bytes > length_) ||
        (offset_ > length_ - n_bytes)) {
      return FONT_COMPRESSION_FAILURE();
    }
    if (data) {
      MEMCPY_BUFFER(data, buffer_ + offset_, n_bytes);
    }
    offset_ += n_bytes;
    return true;
  }

  inline bool ReadU8(uint8_t *value) {
    if (offset_ + 1 > length_) {
      return FONT_COMPRESSION_FAILURE();
    }
    *value = buffer_[offset_];
    ++offset_;
    return true;
  }

  bool ReadU16(uint16_t *value) {
    if (offset_ + 2 > length_) {
      return FONT_COMPRESSION_FAILURE();
    }
    MEMCPY_BUFFER(value, buffer_ + offset_, sizeof(uint16_t));
    *value = woff2_ntohs(*value);
    offset_ += 2;
    return true;
  }

  bool ReadS16(int16_t *value) {
    return ReadU16(reinterpret_cast<uint16_t*>(value));
  }

  bool ReadU24(uint32_t *value) {
    if (offset_ + 3 > length_) {
      return FONT_COMPRESSION_FAILURE();
    }
    *value = static_cast<uint32_t>(buffer_[offset_]) << 16 |
        static_cast<uint32_t>(buffer_[offset_ + 1]) << 8 |
        static_cast<uint32_t>(buffer_[offset_ + 2]);
    offset_ += 3;
    return true;
  }

  bool ReadU32(uint32_t *value) {
    if (offset_ + 4 > length_) {
      return FONT_COMPRESSION_FAILURE();
    }
    MEMCPY_BUFFER(value, buffer_ + offset_, sizeof(uint32_t));
    *value = woff2_ntohl(*value);
    offset_ += 4;
    return true;
  }

  bool ReadS32(int32_t *value) {
    return ReadU32(reinterpret_cast<uint32_t*>(value));
  }

  bool ReadTag(uint32_t *value) {
    if (offset_ + 4 > length_) {
      return FONT_COMPRESSION_FAILURE();
    }
    MEMCPY_BUFFER(value, buffer_ + offset_, sizeof(uint32_t));
    offset_ += 4;
    return true;
  }

  bool ReadR64(uint64_t *value) {
    if (offset_ + 8 > length_) {
      return FONT_COMPRESSION_FAILURE();
    }
    MEMCPY_BUFFER(value, buffer_ + offset_, sizeof(uint64_t));
    offset_ += 8;
    return true;
  }

  const uint8_t *buffer() const { return buffer_; }
  size_t offset() const { return offset_; }
  size_t length() const { return length_; }

  void set_offset(size_t newoffset) { offset_ = newoffset; }

 private:
  const uint8_t * const buffer_;
  const size_t length_;
  size_t offset_;
};

} // namespace woff2

#undef MEMCPY_BUFFER
#undef woff2_ntohl
#undef woff2_ntohs
#undef woff2_htonl
#undef woff2_htons

#endif  // WOFF2_BUFFER_H_
