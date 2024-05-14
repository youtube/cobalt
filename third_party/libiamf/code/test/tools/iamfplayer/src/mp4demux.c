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
 * @file mp4demux.c
 * @brief MP4 and fMP4 demux.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#include "mp4demux.h"

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "a2b_endian.h"
#include "atom.h"
#include "dmemory.h"
#include "iamf_header.h"

#ifndef SUPPORT_VERIFIER
#define SUPPORT_VERIFIER 0
#endif
#if SUPPORT_VERIFIER
#include "vlogging_iamfmp4_sr.h"
#endif

#define MKTAG(a, b, c, d) \
  ((a) | ((b) << 8) | ((c) << 16) | ((unsigned)(d) << 24))
#define FREE(x)                    \
  if (x) {                         \
    _dfree(x, __FILE__, __LINE__); \
    x = 0;                         \
  }

enum MOV_ATOM_TYPE {
  MOV_ATOM_STOP = 0, /* end of atoms */
  MOV_ATOM_NAME,     /* plain atom */
  MOV_ATOM_DESCENT,  /* starts group of children */
  MOV_ATOM_ASCENT,   /* ends group */
  MOV_ATOM_DATA,
};

static int mov_read_trak(mp4r_t *mp4r, int size);
static int mov_read_iamf(mp4r_t *mp4r, int size);
static int mov_read_edts(mp4r_t *mp4r, int size);
static int mov_read_elst(mp4r_t *mp4r, int size);
static int mov_read_tkhd(mp4r_t *mp4r, int size);
static int mov_read_mdhd(mp4r_t *mp4r, int size);
static int mov_read_hdlr(mp4r_t *mp4r, int size);
static int mov_read_stbl(mp4r_t *mp4r, int size);
static int mov_read_stsd(mp4r_t *mp4r, int size);
static int mov_read_stts(mp4r_t *mp4r, int size);
static int mov_read_stsc(mp4r_t *mp4r, int size);
static int mov_read_stsz(mp4r_t *mp4r, int size);
static int mov_read_stco(mp4r_t *mp4r, int size);
#if SUPPORT_VERIFIER
static int mov_read_sgpd(mp4r_t *mp4r, int size);
#endif
static avio_context atoms_tkhd[] = {
    {MOV_ATOM_NAME, "tkhd"}, {MOV_ATOM_DATA, mov_read_tkhd}, {0}};

static avio_context atoms_mdia[] = {{MOV_ATOM_NAME, "mdia"},
                                    {MOV_ATOM_DESCENT},
                                    {MOV_ATOM_NAME, "mdhd"},
                                    {MOV_ATOM_DATA, mov_read_mdhd},
                                    {MOV_ATOM_NAME, "hdlr"},
                                    {MOV_ATOM_DATA, mov_read_hdlr},
                                    {0}};

static avio_context atoms_stbl[] = {{MOV_ATOM_NAME, "hdlr"},
                                    {MOV_ATOM_NAME, "minf"},
                                    {MOV_ATOM_DESCENT},
                                    {MOV_ATOM_NAME, "dinf"},
                                    {MOV_ATOM_NAME, "stbl"},
                                    {MOV_ATOM_DATA, mov_read_stbl},
                                    {0}};

static avio_context atoms_stsd[] = {
    {MOV_ATOM_NAME, "stsd"}, {MOV_ATOM_DATA, mov_read_stsd}, {0}};
static avio_context atoms_stts[] = {
    {MOV_ATOM_NAME, "stts"}, {MOV_ATOM_DATA, mov_read_stts}, {0}};
static avio_context atoms_stsc[] = {
    {MOV_ATOM_NAME, "stsc"}, {MOV_ATOM_DATA, mov_read_stsc}, {0}};
static avio_context atoms_stsz[] = {
    {MOV_ATOM_NAME, "stsz"}, {MOV_ATOM_DATA, mov_read_stsz}, {0}};
static avio_context atoms_stco[] = {
    {MOV_ATOM_NAME, "stco"}, {MOV_ATOM_DATA, mov_read_stco}, {0}};
#if SUPPORT_VERIFIER
static avio_context atoms_sgpd[] = {
    {MOV_ATOM_NAME, "sgpd"}, {MOV_ATOM_DATA, mov_read_sgpd}, {0}};
#endif
static avio_context atoms_edts[] = {
    {MOV_ATOM_NAME, "edts"}, {MOV_ATOM_DATA, mov_read_edts}, {MOV_ATOM_DESCENT},
    {MOV_ATOM_NAME, "elst"}, {MOV_ATOM_DATA, mov_read_elst}, {0}};

static avio_context atoms_iamf[] = {
    {MOV_ATOM_NAME, "iamf"}, {MOV_ATOM_DATA, mov_read_iamf}, {0}};

static int parse(mp4r_t *mp4r, uint32_t *sizemax);

static int avio_rdata(FILE *fin, void *data, int size) {
  if (fread(data, 1, size, fin) != size) {
    return ERR_FAIL;
  }
  return size;
}

static int avio_rstring(FILE *fin, char *txt, int sizemax) {
  int size;
  for (size = 0; size < sizemax; size++) {
    if (fread(txt + size, 1, 1, fin) != 1) {
      return ERR_FAIL;
    }
    if (!txt[size]) {
      break;
    }
  }
  txt[sizemax - 1] = '\0';

  return size;
}

#define avio_rb32() avio_rb32_(mp4r)
static uint32_t avio_rb32_(mp4r_t *mp4r) {
  FILE *fin = mp4r->fin;
  uint32_t val = 0;
  avio_rdata(fin, &val, 4);
  val = bswap32(val);
  return val;
}

#define avio_rb16() avio_rb16_(mp4r)
static uint16_t avio_rb16_(mp4r_t *mp4r) {
  FILE *fin = mp4r->fin;
  uint16_t val;
  avio_rdata(fin, &val, 2);
  val = bswap16(val);
  return val;
}

#define avio_r8() avio_r8_(mp4r)
static int avio_r8_(mp4r_t *mp4r) {
  FILE *fin = mp4r->fin;
  uint8_t val;
  avio_rdata(fin, &val, 1);
  return val;
}

static int mov_read_ftyp(mp4r_t *mp4r, int size) {
  enum { BUFSIZE = 40 };
  char buf[BUFSIZE];
  uint32_t val;

#if SUPPORT_VERIFIER
  char *atom_d = (char *)malloc(size);
  int fpos;
  fpos = ftell(mp4r->fin);
  avio_rdata(mp4r->fin, atom_d, size);
  fseek(mp4r->fin, fpos, SEEK_SET);
  vlog_atom(MP4BOX_FTYP, atom_d, size, fpos - 8);
  free(atom_d);
#endif

  buf[4] = 0;
  avio_rdata(mp4r->fin, buf, 4);
  val = avio_rb32();

  if (mp4r->logger) {
    fprintf(mp4r->logger, "Brand:\t\t\t%s(version %d)\n", buf, val);
  }
  avio_rstring(mp4r->fin, buf, BUFSIZE);
  if (mp4r->logger) {
    fprintf(mp4r->logger, "Compatible brands:\t%s\n", buf);
  }

  return size;
}

