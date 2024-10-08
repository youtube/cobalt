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
 * @file mp4iamfpar.h
 * @brief MP4 and fMP4 file parser.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifndef __MP4_IAMF_PAR_H_
#define __MP4_IAMF_PAR_H_

#include "iamf_header.h"
#include "mp4demux.h"

typedef struct {
  mp4r_t *m_mp4r;
  FILE *m_logger;
} MP4IAMFParser;

void mp4_iamf_parser_init(MP4IAMFParser *);
int mp4_iamf_parser_open_audio_track(MP4IAMFParser *, const char *mp4file,
                                     IAMFHeader **header);
int mp4_iamf_parser_get_audio_track_header(MP4IAMFParser *,
                                           IAMFHeader **header);
int mp4_iamf_parser_read_packet(MP4IAMFParser *, int trakn, uint8_t **pkt_buf,
                                uint32_t *pktlen_out, int64_t *sample_offs,
                                int *ent_no);
void mp4_iamf_parser_close(MP4IAMFParser *);
int mp4_iamf_parser_set_logger(MP4IAMFParser *, FILE *logger);

#endif /* __MP4_IAMF_PAR_H_ */
