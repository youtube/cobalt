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
 * @file bitstream.h
 * @brief bitstream reader APIs.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifndef BIT_STREAM_H
#define BIT_STREAM_H

#include <stdint.h>

#define INT8_BITS 8
#define INT16_BITS 16
#define INT32_BITS 32

typedef struct {
  const uint8_t *data;
  uint32_t size;
  uint32_t b8sp;  // bytes, less than size;
  uint32_t b8p;   // 0~7
} BitStream;

int32_t bs(BitStream *b, const uint8_t *data, int size);
uint32_t bs_get32b(BitStream *b, int n);
int32_t bs_skip(BitStream *b, int n);
void bs_align(BitStream *b);
int32_t bs_skipABytes(BitStream *b, int n);
uint32_t bs_getA8b(BitStream *b);
uint32_t bs_getA16b(BitStream *b);
uint32_t bs_getA32b(BitStream *b);
uint64_t bs_getAleb128(BitStream *b);
int32_t bs_read(BitStream *b, uint8_t *data, int n);
int32_t bs_readString(BitStream *b, char *data, int n);
uint32_t bs_tell(BitStream *b);

int reads16be(uint8_t *data, int offset);
int reads16le(uint8_t *data, int offset);
int reads24be(uint8_t *data, int offset);
int reads24le(uint8_t *data, int offset);
int reads32be(uint8_t *data, int offset);
int reads32le(uint8_t *data, int offset);

uint8_t readu8(uint8_t *data, int offset);
uint32_t readu16be(uint8_t *data, int offset);
uint32_t readu24be(uint8_t *data, int offset);
uint32_t readu32be(uint8_t *data, int offset);

#endif /* BIT_STREAM_H */