enum { SECSINDAY = 24 * 60 * 60 };

#if 0
static char *mp4time(time_t t) {
  int y;

  // subtract some seconds from the start of 1904 to the start of 1970
  for (y = 1904; y < 1970; y++) {
    t -= 365 * SECSINDAY;
    if (!(y & 3)) {
      t -= SECSINDAY;
    }
  }
  return ctime(&t);
}
#endif

static int mov_read_mvhd(mp4r_t *mp4r, int size) {
#if SUPPORT_VERIFIER
  char *atom_d = (char *)malloc(size);
  int fpos;
  fpos = ftell(mp4r->fin);
  avio_rdata(mp4r->fin, atom_d, size);
  fseek(mp4r->fin, fpos, SEEK_SET);
  vlog_atom(MP4BOX_MVHD, atom_d, size, fpos - 8);
  free(atom_d);
#endif

  uint32_t x;
  // version/flags
  avio_rb32();
  // Creation time
  mp4r->ctime = avio_rb32();
  // Modification time
  mp4r->mtime = avio_rb32();
  // Time scale
  mp4r->timescale = avio_rb32();
  // Duration
  mp4r->duration = avio_rb32();
  // reserved
  avio_rb32();
  avio_rb32();
  for (x = 0; x < 2; x++) {
    avio_rb32();
  }
  for (x = 0; x < 9; x++) {
    avio_rb32();
  }
  for (x = 0; x < 6; x++) {
    avio_rb32();
  }
  // next track id
  mp4r->tot_track_scan = 0;
  mp4r->next_track_id = avio_rb32();

  return size;
}

int mov_read_mdhd(mp4r_t *mp4r, int size) {
#if SUPPORT_VERIFIER
  char *atom_d = (char *)malloc(size);
  int fpos;
  fpos = ftell(mp4r->fin);
  avio_rdata(mp4r->fin, atom_d, size);
  fseek(mp4r->fin, fpos, SEEK_SET);
  vlog_atom(MP4BOX_MDHD, atom_d, size, fpos - 8);
  free(atom_d);
#endif

  int sel_a_trak;
  if (mp4r->trak_type[mp4r->cur_r_trak] == TRAK_TYPE_AUDIO) {
    sel_a_trak = mp4r->sel_a_trak;
    audio_rtr_t *atr = mp4r->a_trak;

    // version/flags
    avio_rb32();
    // Creation time
    atr[sel_a_trak].ctime = avio_rb32();
    // Modification time
    atr[sel_a_trak].mtime = avio_rb32();
    // Time scale
    atr[sel_a_trak].samplerate = avio_rb32();
    // Duration
    atr[sel_a_trak].samples = avio_rb32();
    // Language
    avio_rb16();
    // pre_defined
    avio_rb16();
  }
  return size;
}

#define STASH_ATOM() avio_context *sa = mp4r->atom
#define RESTORE_ATOM() mp4r->atom = sa;

int atom_seek_parse(mp4r_t *mp4r, int64_t pos, int size, avio_context *atoms) {
  fseek(mp4r->fin, pos, SEEK_SET);
  mp4r->atom = atoms;
  return parse(mp4r, &size);
}

int mov_read_trak(mp4r_t *mp4r, int size) {
#if SUPPORT_VERIFIER
  char *atom_d = (char *)malloc(size);
  int fpos;
  fpos = ftell(mp4r->fin);
  avio_rdata(mp4r->fin, atom_d, size);
  fseek(mp4r->fin, fpos, SEEK_SET);
  vlog_atom(MP4BOX_TRAK, atom_d, size, fpos - 8);
  free(atom_d);
#endif

  int64_t apos = ftell(mp4r->fin);
  avio_context *list[] = {atoms_tkhd, atoms_edts, atoms_mdia};
  STASH_ATOM();
  for (int i = 0; i < sizeof(list) / sizeof(avio_context *); ++i)
    atom_seek_parse(mp4r, apos, size, list[i]);
  RESTORE_ATOM();
  return size;
}

int mov_read_stbl(mp4r_t *mp4r, int size) {
#if SUPPORT_VERIFIER
  char *atom_d = (char *)malloc(size);
  int fpos;
  fpos = ftell(mp4r->fin);
  avio_rdata(mp4r->fin, atom_d, size);
  fseek(mp4r->fin, fpos, SEEK_SET);
  vlog_atom(MP4BOX_STBL, atom_d, size, fpos - 8);
  free(atom_d);
#endif

  int64_t apos = ftell(mp4r->fin);
  avio_context *list[] = {
    atoms_stsd,
    atoms_stts,
    atoms_stsc,
    atoms_stsz,
    atoms_stco
#if SUPPORT_VERIFIER
    ,
    atoms_sgpd
#endif
  };
  STASH_ATOM();
  for (int i = 0; i < sizeof(list) / sizeof(avio_context *); ++i)
    atom_seek_parse(mp4r, apos, size, list[i]);
  RESTORE_ATOM();
  return size;
}

int mov_read_tkhd(mp4r_t *mp4r, int size) {
#if SUPPORT_VERIFIER
  char *atom_d = (char *)malloc(size);
  int fpos;
  fpos = ftell(mp4r->fin);
  avio_rdata(mp4r->fin, atom_d, size);
  fseek(mp4r->fin, fpos, SEEK_SET);
  vlog_atom(MP4BOX_TKHD, atom_d, size, fpos - 8);
  free(atom_d);
#endif

  // version/flags
  avio_rb32();
  // creation-time
  avio_rb32();
  // modification-time
  avio_rb32();
  // track id
  mp4r->tot_track_scan++;
  uint32_t track_id = avio_rb32();
  mp4r->cur_r_trak = track_id - 1;
  // reserved
  avio_rb32();
  // duration
  avio_rb32();
  // reserved * 3
  for (int i = 0; i < 3; i++) {
    avio_rb32();
  }
  // reserved 16bit, 16bit, x == 0x01000000
  avio_rb32();
  // reserved * 9
  for (int i = 0; i < 9; i++) {
    avio_rb32();
  }
  // reserved
  uint32_t w = avio_rb32();
  // reserved
  uint32_t h = avio_rb32();
  if (w == 0 && h == 0) {
    mp4r->a_trak = (audio_rtr_t *)_drealloc(
        (char *)mp4r->a_trak, sizeof(audio_rtr_t) * (mp4r->num_a_trak + 1),
        __FILE__, __LINE__);
    memset(((uint8_t *)mp4r->a_trak) + sizeof(audio_rtr_t) * (mp4r->num_a_trak),
           0, sizeof(audio_rtr_t));
    mp4r->sel_a_trak = mp4r->num_a_trak;
    mp4r->num_a_trak++;
    mp4r->a_trak[mp4r->sel_a_trak].track_id = track_id;
    mp4r->trak_type[mp4r->cur_r_trak] = TRAK_TYPE_AUDIO;
  } else {
    mp4r->trak_type[mp4r->cur_r_trak] = TRAK_TYPE_VIDEO;
  }

  return (size);
}

