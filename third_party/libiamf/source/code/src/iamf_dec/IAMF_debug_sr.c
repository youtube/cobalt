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
 * @file IAMF_debug_sr.c
 * @brief Debug APIs.
 * @version 0.1
 * @date Created 03/03/2023
 **/


#include <stdio.h>

#include "IAMF_utils.h"
#include "wav/dep_wavwriter.h"
#define N_AUDIO_STREAM 10

static void ia_decoder_plane2stride_out_float(void *dst, const float *src,
                                              int frame_size, int channels) {
  float *float_dst = (float *)dst;

  for (int c = 0; c < channels; ++c) {
    if (src) {
      for (int i = 0; i < frame_size; i++) {
        float_dst[i * channels + c] = src[frame_size * c + i];
      }
    } else {
      for (int i = 0; i < frame_size; i++) {
        float_dst[i * channels + c] = 0;
      }
    }
  }
}

struct stream_log_t {
  void *wav;
  int element_id;
  int nchannels;
};

int _dec_stream_count = 0;
int _rec_stream_count = 0;
int _ren_stream_count = 0;
int _mix_stream_count = 0;
struct stream_log_t _dec_stream_log[N_AUDIO_STREAM];
struct stream_log_t _rec_stream_log[N_AUDIO_STREAM];
struct stream_log_t _ren_stream_log[N_AUDIO_STREAM];
struct stream_log_t _mix_stream_log;

void iamf_rec_stream_log(int eid, int chs, float *in, int size) {
  int i;
  void *wf = NULL;
  void *pcm_b;
  int nch = 0;

  if (size <= 0) return;

  for (i = 0; i < N_AUDIO_STREAM; i++) {
    if (_rec_stream_log[i].element_id == eid) {
      nch = _rec_stream_log[i].nchannels;
      wf = _rec_stream_log[i].wav;
      break;
    }
  }
  if (i == N_AUDIO_STREAM) {
    char ae_fname[256];
    if (_rec_stream_count < N_AUDIO_STREAM) {
      sprintf(ae_fname, "rec_%d.wav", eid);
      nch = _rec_stream_log[_rec_stream_count].nchannels = chs;
      wf = _rec_stream_log[_rec_stream_count].wav =
          dep_wav_write_open2(ae_fname, DEP_WAVE_FORMAT_FLOAT, 48000, 32, nch);
      _rec_stream_log[_rec_stream_count].element_id = eid;
      _rec_stream_count++;
    }
  }

  pcm_b = IAMF_MALLOC(float, size *nch);
  ia_decoder_plane2stride_out_float(pcm_b, in, size, nch);
  dep_wav_write_data(wf, pcm_b, size * nch * sizeof(float));
  free(pcm_b);
}

void iamf_ren_stream_log(int eid, int chs, float *out, int size) {
  int i;
  void *wf = NULL;
  void *pcm_b;
  int nch = 0;

  if (size <= 0) return;

  for (i = 0; i < N_AUDIO_STREAM; i++) {
    if (_ren_stream_log[i].element_id == eid) {
      nch = _ren_stream_log[i].nchannels;
      wf = _ren_stream_log[i].wav;
      break;
    }
  }
  if (i == N_AUDIO_STREAM) {
    char ae_fname[256];
    if (_ren_stream_count < N_AUDIO_STREAM) {
      sprintf(ae_fname, "ren_%d.wav", eid);
      nch = _ren_stream_log[_ren_stream_count].nchannels = chs;
      /* iamf_layout_channels_count(&stream->final_layout->layout); */
      wf = _ren_stream_log[_ren_stream_count].wav =
          dep_wav_write_open2(ae_fname, DEP_WAVE_FORMAT_FLOAT, 48000, 32, nch);
      _ren_stream_log[_ren_stream_count].element_id = eid;
      _ren_stream_count++;
    }
  }

  pcm_b = IAMF_MALLOC(float, size *nch);
  ia_decoder_plane2stride_out_float(pcm_b, out, size, nch);
  dep_wav_write_data(wf, pcm_b, size * nch * sizeof(float));
  free(pcm_b);
}

void iamf_mix_stream_log(int chs, float *out, int size) {
  void *wf = NULL;
  void *pcm_b;
  int nch = 0;

  if (size <= 0) return;

  if (_mix_stream_count > 0) {
    nch = _mix_stream_log.nchannels;
    wf = _mix_stream_log.wav;
  } else {
    char ae_fname[256];
    sprintf(ae_fname, "mix.wav");
    nch = _mix_stream_log.nchannels = chs;
    /* iamf_layout_channels_count(&stream->final_layout->layout); */
    wf = _mix_stream_log.wav =
        dep_wav_write_open2(ae_fname, DEP_WAVE_FORMAT_FLOAT, 48000, 32, nch);
    _mix_stream_count++;
  }

  pcm_b = IAMF_MALLOC(float, size *nch);
  ia_decoder_plane2stride_out_float(pcm_b, out, size, nch);
  dep_wav_write_data(wf, pcm_b, size * nch * sizeof(float));
  free(pcm_b);
}

void iamf_stream_log_free() {
  for (int i = 0; i < _dec_stream_count; i++) {
    dep_wav_write_close(_dec_stream_log[i].wav);
  }
  for (int i = 0; i < _rec_stream_count; i++) {
    dep_wav_write_close(_rec_stream_log[i].wav);
  }
  for (int i = 0; i < _ren_stream_count; i++) {
    dep_wav_write_close(_ren_stream_log[i].wav);
  }
  if (_mix_stream_log.wav) {
    dep_wav_write_close(_mix_stream_log.wav);
  }
}
