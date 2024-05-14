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
 * @file bitstream.c
 * @brief bitstream reader.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#include "bitstream.h"

#include <assert.h>
#include <string.h>

int32_t bs(BitStream *b, const uint8_t *data, int size) {
  b->data = data;
  b->size = size;
  b->b8sp = b->b8p = 0;

  return 0;
}

static uint32_t bs_getLastA32b(BitStream *b) {
  uint32_t ret = 0;
  int n = 4;

  if (b->b8sp >= b->size) return 0;
  if (b->b8sp + 4 > b->size) n = b->size - b->b8sp;

  for (int i = 0; i < n; ++i) {
    ret <<= INT8_BITS;
    ret |= b->data[b->b8sp + i];
  }

  n = 4 - n;
  if (n > 0) ret <<= (INT8_BITS * n);

  return ret;
}

uint32_t bs_get32b(BitStream *b, int n) {
  uint32_t ret = 0;
  uint32_t nb8p = 0, nn;

  assert(n <= INT32_BITS);

  ret = bs_getLastA32b(b);
  if (n + b->b8p > INT32_BITS) {
    nb8p = n + b->b8p - INT32_BITS;
    nn = INT32_BITS - b->b8p;
  } else {
    nn = n;
  }

  ret >>= INT32_BITS - nn - b->b8p;
  if (nn < INT32_BITS) {
    ret &= ~((~0U) << nn);
  }
  b->b8p += nn;
  b->b8sp += (b->b8p / INT8_BITS);
  b->b8p %= INT8_BITS;

  if (nb8p) {
    uint32_t nret = bs_get32b(b, nb8p);
    ret <<= nb8p;
    ret |= nret;
  }

  return ret;
}

int32_t bs_skip(BitStream *b, int n) {
  b->b8p += n;
  b->b8sp += (b->b8p / INT8_BITS);
  b->b8p %= INT8_BITS;

  return 0;
}

void bs_align(BitStream *b) {
  if (b->b8p) {
    ++b->b8sp;
    b->b8p = 0;
  }
}

int32_t bs_skipABytes(BitStream *b, int n) { return bs_read(b, 0, n); }

uint32_t bs_getA8b(BitStream *b) {
  uint32_t ret;

  bs_align(b);
  ret = b->data[b->b8sp];
  ++b->b8sp;
  return ret;
}

uint32_t bs_getA16b(BitStream *b) {
  uint32_t ret = bs_getA8b(b);
  ret <<= INT8_BITS;
  ret |= bs_getA8b(b);
  return ret;
}

uint32_t bs_getA32b(BitStream *b) {
  uint32_t ret = bs_getA16b(b);
  ret <<= INT16_BITS;
  ret |= bs_getA16b(b);
  return ret;
}

uint64_t bs_getAleb128(BitStream *b) {
  uint64_t ret = 0;
  uint32_t i;
  uint8_t byte;

  bs_align(b);

  if (b->b8sp >= b->size) return 0;

  for (i = 0; i < 8; i++) {
    if (b->b8sp + i >= b->size) break;
    byte = b->data[b->b8sp + i];
    ret |= (((uint64_t)byte & 0x7f) << (i * 7));
    if (!(byte & 0x80)) {
      break;
    }
  }
  ++i;
  b->b8sp += i;
  return ret;
}

int32_t bs_read(BitStream *b, uint8_t *data, int n) {
  bs_align(b);
  if (data) memcpy(data, &b->data[b->b8sp], n);
  b->b8sp += n;
  return n;
}

int32_t bs_readString(BitStream *b, char *data, int n) {
  int len = 0, rlen = 0;
  bs_align(b);
  len = strlen((char *)&b->data[b->b8sp]) + 1;
  rlen = len;
  if (rlen > n) rlen = n;
  memcpy(data, &b->data[b->b8sp], rlen - 1);
  data[rlen - 1] = '\0';
  b->b8sp += len;
  return len;
}

uint32_t bs_tell(BitStream *b) { return b->b8p ? b->b8sp + 1 : b->b8sp; }

uint8_t readu8(uint8_t *data, int offset) { return data[offset]; }

uint32_t readu16be(uint8_t *data, int offset) {
  return data[offset] << 8 | data[offset + 1];
}

int reads16be(uint8_t *data, int offset) {
  short ret = readu16be(data, offset);
  return ret;
}

uint32_t readu16le(uint8_t *data, int offset) {
  return data[offset] | data[offset + 1] << 8;
}

int reads16le(uint8_t *data, int offset) {
  short ret = (short)readu16le(data, offset);
  return ret;
}

uint32_t readu24be(uint8_t *data, int offset) {
  return readu16be(data, offset) << 8 | data[offset + 2];
}

int reads24be(uint8_t *data, int offset) {
  uint32_t ret = readu16le(data, offset) << 8 | data[offset + 2];
  int iret = ret << 8;
  return (iret >> 8);
}

int reads24le(uint8_t *data, int offset) {
  uint32_t ret = readu16le(data, offset) | data[offset + 2] << 16;
  int iret = (int)(ret << 8);
  return (iret >> 8);
}

uint32_t readu32be(uint8_t *data, int offset) {
  return readu16be(data, offset) << 16 | readu16be(data, offset + 2);
}

int reads32be(uint8_t *data, int offset) {
  return (int)readu32be(data, offset);
}

int reads32le(uint8_t *data, int offset) {
  return readu16le(data, offset) | readu16le(data, offset + 2) << 16;
}