int mov_read_stsd(mp4r_t *mp4r, int size) {
#if SUPPORT_VERIFIER
  char *atom_d = (char *)malloc(size);
  int fpos;
  fpos = ftell(mp4r->fin);
  avio_rdata(mp4r->fin, atom_d, size);
  fseek(mp4r->fin, fpos, SEEK_SET);
  vlog_atom(MP4BOX_STSD, atom_d, size, fpos - 8);
  free(atom_d);
#endif

  int n;
  avio_context *pos = mp4r->atom;

  // version/flags
  avio_rb32();
  n = avio_rb32();

  for (int i = 0; i < n; ++i) {
    mp4r->atom = atoms_iamf;
    parse(mp4r, &size);
  }
  mp4r->atom = pos;
  return size;
}

int mov_read_edts(mp4r_t *mp4r, int size) {
#if SUPPORT_VERIFIER
  char *atom_d = (char *)malloc(size);
  int fpos;
  fpos = ftell(mp4r->fin);
  avio_rdata(mp4r->fin, atom_d, size);
  fseek(mp4r->fin, fpos, SEEK_SET);
  vlog_atom(MP4BOX_EDTS, atom_d, size, fpos - 8);
  free(atom_d);
#endif
  return size;
}

int mov_read_elst(mp4r_t *mp4r, int size) {
#if SUPPORT_VERIFIER
  char *atom_d = (char *)malloc(size);
  int fpos;
  fpos = ftell(mp4r->fin);
  avio_rdata(mp4r->fin, atom_d, size);
  fseek(mp4r->fin, fpos, SEEK_SET);
  vlog_atom(MP4BOX_ELST, atom_d, size, fpos - 8);
  free(atom_d);
#endif
  int sel_a_trak = mp4r->sel_a_trak;
  audio_rtr_t *atr = mp4r->a_trak;
  int entry_count;
  int ver = avio_r8();
  int64_t start = 0;

  avio_r8();
  avio_rb16();  // flag
  entry_count = avio_rb32();

  for (int i = 0; i < entry_count; ++i) {
    if (ver) {
      avio_rb32();
      avio_rb32();  // segment_duration;

      start = avio_rb32();
      start = start << 32 | avio_rb32();  // media_time;
    } else {
      avio_rb32();          // segment_duration;
      start = avio_rb32();  // media_time;
    }
    avio_rb16();  // media_rate_integer
    avio_rb16();  // edia_rate_fraction
  }

  if (!atr[sel_a_trak].start && start > 0) {
    atr[sel_a_trak].start = start;
    /* printf("get media time %" PRId64"\n", start); */
  }
  return size;
}

int mov_read_iamf(mp4r_t *mp4r, int size) {
#if SUPPORT_VERIFIER
  char *atom_d = (char *)malloc(size);
  int fpos;
  fpos = ftell(mp4r->fin);
  avio_rdata(mp4r->fin, atom_d, size);
  fseek(mp4r->fin, fpos, SEEK_SET);
  vlog_atom(MP4BOX_IAMF, atom_d, size, fpos - 8);
  free(atom_d);
#endif

  int sel_a_trak;
  int offset;
  int tag;
  void *csc;
  IAMFHeader *header;

  sel_a_trak = mp4r->sel_a_trak;
  audio_rtr_t *atr = mp4r->a_trak;

  // Reserved (6 bytes)
  avio_rb32();  // reserved
  avio_rb16();  // reserved
  avio_rb16();  // data_reference_index
  avio_rb32();  // reserved
  avio_rb32();  // reserved
  uint16_t channel_count = avio_rb16();
  atr[sel_a_trak].channels = channel_count;
  uint16_t sample_size = avio_rb16();
  atr[sel_a_trak].bits = sample_size;  // bitdepth
  avio_rb16();                         // predefined
  avio_rb16();                         // reserved
  avio_rb32();                         // sample_rate

  csc = atr[sel_a_trak].csc;
  atr[sel_a_trak].csc = _drealloc(
      atr[sel_a_trak].csc, sizeof(IAMFHeader) * (atr[sel_a_trak].csc_count + 1),
      __FILE__, __LINE__);
  if (atr[sel_a_trak].csc) {
    ++atr[sel_a_trak].csc_count;
    header = (IAMFHeader *)atr[sel_a_trak].csc;
    header->skip = atr[sel_a_trak].start;
    header->timescale = atr[sel_a_trak].samplerate;
  } else
    atr[sel_a_trak].csc = csc;

  header = (IAMFHeader *)atr[sel_a_trak].csc;
  int idx = atr[sel_a_trak].csc_count - 1;

  int codec_size = size - 28;
  header[idx].description_length = codec_size;
  header[idx].description =
      _dmalloc(header->description_length, __FILE__, __LINE__);
  avio_rdata(mp4r->fin, header[idx].description,
             header[idx].description_length);

  if (codec_size > atr[sel_a_trak].csc_maxsize)
    atr[sel_a_trak].csc_maxsize = codec_size;
  /* fprintf(stderr, "* IAMF description %d length %d\n", idx, */
  /* header[idx].description_length); */

  return size;
}

