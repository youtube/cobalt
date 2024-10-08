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
 * @file mp4demux.h
 * @brief MP4 and fMP4 demux.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint32_t first_chunk;
  uint32_t sample_per_chunk;
  uint32_t sample_offset;
  int sample_description_index;
} chunkinfo;

#ifndef _CREATE_T_
#define _CREATE_T_
enum { TAGMAX = 100 };
typedef struct {
  uint16_t atom_type;
  void *opaque;
} avio_context;
#endif

#define TRAK_TYPE_NONE 0
#define TRAK_TYPE_VIDEO 1
#define TRAK_TYPE_AUDIO 2
#define TRAK_TYPE_SUBTITLE 3
#define TRAK_TYPE_METADATA 4

typedef struct {
  uint32_t track_id;
  uint32_t ctime, mtime;
  uint32_t samplerate;
  uint32_t samples;  // total sound samples
  uint32_t channels;
  uint32_t bits;  // sample depth
  // buffer config
  uint16_t buffersize;
  struct {
    uint32_t max;
    uint32_t avg;
    int size;
    int samples;
  } bitrate;
  uint32_t framesamples;
  uint32_t sample_size;  // samples per packet (varable mode = 0, constant mode
                         // = size > 0)
  uint32_t sample_duration;
  struct {
    chunkinfo *chunks;
    uint32_t chunk_count;

    uint32_t *offs;    // sample offs array(stco)
    uint32_t *sizes;   // sample size array(stsz)
    uint32_t *deltas;  // sample count array(stts)
    uint32_t bufsize;
    uint32_t ents;
    uint32_t ents_offset;
    int current;
    int maxsize;
  } frame;
  int64_t start;

  int frag_sdidx;
  // AudioSpecificConfig data:
  struct {
    uint8_t buf[10];
    int size;
  } asc;

  int csc_count;
  int csc_idx;
  uint32_t csc_maxsize;

  void *csc;  // codec specific
  struct {
    int size;
    uint8_t *data;
  } bitbuf;  // bitstream buffer
  uint32_t bitbuf_size;

} audio_rtr_t;

typedef struct {
  FILE *fin;
  FILE *logger;
  char type[8];
  avio_context *atom;
  uint32_t ctime, mtime;
  uint32_t timescale;
  uint32_t duration;
  uint32_t tot_track_scan;
  uint32_t next_track_id;
  uint32_t mdatofs;
  uint32_t mdatsize;

  uint64_t moof_position;
  int moof_flag;

  uint64_t next_moov;
  uint64_t next_moof;

  int num_v_trak;
  int num_a_trak;
  int sel_a_trak;
  int cur_r_trak;
  int trak_type[8];
  // video_wtr_t v_trak;
  audio_rtr_t *a_trak;
  struct {
    // meta fields
    const char *encoder;
    const char *artist;
    const char *artistsort;
    const char *composer;
    const char *composersort;
    const char *title;
    const char *album;
    const char *albumartist;
    const char *albumartistsort;
    const char *albumsort;
    uint8_t compilation;
    uint32_t trackno;
    uint32_t ntracks;
    uint32_t discno;
    uint32_t ndiscs;
    int genre;
    const char *year;
    struct {
      uint8_t *data;
      int size;
    } cover;
    const char *comment;
    struct {
      const char *name;
      const char *data;
    } ext[TAGMAX];
    int extnum;
  } tag;
} mp4r_t;
#ifndef MP4ERR_DEFS
#define MP4ERR_DEFS
enum { ERR_OK = 0, ERR_FAIL = -1, ERR_UNSUPPORTED = -2 };
#endif

mp4r_t *mp4demux_open(const char *name, FILE *logger);
int mp4demux_getframenum(mp4r_t *mp4r, int trak, uint32_t offs);
int mp4demux_seek(mp4r_t *mp4r, int trak, int framenum);
int mp4demux_audio(mp4r_t *mp4r, int trak, int *delta);
int mp4demux_parse(mp4r_t *mp4r, int trak);
int mp4demux_close(mp4r_t *mp4r);
#ifdef __cplusplus
}
#endif
