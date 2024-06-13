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
