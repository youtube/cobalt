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
 * @file atom.h
 * @brief atom.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifndef ATOM_H_INCLUDED
#define ATOM_H_INCLUDED

#include <stdint.h>
#include <stdio.h>

#pragma pack(push, 1)

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define atom_type(c1, c2, c3, c4) \
  (((c1) << 0) + ((c2) << 8) + ((c3) << 16) + ((c4) << 24))
#elif __BYTE_ORDER == __BIG_ENDIAN
#define atom_type(c1, c2, c3, c4) \
  (((c1) << 24) + ((c2) << 16) + ((c3) << 8) + ((c4) << 0))
#endif

#define atom_type_string(t) ((char[5]){((t) >> 0) & 0xff, 0, 0, 0, 0})

enum {
  ATOM_TYPE_FTYP = atom_type('f', 't', 'y', 'p'),
  ATOM_TYPE_MDAT = atom_type('m', 'd', 'a', 't'),
  ATOM_TYPE_MOOV = atom_type('m', 'o', 'o', 'v'),
  ATOM_TYPE_MOOF = atom_type('m', 'o', 'o', 'f'),
  ATOM_TYPE_TRAK = atom_type('t', 'r', 'a', 'k'),
  ATOM_TYPE_TKHD = atom_type('t', 'k', 'h', 'd'),
  ATOM_TYPE_TRAF = atom_type('t', 'r', 'a', 'f'),
  ATOM_TYPE_TRUN = atom_type('t', 'r', 'u', 'n'),
  ATOM_TYPE_MVHD = atom_type('m', 'v', 'h', 'd'),
  ATOM_TYPE_MDIA = atom_type('m', 'd', 'i', 'a'),
  ATOM_TYPE_MINF = atom_type('m', 'i', 'n', 'f'),
  ATOM_TYPE_STBL = atom_type('s', 't', 'b', 'l'),
  ATOM_TYPE_STTS = atom_type('s', 't', 't', 's'),
  ATOM_TYPE_STSC = atom_type('s', 't', 's', 'c'),
  ATOM_TYPE_STSZ = atom_type('s', 't', 's', 'z'),
  ATOM_TYPE_STCO = atom_type('s', 't', 'c', 'o'),
};

typedef struct atom_header_s {
  uint32_t size;
  uint32_t type;
  uint64_t size_extended;
} atom_header;

typedef struct atom_s {
  unsigned char *data;
  unsigned char *data_end;
  uint32_t type;
  uint32_t header_size;
  uint32_t size;
  uint32_t apos;
  int depth;
} atom;

typedef struct atom_mvhd_v1_s {
  uint8_t version;
  uint8_t flags[3];
  uint32_t created;
  uint32_t modified;
  uint32_t scale;
  uint32_t duration;
  uint32_t speed;
  uint16_t volume;
  uint16_t reserved[5];
  uint32_t matrix[9];
  uint64_t quicktime_preview;
  uint32_t quicktime_poster;
  uint64_t quicktime_selection;
  uint32_t quicktime_current_time;
  uint32_t next_track_id;
} atom_mvhd_v1;

void atom_set_logger(FILE *logger);
int atom_dump(FILE *fp, uint64_t apos, uint32_t tmp);

#pragma pack(pop)

#endif /* ATOM_H_INCLUDED */