int mov_read_stts(mp4r_t *mp4r, int size) {
#if SUPPORT_VERIFIER
  char *atom_d = (char *)malloc(size);
  int fpos;
  fpos = ftell(mp4r->fin);
  avio_rdata(mp4r->fin, atom_d, size);
  fseek(mp4r->fin, fpos, SEEK_SET);
  vlog_atom(MP4BOX_STTS, atom_d, size, fpos - 8);
  free(atom_d);
#endif

  uint32_t entry_count;
  uint32_t sample_count, sample_delta;
  uint32_t *count_buf, *delta_buf;
  uint32_t x, cnt;
  int ret = size;

  /* if (size < 16) { //min stts size */
  /* return ERR_FAIL; */
  /* } */

  int sel_a_trak;
  sel_a_trak = mp4r->sel_a_trak;
  audio_rtr_t *atr = mp4r->a_trak;

  // version/flags
  avio_rb32();

  // entry count
  entry_count = avio_rb32();
  /* fprintf(stderr, "sttsin: entry_count %d\n", entry_count); */
  if (!entry_count) return size;

  if (!(entry_count + 1)) {
    return ERR_FAIL;
  }

  count_buf = (uint32_t *)_dcalloc(sizeof(uint32_t), entry_count + 1, __FILE__,
                                   __LINE__);
  delta_buf = (uint32_t *)_dcalloc(sizeof(uint32_t), entry_count + 1, __FILE__,
                                   __LINE__);

  int count_detas = 0;
  for (cnt = 0; cnt < entry_count; cnt++) {
    // chunk entry loop
    sample_count = avio_rb32();
    sample_delta = avio_rb32();
    count_buf[cnt] = sample_count;
    delta_buf[cnt] = sample_delta;
    count_detas += sample_count;
    // fprintf(stderr, "sttsin: entry_count %d, sample_count %u\n", entry_count,
    // sample_count);
    // fprintf(stderr, "sttsin: entry_count %d, sample_delta %u\n", entry_count,
    // sample_delta);
  }
  // fprintf(stderr, "trak id %d\n", sel_a_trak);
  atr[sel_a_trak].frame.deltas = (uint32_t *)_dcalloc(
      count_detas + 1, sizeof(*atr[sel_a_trak].frame.deltas), __FILE__,
      __LINE__);
  if (!atr[sel_a_trak].frame.deltas) {
    ret = ERR_FAIL;
  } else {
    for (cnt = 0, x = 0; cnt < entry_count; cnt++) {
      // chunk entry loop
      sample_count = count_buf[cnt];
      sample_delta = delta_buf[cnt];
      for (int i = 0; i < sample_count; i++) {
        //
        atr[sel_a_trak].frame.deltas[x] = sample_delta;
        x++;
      }
    }
  }
  if (count_buf) {
    _dfree(count_buf, __FILE__, __LINE__);
  }
  if (delta_buf) {
    _dfree(delta_buf, __FILE__, __LINE__);
  }

  return ret;
}

int mov_read_stsc(mp4r_t *mp4r, int size) {
#if SUPPORT_VERIFIER
  char *atom_d = (char *)malloc(size);
  int fpos;
  fpos = ftell(mp4r->fin);
  avio_rdata(mp4r->fin, atom_d, size);
  fseek(mp4r->fin, fpos, SEEK_SET);
  vlog_atom(MP4BOX_STSC, atom_d, size, fpos - 8);
  free(atom_d);
#endif

  uint32_t entry_count;
  uint32_t first_chunk;
  uint32_t samples_in_chunk;
  uint32_t sample_desc_index;
  int used_bytes = 0;

  int sel_a_trak;
  sel_a_trak = mp4r->sel_a_trak;
  audio_rtr_t *atr = mp4r->a_trak;

  // version/flags
  avio_rb32();
  used_bytes += 4;
  // Sample size
  entry_count = avio_rb32();
  /* fprintf(stderr, "stscin: entry_count %d\n", entry_count); */
  if (!entry_count) return size;
  used_bytes += 4;
  atr[sel_a_trak].frame.chunk_count = entry_count;
  atr[sel_a_trak].frame.chunks = (chunkinfo *)_dcalloc(
      sizeof(chunkinfo), entry_count + 1, __FILE__, __LINE__);
  if (!atr[sel_a_trak].frame.chunks) {
    return ERR_FAIL;
  }

  for (int i = 0; used_bytes < size && i < entry_count; i++) {
    first_chunk = avio_rb32();
    samples_in_chunk = avio_rb32();
    sample_desc_index = avio_rb32();
    atr[sel_a_trak].frame.chunks[i].first_chunk = first_chunk;
    atr[sel_a_trak].frame.chunks[i].sample_per_chunk = samples_in_chunk;
    atr[sel_a_trak].frame.chunks[i].sample_description_index =
        sample_desc_index;
    if (i)
      atr[sel_a_trak].frame.chunks[i].sample_offset =
          atr[sel_a_trak].frame.chunks[i - 1].sample_offset +
          atr[sel_a_trak].frame.chunks[i - 1].sample_per_chunk;
    /* fprintf(stderr, "stscin: entry_count %d, first_chunk %u\n", entry_count,
     */
    /* first_chunk); */
    /* fprintf(stderr, "stscin: entry_count %d, samples_in_chunk %u\n", */
    /* entry_count, samples_in_chunk); */
    /* fprintf(stderr, "stscin: entry_count %d, sample_desc_index %u\n", */
    /* entry_count, sample_desc_index); */
    /* fprintf(stderr, "stscin: entry_count %d, sample_offset %u\n",
     * entry_count, */
    /* atr[sel_a_trak].frame.chunks[i].sample_offset); */
  }
  atr[sel_a_trak].frame.chunks[entry_count].first_chunk = 0xFFFFFFFF;  // dummy
  atr[sel_a_trak].frame.chunks[entry_count].sample_per_chunk = 0;
  atr[sel_a_trak].frame.chunks[entry_count].sample_description_index = -1;

  return size;
}

int mov_read_stsz(mp4r_t *mp4r, int size) {
#if SUPPORT_VERIFIER
  char *atom_d = (char *)malloc(size);
  int fpos;
  fpos = ftell(mp4r->fin);
  avio_rdata(mp4r->fin, atom_d, size);
  fseek(mp4r->fin, fpos, SEEK_SET);
  vlog_atom(MP4BOX_STSZ, atom_d, size, fpos - 8);
  free(atom_d);
#endif

  int cnt;
  uint32_t sample_size;
  uint32_t sample_count;

  int sel_a_trak;
  sel_a_trak = mp4r->sel_a_trak;
  audio_rtr_t *atr = mp4r->a_trak;

  // version/flags
  avio_rb32();
  // Sample size
  sample_size = avio_rb32();
  atr[sel_a_trak].sample_size = sample_size;
  /* atr[sel_a_trak].frame.maxsize = sample_size; */
  // Number of entries
  sample_count = avio_rb32();
  /* fprintf(stderr, "stszin: entry_count %d, sample_size %u\n", sample_count,
   * sample_size); */
  if (!sample_count) return size;
  atr[sel_a_trak].frame.ents = sample_count;

  if (!(atr[sel_a_trak].frame.ents + 1)) {
    return ERR_FAIL;
  }

  atr[sel_a_trak].frame.sizes = (uint32_t *)_dcalloc(
      atr[sel_a_trak].frame.ents + 1, sizeof(*atr[sel_a_trak].frame.sizes),
      __FILE__, __LINE__);
  if (!atr[sel_a_trak].frame.sizes) {
    return ERR_FAIL;
  }

  uint32_t fsize;
  if (sample_size) atr[sel_a_trak].frame.maxsize = sample_size;
  for (cnt = 0; cnt < atr[sel_a_trak].frame.ents; cnt++) {
    if (sample_size == 0) {
      fsize = avio_rb32();
      if (atr[sel_a_trak].frame.maxsize < fsize) {
        mp4r->a_trak[sel_a_trak].bitbuf.data = (uint8_t *)_drealloc(
            mp4r->a_trak[sel_a_trak].bitbuf.data,
            mp4r->a_trak[sel_a_trak].frame.maxsize + 1, __FILE__, __LINE__);
        atr[sel_a_trak].frame.maxsize = fsize;
      }
      atr[sel_a_trak].frame.sizes[cnt] = fsize;
    } else {
      fsize = sample_size;
    }
    atr[sel_a_trak].frame.sizes[cnt] = fsize;
  }

  return size;
}

