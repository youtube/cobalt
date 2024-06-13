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
 * @file aac_multistream_decoder.c
 * @brief AAC decoder.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#include "aac_multistream_decoder.h"

#include "fdk-aac/aacdecoder_lib.h"
#include <stdlib.h>
#include <string.h>

#include "IAMF_debug.h"
#include "IAMF_defines.h"
#include "IAMF_types.h"
#include "IAMF_utils.h"

#ifdef IA_TAG
#undef IA_TAG
#endif

#define IA_TAG "AACMS"

#define MAX_BUFFER_SIZE MAX_AAC_FRAME_SIZE * 2

struct AACMSDecoder {
  uint32_t flags;
  int streams;
  int coupled_streams;
  int delay;

  HANDLE_AACDECODER *handles;
  INT_PCM buffer[MAX_BUFFER_SIZE];
};

typedef void (*aac_copy_channel_out_func)(void *dst, const void *src,
                                          int frame_size, int channes);

void aac_copy_channel_out_short_plane(void *dst, const void *src,
                                      int frame_size, int channels) {
  if (channels == 1) {
    memcpy(dst, src, sizeof(INT_PCM) * frame_size);
  } else if (channels == 2) {
    INT_PCM *in, *out;
    in = (INT_PCM *)src;
    out = (INT_PCM *)dst;
    for (int s = 0; s < frame_size; ++s) {
      out[s] = in[channels * s];
      out[s + frame_size] = in[channels * s + 1];
    }
  }
}

static int aac_config_set_channels(uint8_t *conf, uint32_t size, int channels) {
  int nbytes = 0;
  uint8_t byte = 0;

  // audioObjectType = 2 (5bits)
  // samplingFrequencyIndex (4 or 28 bits)
  // channelConfiguration (4 bits)
  byte = conf[nbytes] & 0x7;
  byte = byte << 1 | conf[++nbytes] >> 7;
  if (byte == 0xf) nbytes += 3;
  byte = ~(0xf << 3) & conf[nbytes];
  byte = (channels & 0xf) << 3 | byte;
  ia_logd("%d byte 1 bit: 0x%x->0x%x", nbytes, conf[nbytes], byte);
  if (nbytes < size)
    conf[nbytes] = byte;
  else
    return IAMF_ERR_BAD_ARG;

  return IAMF_OK;
}

static int aac_multistream_decode_native(
    AACMSDecoder *st, uint8_t *buffer[], uint32_t size[], void *pcm,
    int frame_size, aac_copy_channel_out_func copy_channel_out) {
  UINT valid;
  AAC_DECODER_ERROR err;
  INT_PCM *out = (INT_PCM *)pcm;
  int fs = 0;
  UINT flags = 0;
  CStreamInfo *info = 0;

  for (int i = 0; i < st->streams; ++i) {
    ia_logt("stream %d", i);
    valid = size[i];
    flags = buffer[i] ? 0 : AACDEC_FLUSH;
    if (!flags) {
      err = aacDecoder_Fill(st->handles[i], &buffer[i], &size[i], &valid);
      if (err != AAC_DEC_OK) {
        return IAMF_ERR_INVALID_PACKET;
      }
    }
    err = aacDecoder_DecodeFrame(st->handles[i], st->buffer, MAX_BUFFER_SIZE,
                                 flags);
    if (err == AAC_DEC_NOT_ENOUGH_BITS) {
      return IAMF_ERR_BUFFER_TOO_SMALL;
    }
    if (err != AAC_DEC_OK) {
      ia_loge("stream %d : fail to decode", i);
      return IAMF_ERR_INTERNAL;
    }

    info = aacDecoder_GetStreamInfo(st->handles[i]);
    if (info) {
      fs = info->frameSize;
      if (fs > MAX_AAC_FRAME_SIZE) {
        ia_logw("frame size %d , is larger than %u", fs, MAX_AAC_FRAME_SIZE);
        fs = MAX_AAC_FRAME_SIZE;
      }

      ia_logt("aac decoder %d:", i);
      ia_logt(" > sampleRate : %d.", info->sampleRate);
      ia_logt(" > frameSize  : %d.", info->frameSize);
      ia_logt(" > numChannels: %d.", info->numChannels);
      ia_logt(" > outputDelay: %u.", info->outputDelay);

    } else {
      fs = frame_size;
    }

    if (info) {
      (*copy_channel_out)(out, st->buffer, fs, info->numChannels);
      out += (fs * info->numChannels);
    } else {
      ia_logw("Can not get stream info.");
    }
  }

  if (info && st->delay < 0) st->delay = info->outputDelay;

  return fs;
}

