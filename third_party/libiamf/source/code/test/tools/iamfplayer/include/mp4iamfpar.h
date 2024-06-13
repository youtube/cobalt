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