int mov_read_stco(mp4r_t *mp4r, int size) {
#if SUPPORT_VERIFIER
  char *atom_d = (char *)malloc(size);
  int fpos;
  fpos = ftell(mp4r->fin);
  avio_rdata(mp4r->fin, atom_d, size);
  fseek(mp4r->fin, fpos, SEEK_SET);
  vlog_atom(MP4BOX_STCO, atom_d, size, fpos - 8);
  free(atom_d);
#endif

  uint32_t fofs, x;
  uint32_t cnt;

  int sel_a_trak;
  sel_a_trak = mp4r->sel_a_trak;
  audio_rtr_t *atr = mp4r->a_trak;

  // version/flags
  avio_rb32();
  // Number of entries
  uint32_t entry_count = avio_rb32();
  /* fprintf(stderr, "stcoin: entry_count %d\n", entry_count); */
  if (entry_count < 1) {
    return size;
  }
  // first chunk offset

  atr[sel_a_trak].frame.offs = (uint32_t *)_dcalloc(
      atr[sel_a_trak].frame.ents + 1, sizeof(*atr[sel_a_trak].frame.offs),
      __FILE__, __LINE__);
  if (!atr[sel_a_trak].frame.offs) {
    return ERR_FAIL;
  }

  for (cnt = 0, x = 0; cnt < entry_count; cnt++) {
    // chunk entry loop
    fofs = avio_rb32();
    // fprintf(stderr, "stcoin: entry (%d) offset %d\n", cnt, fofs);
    uint32_t sample_size = 0;
    for (int i = 0; i < atr[sel_a_trak].frame.chunk_count; i++) {
      sample_size = atr[sel_a_trak].frame.chunks[i].sample_per_chunk;
      if (atr[sel_a_trak].frame.chunks[i].first_chunk <= cnt + 1 &&
          cnt + 1 < atr[sel_a_trak].frame.chunks[i + 1].first_chunk) {
        // if first_chunk == 0xffffffff then end of entries...
        break;
      }
    }
    for (int i = 0; i < sample_size; i++, x++) {
      if (i > 0) {
        fofs += atr[sel_a_trak].frame.sizes[x - 1];
      }
      atr[sel_a_trak].frame.offs[x] = fofs;
    }
  }

  return size;
}

#if SUPPORT_VERIFIER
int mov_read_sgpd(mp4r_t *mp4r, int size) {
  char *atom_d = (char *)malloc(size);
  int fpos;
  fpos = ftell(mp4r->fin);
  avio_rdata(mp4r->fin, atom_d, size);
  fseek(mp4r->fin, fpos, SEEK_SET);
  vlog_atom(MP4BOX_SGPD, atom_d, size, fpos - 8);
  free(atom_d);

  return size;
}
#endif

static int mov_read_moof(mp4r_t *mp4r, int size) {
#if SUPPORT_VERIFIER
  char *atom_d = (char *)malloc(size);
  int fpos;
  fpos = ftell(mp4r->fin);
  avio_rdata(mp4r->fin, atom_d, size);
  fseek(mp4r->fin, fpos, SEEK_SET);
  vlog_atom(MP4BOX_MOOF, atom_d, size, fpos - 8);
  free(atom_d);
#endif

  IAMFHeader *header;
  audio_rtr_t *atr = mp4r->a_trak;
  mp4r->moof_position = ftell(mp4r->fin) - 8;

  for (int i = 0; i < mp4r->num_a_trak; ++i) {
    atr[i].frame.ents_offset += atr[i].frame.ents;
    FREE(atr[i].frame.sizes);
    FREE(atr[i].frame.offs);
    atr[i].frame.ents = 0;

    header = (IAMFHeader *)atr[i].csc;
    if (header) {
      ;  //
    }
  }

  /* fprintf(stderr, "moof pos %" PRId64", size %d\n", mp4r->moof_position,
   * size); */
  return size;
}

int mov_read_traf(mp4r_t *mp4r, int size) {
#if SUPPORT_VERIFIER
  char *atom_d = (char *)malloc(size);
  int fpos;
  fpos = ftell(mp4r->fin);
  avio_rdata(mp4r->fin, atom_d, size);
  fseek(mp4r->fin, fpos, SEEK_SET);
  vlog_atom(MP4BOX_TRAF, atom_d, size, fpos - 8);
  free(atom_d);
#endif
  return size;
}

static int mov_read_tfhd(mp4r_t *mp4r, int size) {
#if SUPPORT_VERIFIER
  char *atom_d = (char *)malloc(size);
  int fpos;
  fpos = ftell(mp4r->fin);
  avio_rdata(mp4r->fin, atom_d, size);
  fseek(mp4r->fin, fpos, SEEK_SET);
  vlog_atom(MP4BOX_TFHD, atom_d, size, fpos - 8);
  free(atom_d);
#endif

  int vf = avio_rb32();
  int id = avio_rb32();

  /* fprintf(stderr, "find track id %d.\n", id); */
  for (int i = 0; i < mp4r->num_a_trak; ++i) {
    if (mp4r->a_trak[i].track_id == id) {
      mp4r->sel_a_trak = i;
      /* fprintf(stderr, "track id %d is found.\n", id); */
      break;
    }
  }

  /* base_data_offset */
  if (vf & 0x1) {
    avio_rb32();
    avio_rb32();
  }

  // sample_description_index
  if (vf & 0x2) {
    int sdidx = avio_rb32();
    if (!mp4r->a_trak[mp4r->sel_a_trak].frag_sdidx ||
        mp4r->a_trak[mp4r->sel_a_trak].frag_sdidx == ~sdidx) {
      mp4r->a_trak[mp4r->sel_a_trak].frag_sdidx = sdidx;
    } else if (mp4r->a_trak[mp4r->sel_a_trak].frag_sdidx != sdidx) {
      printf("sample description index from %d change to %d\n",
             mp4r->a_trak[mp4r->sel_a_trak].frag_sdidx, sdidx);
      mp4r->a_trak[mp4r->sel_a_trak].frag_sdidx = ~sdidx;
    }
  }
  // default_sample_duration
  if (vf & 0x8) {
    mp4r->a_trak[mp4r->sel_a_trak].sample_duration = avio_rb32();
  }
  // default_sample_size
  if (vf & 0x10) mp4r->a_trak[mp4r->sel_a_trak].sample_size = avio_rb32();
  // default_sample_flags
  /* if (vf & 0x20) avio_rb32(); */

  return size;
}

