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
 * @file flac_multistream_decoder.c
 * @brief FLAC decoder.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#include "flac_multistream_decoder.h"

#include "FLAC/stream_decoder.h"
#include <stdlib.h>
#include <string.h>

#include "IAMF_debug.h"
#include "IAMF_defines.h"
#include "IAMF_types.h"
#include "IAMF_utils.h"
#include "bitstream.h"

#ifdef IA_TAG
#undef IA_TAG
#endif

#define IA_TAG "FLACMS"

#define MAX_BUFFER_SIZE MAX_FLAC_FRAME_SIZE * 2

typedef struct FLACDecoderHandle {
  FLAC__StreamDecoder *dec;
  struct {
    uint32_t depth;
    uint32_t channels;
    uint32_t sample_rate;
  } stream_info;
  uint8_t *packet;
  uint32_t packet_size;
  uint32_t fs;
  int buffer[MAX_FLAC_FRAME_SIZE];
} FLACDecoderHandle;

typedef struct FLACMSDecoder {
  uint32_t flags;
  int streams;
  int coupled_streams;

  FLACDecoderHandle *handles;
} FLACMSDecoder;

static FLAC__StreamDecoderReadStatus flac_stream_read(
    const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes,
    void *client_data) {
  FLACDecoderHandle *handle = (FLACDecoderHandle *)client_data;
  if (handle->packet) {
    ia_logt("read stream %d data", handle->packet_size);
    memcpy(buffer, handle->packet, handle->packet_size);
    *bytes = handle->packet_size;
    handle->packet = 0;
    return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
  } else {
    ia_loge("The data is incomplete.");
  }

  return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
}

static FLAC__StreamDecoderWriteStatus flac_stream_write(
    const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame,
    const FLAC__int32 *const buffer[], void *client_data) {
  FLACDecoderHandle *handle = (FLACDecoderHandle *)client_data;
  char *out = (char *)handle->buffer;
  int fss = 0;

  handle->fs = frame->header.blocksize;
  fss = handle->fs * sizeof(FLAC__int32);

  for (int i = 0; i < handle->stream_info.channels; ++i) {
    memcpy(out, (void *)buffer[i], fss);
    out += fss;
  }

  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

static void flac_stream_metadata(const FLAC__StreamDecoder *decoder,
                                 const FLAC__StreamMetadata *metadata,
                                 void *client_data) {
  if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
    FLACDecoderHandle *handle = (FLACDecoderHandle *)client_data;
    handle->stream_info.depth = metadata->data.stream_info.bits_per_sample;
    handle->stream_info.channels = metadata->data.stream_info.channels;
    handle->stream_info.sample_rate = metadata->data.stream_info.sample_rate;
    ia_logi("depth %d, channels %d, sample_rate %d.", handle->stream_info.depth,
            handle->stream_info.channels, handle->stream_info.sample_rate);
  }
}

static void flac_stream_error(const FLAC__StreamDecoder *decoder,
                              FLAC__StreamDecoderErrorStatus status,
                              void *client_data) {}

static int flac_header_set_channels(uint8_t *h, uint32_t size, int n) {
  int last, type, s, offset;
  uint8_t byte;

  offset = 4;
  while (1) {
    last = h[offset] >> 7 & 0x01;
    type = h[offset] & 0x7f;
    ++offset;
    s = readu24be(h, offset);
    offset += 3;

    if (!type) {
      offset += 12;
      byte = ~(0x7 << 1) & h[offset];
      byte = ((n - 1) & 0x7) << 1 | byte;
      ia_logt("%d byte 1 bit: 0x%x->0x%x", offset, h[offset], byte);
      if (offset < size) h[offset] = byte;
    } else
      offset += s;

    if (last || offset >= size) break;
  }

  if (offset >= size) return IAMF_ERR_BAD_ARG;
  return IAMF_OK;
}

static int flac_multistream_decode_native(FLACMSDecoder *st, uint8_t *buffer[],
                                          uint32_t size[], void *pcm,
                                          int frame_size) {
  FLACDecoderHandle *handle = NULL;
  char *out = (char *)pcm;
  int ss = 0;

  for (int i = 0; i < st->streams; ++i) {
    ia_logt("stream %d", i);
    handle = &st->handles[i];
    handle->packet = buffer[i];
    handle->packet_size = size[i];

    if (!FLAC__stream_decoder_process_single(handle->dec)) {
      return IAMF_ERR_INTERNAL;
    }

    if (handle->fs != frame_size)
      ia_logw("Different frame size %d vs %d", frame_size, handle->fs);

    ss = handle->fs * handle->stream_info.channels * 4;
    memcpy(out, handle->buffer, ss);
    out += ss;
  }
  if (!handle) {
    return IAMF_ERR_BAD_ARG;
  } else {
    return handle->fs;
  }
}