AACMSDecoder *aac_multistream_decoder_open(uint8_t *config, uint32_t size,
                                           int streams, int coupled_streams,
                                           uint32_t flags, int *error) {
  AACMSDecoder *st = 0;
  HANDLE_AACDECODER *handles = 0;
  HANDLE_AACDECODER handle;

  UCHAR *conf[2] = {0, 0};
  UINT clen;

  int i, ret = 0;

  if (!config || !size || streams < 1) {
    ret = IAMF_ERR_BAD_ARG;
    goto end;
  }

  conf[0] = config;
  clen = size;
  conf[1] = IAMF_MALLOC(UCHAR, clen);
  if (!conf[1]) {
    ret = IAMF_ERR_ALLOC_FAIL;
    goto end;
  }

  st = IAMF_MALLOCZ(AACMSDecoder, 1);
  handles = IAMF_MALLOCZ(HANDLE_AACDECODER, streams);

  if (!st || !handles) {
    ret = IAMF_ERR_ALLOC_FAIL;
    IAMF_FREE(handles);
    goto end;
  }

  st->flags = flags;
  st->streams = streams;
  st->coupled_streams = coupled_streams;
  st->delay = -1;
  st->handles = handles;
  memcpy(conf[1], conf[0], clen);
  aac_config_set_channels(conf[1], clen, 1);

  for (i = 0; i < st->streams; ++i) {
    handle = aacDecoder_Open(TT_MP4_RAW, 1);
    if (!handle) {
      ret = IAMF_ERR_INVALID_STATE;
      break;
    }

    st->handles[i] = handle;

    if (i < coupled_streams)
      ret = aacDecoder_ConfigRaw(handle, conf, &clen);
    else
      ret = aacDecoder_ConfigRaw(handle, &conf[1], &clen);
    if (ret != AAC_DEC_OK) {
      ia_loge("aac config raw error code %d", ret);
      ret = IAMF_ERR_INTERNAL;
      break;
    }
    ret = aacDecoder_SetParam(handle, AAC_CONCEAL_METHOD, 1);
    if (ret != AAC_DEC_OK) {
      ia_loge("aac set parameter error code %d", ret);
      ret = IAMF_ERR_INTERNAL;
      break;
    }
  }

end:
  if (ret < 0) {
    if (error) *error = ret;
    if (st) aac_multistream_decoder_close(st);
    st = 0;
  }

  IAMF_FREE(conf[1]);

  return st;
}

int aac_multistream_decode(AACMSDecoder *st, uint8_t *buffer[], uint32_t size[],
                           void *pcm, uint32_t frame_size) {
  if (st->flags & AUDIO_FRAME_PLANE)
    return aac_multistream_decode_native(st, buffer, size, pcm, frame_size,
                                         aac_copy_channel_out_short_plane);
  ia_loge("flags is 0x%x, is not implmeneted.", st->flags);
  return IAMF_ERR_UNIMPLEMENTED;
}

void aac_multistream_decoder_close(AACMSDecoder *st) {
  if (st) {
    if (st->handles) {
      for (int i = 0; i < st->streams; ++i) {
        if (st->handles[i]) {
          aacDecoder_Close(st->handles[i]);
        }
      }
      free(st->handles);
    }
    free(st);
  }
}

int aac_multistream_decoder_get_delay(AACMSDecoder *st) {
  return st->delay < 0 ? 0 : st->delay;
}
