/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at www.aomedia.org/license/software-license/bsd-3-c-c. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * www.aomedia.org/license/patent.
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
uint32_t bs_getExpandableSize(BitStream *b);
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
