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
 * @file mp4iamfpar.c
 * @brief MP4 and fMP4 file parser.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#include "mp4iamfpar.h"

#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

#include "atom.h"

void mp4_iamf_parser_init(MP4IAMFParser *ths) {
  ths->m_mp4r = NULL;
  ths->m_logger = stderr;
}

int mp4_iamf_parser_open_audio_track(MP4IAMFParser *ths, const char *mp4file,
                                     IAMFHeader **header) {
  if ((ths->m_mp4r = mp4demux_open(mp4file, ths->m_logger)) == NULL) {
    fprintf(stderr, "file open error\n");
    return -1;
  }

  return mp4_iamf_parser_get_audio_track_header(ths, header);
}

int mp4_iamf_parser_get_audio_track_header(MP4IAMFParser *ths,
                                           IAMFHeader **header) {
  IAMFHeader *iamf_header;
  audio_rtr_t *atr = 0;
  int idx = 0, i = 0;

  if (!ths->m_mp4r) {
    fprintf(stderr, "There is no mp4 file.\n");
    return -1;
  }

  atr = ths->m_mp4r->a_trak;
  if (!atr) {
    fprintf(stderr, "MP4 file does not have track.\n");
    return -1;
  }

  iamf_header = (IAMFHeader *)atr->csc;
  if (iamf_header == NULL || atr == NULL) {
    fprintf(stderr, "MP4 file does not have audio track.\n");
    return -1;
  }

  idx = atr[0].frame.current - atr[0].frame.ents_offset;
  if (ths->m_mp4r->moof_flag) {
    i = atr[0].frag_sdidx;
    if (i < 0) i = ~i;
    if (i) --i;
  } else {
    for (; i < atr[0].frame.chunk_count; ++i) {
      idx -= (int)(atr[0].frame.chunks[i].sample_per_chunk +
                   atr[0].frame.chunks[i].sample_offset);
      if (idx < 0) break;
    }
  }

  *header = &iamf_header[i];

  return 1;
}

int mp4_iamf_parser_read_packet(MP4IAMFParser *ths, int trakn,
                                uint8_t **pkt_buf, uint32_t *pktlen_out,
                                int64_t *sample_offs, int *cur_entno) {
  audio_rtr_t *atr = ths->m_mp4r->a_trak;
  int sample_delta = 0, idx = 0;
  int ret = 0;
  uint32_t used = 0;
  IAMFHeader *header;
  uint8_t *pkt = 0;

  if (atr[trakn].frame.ents_offset + atr[trakn].frame.ents <=
      atr[trakn].frame.current) {
    if (ths->m_mp4r->moof_flag) {
      if (mp4demux_parse(ths->m_mp4r, trakn) < 0)
        return (-1);
      else {
        header = (IAMFHeader *)atr[trakn].csc;
        if (ths->m_mp4r->next_moov > 0 &&
            ths->m_mp4r->next_moov < ths->m_mp4r->next_moof &&
            header->description &&
            !iamf_header_read_description_OBUs(header, &pkt, &used))
          return (-1);
      }
    } else
      return (-1);
  }

  idx = atr[trakn].frame.current - atr[trakn].frame.ents_offset;

  ret = idx;
  if (ths->m_mp4r->moof_flag) {
    if (!ret && atr[trakn].frag_sdidx < 0) {
      header = (IAMFHeader *)atr[trakn].csc;
      atr[trakn].frag_sdidx = ~atr[trakn].frag_sdidx;
      ret = iamf_header_read_description_OBUs(
          &header[atr[trakn].frag_sdidx - 1], &pkt, &used);
      /* printf("Read next description at %d.\n", atr[trakn].frame.current); */
    }
  } else {
    int sdi, sdin;
    for (int i = 0; i < atr[trakn].frame.chunk_count; ++i) {
      ret -= (int)(atr[trakn].frame.chunks[i].sample_per_chunk +
                   atr[trakn].frame.chunks[i].sample_offset);
      if (!ret) {
        sdi = atr[trakn].frame.chunks[i].sample_description_index;
        sdin = atr[trakn].frame.chunks[i + 1].sample_description_index;
        if (sdi != sdin && sdin > 0) {
          header = (IAMFHeader *)atr[trakn].csc;
          ret =
              iamf_header_read_description_OBUs(&header[sdin - 1], &pkt, &used);
          printf("Read next samples description (%d -> )%d at %d\n", sdi, sdin,
                 atr[trakn].frame.current);
        }
        break;
      } else if (ret < 0)
        break;
    }
  }

  if (sample_offs && atr[trakn].frame.offs) {
    *sample_offs = atr[trakn].frame.offs[idx];
  }

  if (cur_entno) {
    *cur_entno = atr[trakn].frame.current + 1;
  }

  if (mp4demux_audio(ths->m_mp4r, trakn, &sample_delta) != ERR_OK) {
    fprintf(stderr, "mp4demux_frame() error\n");
    return (-1);
  }

  pkt = (uint8_t *)realloc(pkt, used + atr[trakn].bitbuf.size);
  if (!pkt) return (-1);
  memcpy((uint8_t *)pkt + used, atr[trakn].bitbuf.data, atr[trakn].bitbuf.size);
  *pkt_buf = pkt;
  *pktlen_out = used + atr[trakn].bitbuf.size;
  return (sample_delta);
}

void mp4_iamf_parser_close(MP4IAMFParser *ths) {
  if (ths->m_mp4r) {
    mp4demux_close(ths->m_mp4r);  // close all track ids, too.
  }
  ths->m_mp4r = NULL;
}

int mp4_iamf_parser_set_logger(MP4IAMFParser *ths, FILE *logger) {
  ths->m_logger = logger;
  return (0);
}
