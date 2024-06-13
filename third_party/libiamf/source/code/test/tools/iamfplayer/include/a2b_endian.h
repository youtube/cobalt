/*
BSD 3-Clause Clear License The Clear BSD License

Copyright (c) 2023, Alliance for Open Media.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/**
 * @file a2b_endian.h
 * @brief Endian convert.
 * @version 0.1
 * @date Created 03/03/2023
 **/


#ifndef _A2B_ENDIAN_H
#define _A2B_ENDIAN_H

#include <stdint.h>

static inline uint32_t bswap32(const uint32_t u32) {
#ifndef WORDS_BIGENDIAN
#if defined(__GNUC__) && \
    ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 3)))
  return __builtin_bswap32(u32);
#else
  return (u32 << 24) | ((u32 << 8) & 0xFF0000) | ((u32 >> 8) & 0xFF00) |
         (u32 >> 24);
#endif
#else
  return u32;
#endif
}

static inline uint16_t bswap16(const uint16_t u16) {
#ifndef WORDS_BIGENDIAN
#if defined(__GNUC__) && \
    ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 8)))
  return __builtin_bswap16(u16);
#else
  return (uint16_t)((u16 << 8) | (u16 >> 8));
#endif
#else
  return u16;
#endif
}

static inline uint16_t bswap64(const uint64_t u64) {
#ifndef WORDS_BIGENDIAN
#if defined(__GNUC__) && \
    ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 8)))
  return __builtin_bswap64(u64);
#else
  return (bswap32(u64) + 0ULL) << 32 | bswap32(u64 >> 32);
#endif
#else
  return u64;
#endif
}

#define be64(x) bswap64(x)
#define be32(x) bswap32(x)
#define be16(x) bswap16(x)

#endif