static int mov_read_trun(mp4r_t *mp4r, int size) {
#if SUPPORT_VERIFIER
  char *atom_d = (char *)malloc(size);
  int fpos;
  fpos = ftell(mp4r->fin);
  avio_rdata(mp4r->fin, atom_d, size);
  fseek(mp4r->fin, fpos, SEEK_SET);
  vlog_atom(MP4BOX_TRUN, atom_d, size, fpos - 8);
  free(atom_d);
#endif
  int cnt;
  uint32_t vf;
  uint32_t sample_size;
  uint32_t sample_count;
  uint32_t offset = 0;

  int sel_a_trak;
  sel_a_trak = mp4r->sel_a_trak;
  audio_rtr_t *atr = mp4r->a_trak;

  // version/flags
  vf = avio_rb32();
  // Number of entries
  sample_count = avio_rb32();
  //  fprintf(stderr, "trunin: sle_a_trak %d, ents %d, entry_count %d, \n",
  //  sel_a_trak, atr[sel_a_trak].frame.ents, sample_count);
  if (!sample_count) return size;

  atr[sel_a_trak].frame.ents = sample_count;

  if (!(atr[sel_a_trak].frame.ents + 1)) {
    return ERR_FAIL;
  }

  atr[sel_a_trak].frame.sizes = (uint32_t *)_dmalloc(
      sizeof(*atr[sel_a_trak].frame.sizes) * atr[sel_a_trak].frame.ents,
      __FILE__, __LINE__);
  atr[sel_a_trak].frame.offs = (uint32_t *)_dmalloc(
      sizeof(*atr[sel_a_trak].frame.offs) * atr[sel_a_trak].frame.ents,
      __FILE__, __LINE__);

  if (!atr[sel_a_trak].frame.sizes) {
    return ERR_FAIL;
  }

  if (vf & 0x1) offset = avio_rb32();
  /* fprintf(stderr, "offset %u\n", offset); */

  if (vf & 0x04) avio_rb32();

  uint32_t fsize = mp4r->a_trak[mp4r->sel_a_trak].sample_size;
  if (fsize) atr[sel_a_trak].frame.maxsize = fsize;
  offset += mp4r->moof_position;
  for (cnt = 0; cnt < atr[sel_a_trak].frame.ents; cnt++) {
    if (vf & 0x100) avio_rb32();

    if (vf & 0x200) fsize = avio_rb32();
    if (atr[sel_a_trak].frame.maxsize < fsize) {
      atr[sel_a_trak].frame.maxsize = fsize;
    }
    atr[sel_a_trak].frame.sizes[cnt] = fsize;
    atr[sel_a_trak].frame.offs[cnt] = offset;
    // fprintf(stderr, "entry %d : size  %d, offset %u\n", cnt, fsize, offset);
    offset += fsize;

    if (vf & 0x400) avio_rb32();
    if (vf & 0x800) avio_rb32();
  }

  return size;
}

int parse(mp4r_t *mp4r, uint32_t *sizemax) {
  uint64_t apos = 0;
  uint64_t aposmax = ftell(mp4r->fin) + *sizemax;
  uint32_t size;

  if (mp4r->atom->atom_type != MOV_ATOM_NAME) {
    if (mp4r->logger) {
      fprintf(mp4r->logger, "parse error: root is not a 'name' atom_type\n");
    }
    return ERR_FAIL;
  }
  /* fprintf(stderr, "looking for '%s', size %u\n", (char *)mp4r->atom->opaque,
   */
  /* *sizemax); */

  // search for atom in the file
  while (1) {
    char name[4];
    uint32_t tmp;

    apos = ftell(mp4r->fin);
    if (apos >= (aposmax - 8)) {
      /* fprintf(stderr, "parse error: atom '%s' not found, at %" PRIu64"\n", */
      /* (char *)mp4r->atom->opaque, apos); */
      return ERR_FAIL;
    }
    if ((tmp = avio_rb32()) < 8) {
      fprintf(stderr, "invalid atom size %x @%lx\n", tmp, ftell(mp4r->fin));
      return ERR_FAIL;
    }

    size = tmp;
    if (avio_rdata(mp4r->fin, name, 4) != 4) {
      /* EOF */
      //      fprintf(stderr, "can't read atom name @%lx\n", ftell(mp4r->fin));
      return ERR_FAIL;
    }

    //    fprintf(stderr, "atom: '%.4s'(%x)\n", name, size);

    if (!memcmp(name, mp4r->atom->opaque, 4)) {
      // fprintf(stderr, "OK\n");
#if 0
      atom_dump(mp4r->fin, apos, tmp);
#endif
      fseek(mp4r->fin, apos + 8, SEEK_SET);
      break;
    }
    // fprintf(stderr, "\n");

    fseek(mp4r->fin, apos + size, SEEK_SET);
  }
  *sizemax = size;
  mp4r->atom++;
  /* fprintf(stderr, "parse: pos %" PRId64"\n", apos); */
  if (mp4r->atom->atom_type == MOV_ATOM_DATA) {
    int err = ((int (*)(mp4r_t *, int))mp4r->atom->opaque)(mp4r, size - 8);
    if (err < ERR_OK) {
      fseek(mp4r->fin, apos + size, SEEK_SET);
      return err;
    }
    mp4r->atom++;
  }
  if (mp4r->atom->atom_type == MOV_ATOM_DESCENT) {
    // apos = ftell(mp4r->fin);

    // fprintf(stderr, "descent\n");
    mp4r->atom++;
    while (mp4r->atom->atom_type != MOV_ATOM_STOP) {
      uint32_t subsize = size - 8;
      int ret;
      if (mp4r->atom->atom_type == MOV_ATOM_ASCENT) {
        mp4r->atom++;
        break;
      }
      // fseek(mp4r->fin, apos, SEEK_SET);
      if ((ret = parse(mp4r, &subsize)) < 0) {
        return ret;
      }
    }
    // fprintf(stderr, "ascent\n");
  }

  /* fprintf(stderr, "parse: pos+size %" PRId64"\n", apos + size); */
  fseek(mp4r->fin, apos + size, SEEK_SET);

  return ERR_OK;
}

static int mov_moov_probe(mp4r_t *mp4r, int sizemax);
static int mov_moof_probe(mp4r_t *mp4r, int sizemax);
static int mov_read_moov(mp4r_t *mp4r, int sizemax);

static avio_context g_head[] = {
    {MOV_ATOM_NAME, "ftyp"}, {MOV_ATOM_DATA, mov_read_ftyp}, {0}};

static avio_context g_moov[] = {
    {MOV_ATOM_NAME, "moov"}, {MOV_ATOM_DATA, mov_read_moov}, {0}};

static avio_context moov_probe[] = {
    {MOV_ATOM_NAME, "moov"}, {MOV_ATOM_DATA, mov_moov_probe}, {0}};

static avio_context moof_probe[] = {
    {MOV_ATOM_NAME, "moof"}, {MOV_ATOM_DATA, mov_moof_probe}, {0}};

