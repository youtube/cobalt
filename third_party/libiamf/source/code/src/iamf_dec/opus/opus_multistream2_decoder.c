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
 * @file opus_multistream2_decoder.c
 * @brief opus decoder.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#include "opus/opus_multistream2_decoder.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "IAMF_debug.h"
#include "IAMF_defines.h"
#include "IAMF_types.h"
#include "opus/opus.h"

#ifdef IA_TAG
#undef IA_TAG
#endif

#define IA_TAG "OPUSMS2"

typedef void (*opus_copy_channels_out_func)(void *dst, const short *src,
                                            int frame_size, int channels);

struct OpusMS2Decoder {
  uint32_t flags;

  int streams;
  int coupled_streams;

  void *buffer;
};

static inline int align(int i) {
  struct foo {
    char c;
    union {
      void *p;
      opus_int32 i;
      float v;
    } u;
  };
  unsigned int alignment = offsetof(struct foo, u);
  return ((i + alignment - 1) / alignment) * alignment;
}

static opus_int32 opus_multistream2_decoder_get_size(int nb_streams,
                                                     int nb_coupled_streams) {
  int coupled_size;
  int mono_size;

  coupled_size = opus_decoder_get_size(2);
  mono_size = opus_decoder_get_size(1);
  return align(sizeof(OpusMS2Decoder)) +
         nb_coupled_streams * align(coupled_size) +
         (nb_streams - nb_coupled_streams) * align(mono_size);
}

static int opus_multistream2_decoder_init(OpusMS2Decoder *st, opus_int32 Fs,
                                          int streams, int coupled_streams,
                                          uint32_t flags) {
  int coupled_size;
  int mono_size;
  int i, ret;
  char *ptr;

  st->streams = streams;
  st->coupled_streams = coupled_streams;
  st->flags = flags;

  ptr = (char *)st + align(sizeof(OpusMS2Decoder));
  coupled_size = opus_decoder_get_size(2);
  mono_size = opus_decoder_get_size(1);

  for (i = 0; i < st->coupled_streams; i++) {
    ret = opus_decoder_init((OpusDecoder *)ptr, Fs, 2);
    if (ret != IAMF_OK) {
      return ret;
    }
    ptr += align(coupled_size);
  }
  for (; i < st->streams; i++) {
    ret = opus_decoder_init((OpusDecoder *)ptr, Fs, 1);
    if (ret != IAMF_OK) {
      return ret;
    }
    ptr += align(mono_size);
  }

  st->buffer = calloc(1, MAX_OPUS_FRAME_SIZE * 2 * sizeof(short));
  if (!st->buffer) return IAMF_ERR_ALLOC_FAIL;
  return IAMF_OK;
}

static int opus_multistream2_decoder_decode_native(
    OpusMS2Decoder *st, uint8_t *buffer[], opus_int32 size[], void *pcm,
    int frame_size, opus_copy_channels_out_func copy_channel_out) {
  int coupled_size;
  int mono_size;
  int s, ret = 0;
  char *ptr;
  char *p = (char *)pcm;

  if (frame_size <= 0) {
    return IAMF_ERR_BAD_ARG;
  }

  ptr = (char *)st + align(sizeof(OpusMS2Decoder));
  coupled_size = opus_decoder_get_size(2);
  mono_size = opus_decoder_get_size(1);

  for (s = 0; s < st->streams; s++) {
    OpusDecoder *dec;

    dec = (OpusDecoder *)ptr;
    ptr += (s < st->coupled_streams) ? align(coupled_size) : align(mono_size);

    ret = opus_decode(dec, buffer[s], size[s], st->buffer, frame_size, 0);

    ia_logt("stream %d decoded result %d", s, ret);
    if (ret <= 0) {
      return ret;
    }
    frame_size = ret;
    if (s < st->coupled_streams) {
      (*copy_channel_out)((void *)p, st->buffer, ret, 2);
      p += (4 * ret);
    } else {
      (*copy_channel_out)((void *)p, st->buffer, ret, 1);
      p += (2 * ret);
    }
  }

  return ret;
}

void opus_copy_channel_out_short_plane(void *dst, const short *src,
                                       int frame_size, int channels) {
  ia_logt("copy frame %d, channels %d  dst %p src %p.", frame_size, channels,
          dst, src);
  if (channels == 1) {
    memcpy(dst, src, sizeof(short) * frame_size);
  } else if (channels == 2) {
    short *pcm = (short *)dst;
    for (int s = 0; s < frame_size; ++s) {
      pcm[s] = src[channels * s];
      pcm[s + frame_size] = src[channels * s + 1];
    }
  }
}

OpusMS2Decoder *opus_multistream2_decoder_create(int Fs, int streams,
                                                 int coupled_streams,
                                                 uint32_t flags, int *error) {
  int ret;
  OpusMS2Decoder *st;
  int size;
  if ((coupled_streams > streams) || (streams < 1) || (coupled_streams < 0) ||
      (streams > 255 - coupled_streams)) {
    if (error) {
      *error = IAMF_ERR_BAD_ARG;
    }
    return NULL;
  }

  size = opus_multistream2_decoder_get_size(streams, coupled_streams);
  st = (OpusMS2Decoder *)malloc(size);
  if (st == NULL) {
    if (error) {
      *error = IAMF_ERR_ALLOC_FAIL;
    }
    return NULL;
  }
  memset(st, 0, size);

  ret = opus_multistream2_decoder_init(st, Fs, streams, coupled_streams, flags);
  if (error) {
    *error = ret;
  }
  if (ret != IAMF_OK) {
    free(st);
    st = NULL;
  }
  return st;
}

int opus_multistream2_decode(OpusMS2Decoder *st, uint8_t *buffer[],
                             uint32_t size[], void *pcm, uint32_t frame_size) {
  if (st->flags & AUDIO_FRAME_PLANE)
    return opus_multistream2_decoder_decode_native(
        st, buffer, (opus_int32 *)size, pcm, frame_size,
        opus_copy_channel_out_short_plane);
  ia_logw("flag is 0x%x, is not implmented.", st->flags);
  return IAMF_ERR_UNIMPLEMENTED;
}

void opus_multistream2_decoder_destroy(OpusMS2Decoder *st) {
  if (st->buffer) {
    free(st->buffer);
  }
  free(st);
}