FLACMSDecoder *flac_multistream_decoder_open(uint8_t *config, uint32_t size,
                                             int streams, int coupled_streams,
                                             uint32_t flags, int *error) {
  FLACMSDecoder *st = 0;
  FLACDecoderHandle *handles = 0;
  FLACDecoderHandle *handle;
  FLAC__StreamDecoderInitStatus status;
  uint8_t *header[2];
  int i, ret = 0;
  char magic[] = {'f', 'L', 'a', 'C'};

  if (!config || !size) {
    if (error) *error = IAMF_ERR_BAD_ARG;
    return 0;
  }

  st = IAMF_MALLOCZ(FLACMSDecoder, 1);
  handles = IAMF_MALLOCZ(FLACDecoderHandle, streams);
  for (int i = 0; i < 2; ++i) header[i] = IAMF_MALLOCZ(uint8_t, size + 4);

  if (!st || !handles || !header[0] || !header[1]) {
    if (error) {
      *error = IAMF_ERR_ALLOC_FAIL;
    }
    if (st) {
      flac_multistream_decoder_close(st);
    }
    IAMF_FREE(handles);
    for (int i = 0; i < 2; ++i) IAMF_FREE(header[i]);

    return 0;
  }

  for (int i = 0; i < 2; ++i) {
    memcpy(header[i], magic, 4);
    memcpy(header[i] + 4, config, size);
  }

  flac_header_set_channels(header[1], size + 4, 1);
  st->flags = flags;
  st->streams = streams;
  st->coupled_streams = coupled_streams;
  st->handles = handles;

  for (i = 0; i < st->streams; ++i) {
    handle = &st->handles[i];
    if (i < coupled_streams)
      handle->packet = header[0];
    else
      handle->packet = header[1];
    handle->packet_size = size + 4;

    handle->dec = FLAC__stream_decoder_new();
    if (!handle->dec) {
      ret = IAMF_ERR_INVALID_STATE;
      break;
    }

    FLAC__stream_decoder_set_md5_checking(handle->dec, false);

    status = FLAC__stream_decoder_init_stream(
        handle->dec, flac_stream_read, NULL, NULL, NULL, NULL,
        flac_stream_write, flac_stream_metadata, flac_stream_error, handle);

    if (status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
      ia_loge("flac decoder init errno %d (%s)", -status,
              FLAC__StreamDecoderInitStatusString[status]);
      ret = IAMF_ERR_INTERNAL;
      break;
    }

    if (!FLAC__stream_decoder_process_until_end_of_metadata(handle->dec)) {
      ia_loge("Failed to read meta.");
      ret = IAMF_ERR_INTERNAL;
      break;
    }
  }

  if (ret < 0) {
    if (error) {
      *error = ret;
    }
    flac_multistream_decoder_close(st);
    st = 0;
  }

  for (int i = 0; i < 2; ++i) IAMF_FREE(header[i]);

  return st;
}

int flac_multistream_decode(FLACMSDecoder *st, uint8_t *buffer[],
                            uint32_t size[], void *pcm, uint32_t frame_size) {
  if (st->flags & AUDIO_FRAME_PLANE)
    return flac_multistream_decode_native(st, buffer, size, pcm, frame_size);
  else {
    return IAMF_ERR_UNIMPLEMENTED;
  }
}

void flac_multistream_decoder_close(FLACMSDecoder *st) {
  if (st) {
    if (st->handles) {
      for (int i = 0; i < st->streams; ++i) {
        if (st->handles[i].dec) {
          FLAC__stream_decoder_flush(st->handles[i].dec);
          FLAC__stream_decoder_delete(st->handles[i].dec);
        }
      }
      free(st->handles);
    }
    free(st);
  }
}

int flac_multistream_decoder_get_sample_bits(FLACMSDecoder *st) {
  if (st && st->handles) return st->handles[0].stream_info.depth;
  return IAMF_ERR_INTERNAL;
}