static avio_context g_moof[] = {{MOV_ATOM_NAME, "moof"},
                                {MOV_ATOM_DATA, mov_read_moof},
                                {MOV_ATOM_DESCENT},
                                {MOV_ATOM_NAME, "traf"},
                                {MOV_ATOM_DATA, mov_read_traf},
                                {MOV_ATOM_DESCENT},
                                {MOV_ATOM_NAME, "tfhd"},
                                {MOV_ATOM_DATA, mov_read_tfhd},
                                {MOV_ATOM_NAME, "trun"},
                                {MOV_ATOM_DATA, mov_read_trun},
                                {MOV_ATOM_ASCENT},
                                {MOV_ATOM_ASCENT},
                                {0}};

static avio_context g_mvhd[] = {
    {MOV_ATOM_NAME, "mvhd"}, {MOV_ATOM_DATA, mov_read_mvhd}, {0}};

static avio_context g_trak[] = {
    {MOV_ATOM_NAME, "trak"}, {MOV_ATOM_DATA, mov_read_trak}, {0}};

int mov_read_hdlr(mp4r_t *mp4r, int size) {
#if SUPPORT_VERIFIER
  char *atom_d = (char *)malloc(size);
  int fpos;
  fpos = ftell(mp4r->fin);
  avio_rdata(mp4r->fin, atom_d, size);
  fseek(mp4r->fin, fpos, SEEK_SET);
  vlog_atom(MP4BOX_HDLR, atom_d, size, fpos - 8);
  free(atom_d);
#endif

  uint8_t buf[5];

  buf[4] = 0;
  // version/flags
  avio_rb32();
  // pre_defined
  avio_rb32();
  // Component subtype
  avio_rdata(mp4r->fin, buf, 4);
  if (mp4r->logger) {
    fprintf(mp4r->logger, "*track media type: '%s': ", buf);
  }
  /* fprintf(stderr, "*track media type: '%s'\n", buf); */

  if (!memcmp("vide", buf, 4)) {
    fprintf(stderr, "unsupported, skipping in hdlrin\n");
  } else if (!memcmp("soun", buf, 4)) {
    // fprintf(stderr, "sound\n");
    mp4r->atom = atoms_stbl;
  }
  // reserved
  avio_rb32();
  avio_rb32();
  avio_rb32();
  // name
  // null terminate
  avio_r8();

  return size;
}

int mov_moov_probe(mp4r_t *mp4r, int sizemax) {
  mp4r->next_moov = ftell(mp4r->fin);
  /* fprintf(stderr, "find moov box at %" PRId64".\n", mp4r->next_moov); */
  return ERR_OK;
}

int mov_moof_probe(mp4r_t *mp4r, int sizemax) {
  mp4r->next_moof = ftell(mp4r->fin);
  /* fprintf(stderr, "find moof box at %" PRId64".\n", mp4r->next_moof); */
  return ERR_OK;
}

int mov_read_moov(mp4r_t *mp4r, int sizemax) {
  uint64_t apos = ftell(mp4r->fin);
  uint32_t atomsize;
  avio_context *old_atom = mp4r->atom;
  int err, ret = sizemax;
  int ntrack = 0;

#if SUPPORT_VERIFIER
  char *atom_d = (char *)malloc(sizemax);
  int fpos;
  fpos = ftell(mp4r->fin);
  avio_rdata(mp4r->fin, atom_d, sizemax);
  fseek(mp4r->fin, fpos, SEEK_SET);
  vlog_atom(MP4BOX_MOOV, atom_d, sizemax, fpos - 8);
  free(atom_d);
#endif

  mp4r->atom = g_mvhd;
  atomsize = sizemax + apos - ftell(mp4r->fin);
  if (parse(mp4r, &atomsize) < 0) {
    mp4r->atom = old_atom;
    return ERR_FAIL;
  }

  while (1) {
    mp4r->atom = g_trak;
    atomsize = sizemax + apos - ftell(mp4r->fin);
    if (atomsize < 8) {
      break;
    }
    err = parse(mp4r, &atomsize);
    if (err >= 0) {
      if (mp4r->tot_track_scan < mp4r->next_track_id - 1) {
        continue;
      }
    }
    if (err != ERR_UNSUPPORTED) {
      ret = err;
      break;
    }
    // fprintf(stderr, "UNSUPP\n");
  }

  mp4r->atom = old_atom;
  return ret;
}

static int mp4demux_clean_tracks(mp4r_t *mp4r);
int mp4demux_audio(mp4r_t *mp4r, int trakn, int *delta) {
  audio_rtr_t *atr = mp4r->a_trak;
  int ret = 0, atomsize;
  int idx = atr[trakn].frame.current - atr[trakn].frame.ents_offset;

  if (idx > atr[trakn].frame.ents) {
    return ERR_FAIL;
  }

  if (atr[trakn].frame.sizes) {
    // VBR
    atr[trakn].bitbuf.size = atr[trakn].frame.sizes[idx];
  } else if (atr[trakn].sample_size != 0) {
    // CBR.. stsz
    atr[trakn].bitbuf.size = atr[trakn].sample_size;
  }

  mp4demux_seek(mp4r, trakn, atr[trakn].frame.current);

  if (fread(atr[trakn].bitbuf.data, 1, atr[trakn].bitbuf.size, mp4r->fin) !=
      atr[trakn].bitbuf.size) {
    fprintf(stderr, "can't read frame data(frame %d@0x%x)\n",
            atr[trakn].frame.current,
            atr[trakn].frame.sizes ? atr[trakn].frame.sizes[idx] : 0);
    return ERR_FAIL;
  }

  if (delta) {
    if (atr[trakn].frame.deltas)
      *delta = atr[trakn].frame.deltas[idx];
    else
      *delta = atr[trakn].sample_duration;
  }

  atr[trakn].frame.current++;
  return ERR_OK;
}

