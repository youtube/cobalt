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
 * @file iamf_header.c
 * @brief IAMF information.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include "atom.h"
#include "dmemory.h"
#include "iamf_header.h"

/* Header contents:
  - "OpusHead" (64 bits)
  - version number (8 bits)
  - Channels C (8 bits)
  - Pre-skip (16 bits)
  - Sampling rate (32 bits)
  - Gain in dB (16 bits, S7.8)
  - Mapping (8 bits, 0=single stream (mono/stereo) 1=Vorbis mapping,
             2..254: reserved, 255: multistream with no mapping)

  - if (mapping != 0)
     - N = total number of streams (8 bits)
     - M = number of paired streams (8 bits)
     - C times channel origin
          - if (C<2*M)
             - stream = byte/2
             - if (byte&0x1 == 0)
                 - left
               else
                 - right
          - else
             - stream = byte-M
*/

typedef struct {
  const unsigned char *data;
  int maxlen;
  int pos;
} ROPacket;

int iamf_header_read_description_OBUs(IAMFHeader *h, uint8_t **dst,
                                      uint32_t *size) {
  uint8_t *dsc = 0;

  dsc = (uint8_t *)malloc(h->description_length);
  if (dsc) {
    memcpy(dsc, h->description, h->description_length);
    *dst = dsc;
    *size = h->description_length;
    return h->description_length;
  }
  return 0;
}

#define FREE(x)                    \
  if (x) {                         \
    _dfree(x, __FILE__, __LINE__); \
    x = 0;                         \
  }
void iamf_header_free(IAMFHeader *h, int n) {
  if (h) {
    for (int i = 0; i < n; ++i) {
      FREE(h[i].description);
    }
  }
  FREE(h);
}
