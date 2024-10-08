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
 * @file vlogging_iamfmp4_sr.h
 * @brief verification log generator.
 * @version 0.1
 * @date Created 03/29/2023
 **/

#ifndef _VLOGGING_IAMFMP4_SR_H_
#define _VLOGGING_IAMFMP4_SR_H_

#include <stdarg.h>

#include "vlogging_tool_sr.h"

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define atom_type(c1, c2, c3, c4) \
  (((c1) << 0) + ((c2) << 8) + ((c3) << 16) + ((c4) << 24))
#elif __BYTE_ORDER == __BIG_ENDIAN
#define atom_type(c1, c2, c3, c4) \
  (((c1) << 24) + ((c2) << 16) + ((c3) << 8) + ((c4) << 0))
#endif

enum _mp4box_type {
  MP4BOX_FTYP = atom_type('f', 't', 'y', 'p'),
  MP4BOX_MOOV = atom_type('m', 'o', 'o', 'v'),
  MP4BOX_TRAK = atom_type('t', 'r', 'a', 'k'),
  MP4BOX_MVHD = atom_type('m', 'v', 'h', 'd'),
  MP4BOX_MVEX = atom_type('m', 'v', 'e', 'x'),
  MP4BOX_TREX = atom_type('t', 'r', 'e', 'x'),
  MP4BOX_EDTS = atom_type('e', 'd', 't', 's'),
  MP4BOX_ELST = atom_type('e', 'l', 's', 't'),
  MP4BOX_MDHD = atom_type('m', 'd', 'h', 'd'),
  MP4BOX_HDLR = atom_type('h', 'd', 'l', 'r'),
  MP4BOX_MINF = atom_type('m', 'i', 'n', 'f'),
  MP4BOX_STBL = atom_type('s', 't', 'b', 'l'),
  MP4BOX_STTS = atom_type('s', 't', 't', 's'),
  MP4BOX_STSC = atom_type('s', 't', 's', 'c'),
  MP4BOX_STSZ = atom_type('s', 't', 's', 'z'),
  MP4BOX_STCO = atom_type('s', 't', 'c', 'o'),
  MP4BOX_STSD = atom_type('s', 't', 's', 'd'),
  MP4BOX_SGPD = atom_type('s', 'p', 'g', 'd'),
  MP4BOX_MOOF = atom_type('m', 'o', 'o', 'f'),
  MP4BOX_TRAF = atom_type('t', 'r', 'a', 'f'),
  MP4BOX_MFHD = atom_type('m', 'f', 'h', 'd'),
  MP4BOX_TKHD = atom_type('t', 'k', 'h', 'd'),
  MP4BOX_TFHD = atom_type('t', 'f', 'h', 'd'),
  MP4BOX_TRUN = atom_type('t', 'r', 'u', 'n'),
  MP4BOX_MDAT = atom_type('m', 'd', 'a', 't'),

  /* Immersive audio atom */
  MP4BOX_IAMF = atom_type('i', 'a', 'm', 'f'),
  MP4BOX_IACB = atom_type('i', 'a', 'c', 'b'),
};

int vlog_atom(uint32_t atom_type, void* atom_d, int size, uint64_t atom_addr);
#endif