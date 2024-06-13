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

/*
AOM-IAMF Standard Deliverable Status:
This software module is out of scope and not part of the IAMF Final Deliverable.
*/

/**
 * @file m2b_rdr.c
 * @brief Multichannels to Binaural rendering.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#include "ae_rdr.h"

#if ENABLE_MULTICHANNEL_TO_BINAURAL
#if defined(__linux__) || defined(__APPLE__)
#else
#pragma comment(lib, "iamf2bear.lib")
#endif
#include "bear/iamf_bear_api.h"
// Multichannel to Binaural Renderer(BEAR)
void IAMF_element_renderer_init_M2B(binaural_filter_t* binaural_f,
                                    uint32_t in_layout, uint64_t elm_id,
                                    int frame_size, int sample_rate) {
  int i;

  if (binaural_f->m2b_init != 1) {
    binaural_f->m2b_api = CreateBearAPI(NULL);  // load default.tf
    for (i = 0; i < N_SOURCE_ELM; i++) {
      binaural_f->m2b_elm_id[i] = -1;
      binaural_f->m2b_source_id[i] = -1;
    }
    binaural_f->m2b_init = 1;
  }

  if (binaural_f->m2b_api) {
    for (i = 0; i < N_SOURCE_ELM; i++) {
      if (binaural_f->m2b_elm_id[i] == -1) break;
    }
    if (i < N_SOURCE_ELM) {
      binaural_f->m2b_source_id[i] = ConfigureBearDirectSpeakerChannel(
          binaural_f->m2b_api, (int)in_layout, (size_t)frame_size, sample_rate);
      binaural_f->m2b_elm_id[i] = elm_id;
    }
  }
}

void IAMF_element_renderer_deinit_M2B(binaural_filter_t* binaural_f,
                                      uint64_t elm_id) {
  int i;

  if (binaural_f->m2b_init != 0) {
    for (i = 0; i < N_SOURCE_ELM; i++) {
      if (binaural_f->m2b_elm_id[i] == elm_id) {
        if (binaural_f->m2b_source_id[i] >= 0) {
          DestoryBearChannel(binaural_f->m2b_api, binaural_f->m2b_source_id[i]);
          binaural_f->m2b_elm_id[i] = -1;
          binaural_f->m2b_source_id[i] = -1;
        }
      }
    }
    for (i = 0; i < N_SOURCE_ELM; i++) {
      if (binaural_f->m2b_source_id[i] >= 0) {
        break;
      }
    }
    if (i == N_SOURCE_ELM) {
      DestoryBearAPI(binaural_f->m2b_api);
      binaural_f->m2b_init = 0;
    }
  }
}

int IAMF_element_renderer_render_M2B(binaural_filter_t* binaural_f,
                                     uint64_t elm_id, float* in[], float* out[],
                                     int nsamples) {
  float** sin = (float**)in;
  float** sout = (float**)out;
  int i;
  if (binaural_f->m2b_init != 0) {
    for (i = 0; i < N_SOURCE_ELM; i++) {
      if (binaural_f->m2b_elm_id[i] == elm_id) {
        SetBearDirectSpeakerChannel(binaural_f->m2b_api,
                                    binaural_f->m2b_source_id[i], sin);
        GetBearRenderedAudio(binaural_f->m2b_api, binaural_f->m2b_source_id[i],
                             sout);
        return 0;
      }
    }
  }
  return -1;
}
#endif