int mp4demux_parse(mp4r_t *mp4r, int trak) {
  if (mp4r->moof_flag) {
    int atomsize = INT_MAX;
    int ret;
    uint64_t pos = ftell(mp4r->fin);
    uint64_t next_moov = 0;
    uint64_t size;

    fseek(mp4r->fin, 0, SEEK_END);
    size = ftell(mp4r->fin);
    if (pos == size) return ERR_FAIL;

    fprintf(stderr, "Warning: pos %" PRIu64 ", file size %" PRIu64 "\n", pos,
            size);
    fseek(mp4r->fin, pos, SEEK_SET);

#if 0
    mp4r->atom = moov_probe;
    if (parse(mp4r, &atomsize) < 0) {
      mp4r->next_moov = (uint64_t)-1;
    }
    fseek(mp4r->fin, pos, SEEK_SET);
#endif

    atomsize = INT_MAX;
    mp4r->atom = moof_probe;
    if (parse(mp4r, &atomsize) < 0) {
      return ERR_FAIL;
    }
    fseek(mp4r->fin, pos, SEEK_SET);

    if (mp4r->next_moov > 0 && mp4r->next_moov < mp4r->next_moof) {
      fprintf(stderr, "* Find new moov box.\n");
      mp4r->atom = g_moov;
      mp4demux_clean_tracks(mp4r);

      atomsize = INT_MAX;
      if ((ret = parse(mp4r, &atomsize)) < 0) {
        fprintf(stderr, "parse:%d\n", ret);
      }
    }
    mp4r->atom = g_moof;
    atomsize = INT_MAX;
    if ((ret = parse(mp4r, &atomsize)) < 0) {
      fprintf(stderr, "parse:%d\n", ret);
    }
    /* printf("moof size %d\n", atomsize); */

    if (mp4r->next_moov > 0 && mp4r->next_moov < mp4r->next_moof) {
      for (int i = 0; i < mp4r->num_a_trak; i++) {
        /* fprintf(stderr, "* Trak %d : allocate buffer size %d.\n", i, */
        /* mp4r->a_trak[i].frame.maxsize); */
        mp4r->a_trak[i].bitbuf.data = (uint8_t *)_drealloc(
            mp4r->a_trak[i].bitbuf.data, mp4r->a_trak[i].frame.maxsize + 1,
            __FILE__, __LINE__);
        if (!mp4r->a_trak[i].bitbuf.data) {
          return ERR_FAIL;
        }
      }
    }

    if (atomsize == INT_MAX) return ERR_FAIL;
    /* fseek(mp4r->fin, mp4r->moof_position + atomsize, SEEK_SET); */
    return ERR_OK;
  }
  return ERR_UNSUPPORTED;
}

int mp4demux_seek(mp4r_t *mp4r, int trakn, int framenum) {
  audio_rtr_t *atr = mp4r->a_trak;
  int idx = framenum - atr[trakn].frame.ents_offset;

  if (idx > atr[trakn].frame.ents) {
    return ERR_FAIL;
  }
  if (atr[trakn].frame.offs) {
    fseek(mp4r->fin, atr[trakn].frame.offs[idx], SEEK_SET);
  }
  atr[trakn].frame.current = framenum;

  return ERR_OK;
}

#if 0
void mp4demux_info(mp4r_t *mp4r, int trakn, int print) {
  audio_rtr_t *atr = mp4r->a_trak;
  fprintf(stderr, "Modification Time:\t\t%s\n", mp4time(mp4r->mtime));
  if (trakn < mp4r->num_a_trak) {
    int s = (trakn < 0) ? 0 : trakn;
    int e = (trakn < 0) ? mp4r->num_a_trak - 1 : trakn;
    for (int i = s; i <= e; i++) {
      if (print) {
        fprintf(stderr, "Audio track#%d -------------\n", i);
        fprintf(stderr, "Samplerate:\t\t%u\n", atr[i].samplerate);
        fprintf(stderr, "Total samples:\t\t%d\n", atr[i].samples);
        fprintf(stderr, "Total channels:\t\t%d\n", atr[i].channels);
        fprintf(stderr, "Bits per sample:\t%d\n", atr[i].bits);
        fprintf(stderr, "Frames:\t\t\t%d\n", atr[i].frame.ents);
      }
    }
  }
}
#endif

#define FREE_FUN(x, f, c) \
  if (x) {                \
    f(x, c);              \
    x = 0;                \
  }
int mp4demux_clean_tracks(mp4r_t *mp4r) {
  if (mp4r) {
    audio_rtr_t *atr = mp4r->a_trak;
    for (int i = 0; i < mp4r->num_a_trak; i++) {
      FREE(atr[i].frame.chunks);
      FREE(atr[i].frame.offs);
      FREE(atr[i].frame.sizes);
      FREE(atr[i].frame.deltas);
      FREE(atr[i].bitbuf.data);
      FREE_FUN(atr[i].csc, iamf_header_free, atr[i].csc_count);
    }
    FREE(mp4r->a_trak);
    mp4r->num_a_trak = 0;
  }
  return ERR_OK;
}

int mp4demux_close(mp4r_t *mp4r) {
  if (mp4r) {
    mp4demux_clean_tracks(mp4r);
    if (mp4r->fin) {
      fclose(mp4r->fin);
    }
    mp4r->fin = NULL;

    for (int i = 0; i < mp4r->tag.extnum; i++) {
      free((void *)mp4r->tag.ext[i].name);
      free((void *)mp4r->tag.ext[i].data);
    }
    mp4r->tag.extnum = 0;
    FREE(mp4r);
  }
  return ERR_OK;
}

mp4r_t *mp4demux_open(const char *name, FILE *logger) {
  mp4r_t *mp4r = NULL;
  FILE *fin;
  uint32_t atomsize;
  int ret;

  fin = fopen(name, "rb");
  if (!fin) {
    goto err;
  }

  mp4r = (mp4r_t *)_dcalloc(1, sizeof(mp4r_t), __FILE__, __LINE__);
  if (mp4r == NULL) {
    goto err;
  }
  mp4r->fin = fin;
  mp4r->logger = logger;

  if (mp4r->logger) {
    fprintf(mp4r->logger, "**** MP4 header ****\n");
  }
  mp4r->atom = g_head;  // ftyp
  atomsize = INT_MAX;
  if (parse(mp4r, &atomsize) < 0) {
    goto err;
  }

  //////////////////////////
  mp4r->atom = g_moov;
  atomsize = INT_MAX;
  rewind(mp4r->fin);
  if ((ret = parse(mp4r, &atomsize)) < 0) {
    fprintf(stderr, "parse:%d\n", ret);
    if (atomsize == INT_MAX) goto err;
  }

  /* fprintf(stderr, "parse end.\n"); */
  // alloc frame buffer
  for (int i = 0; i < mp4r->num_a_trak; i++) {
    fprintf(stderr, "audio track %d, sample count %d.\n", i,
            mp4r->a_trak[i].frame.ents);
    if (!mp4r->a_trak[i].frame.ents) {
      mp4r->moof_flag = 1;
    }
  }

  if (mp4r->moof_flag) {
    mp4r->atom = g_moof;
    atomsize = INT_MAX;
    if ((ret = parse(mp4r, &atomsize)) < 0) {
      fprintf(stderr, "parse:%d\n", ret);
    }
  }

  for (int i = 0; i < mp4r->num_a_trak; i++) {
    /* fprintf(stderr, "trak %d : allocate buffer size %d.\n", i, */
    /* mp4r->a_trak[i].frame.maxsize); */
    mp4r->a_trak[i].bitbuf.data = (uint8_t *)_drealloc(
        mp4r->a_trak[i].bitbuf.data,
        mp4r->a_trak[i].frame.maxsize + 1 + mp4r->a_trak[i].csc_maxsize,
        __FILE__, __LINE__);
    if (!mp4r->a_trak[i].bitbuf.data) {
      goto err;
    }
  }

  //////////////////////////

  // mp4demux_info(mp4r, -1, 1);

  return mp4r;

err:
  if (mp4r != NULL) {
    mp4demux_close(mp4r);
  }
  return NULL;
}
