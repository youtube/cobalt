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
 * @file h2b_rdr.c
 * @brief HOA to Binaural rendering.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#include "ae_rdr.h"

#if ENABLE_HOA_TO_BINAURAL
#if defined(__linux__) || defined(__APPLE__)
#else
#pragma comment(lib, "iamf2resonance.lib")
#endif
#include "resonance/iamf_resonance_api.h"

// HOA to Binaural Renderer(Resonance)
void IAMF_element_renderer_init_H2B(binaural_filter_t* binaural_f,
                                    int in_channels, uint64_t elm_id,
                                    int nsamples, int sample_rate) {
  int inchs = in_channels;
  int frame_size = nsamples;
  int i;

  if (binaural_f->h2b_init != 1) {
    binaural_f->h2b_api =
        CreateResonanceAudioApi2(2, frame_size, sample_rate);  // 2 = outchs
    if (binaural_f->h2b_api) {
      for (i = 0; i < N_SOURCE_ELM; i++) {
        binaural_f->h2b_elm_id[i] = -1;
        binaural_f->h2b_amb_id[i] = -1;
        binaural_f->h2b_inchs[i] = -1;
      }
      binaural_f->h2b_init = 1;
    }
  }

  if (binaural_f->h2b_api) {
    for (i = 0; i < N_SOURCE_ELM; i++) {
      if (binaural_f->h2b_elm_id[i] == -1) break;
    }
    if (i < N_SOURCE_ELM) {
      binaural_f->h2b_amb_id[i] =
          CreateAmbisonicSource(binaural_f->h2b_api, inchs);
      binaural_f->h2b_inchs[i] = in_channels;
      binaural_f->h2b_elm_id[i] = elm_id;
    }
  }
}

void IAMF_element_renderer_deinit_H2B(binaural_filter_t* binaural_f,
                                      uint64_t elm_id) {
  int i;

  if (binaural_f->h2b_init != 0) {
    for (i = 0; i < N_SOURCE_ELM; i++) {
      if (binaural_f->h2b_elm_id[i] == elm_id) {
        if (binaural_f->h2b_amb_id[i] >= 0) {
          DestroySource(binaural_f->h2b_api, binaural_f->h2b_amb_id[i]);
          binaural_f->h2b_elm_id[i] = -1;
          binaural_f->h2b_amb_id[i] = -1;
          binaural_f->h2b_inchs[i] = -1;
        }
      }
    }
    for (i = 0; i < N_SOURCE_ELM; i++) {
      if (binaural_f->h2b_amb_id[i] >= 0) {
        break;
      }
    }
    if (i == N_SOURCE_ELM) {
      DestoryResonanceAudioApi2(binaural_f->h2b_api);
      binaural_f->h2b_init = 0;
    }
  }
}

int IAMF_element_renderer_render_H2B(binaural_filter_t* binaural_f,
                                     uint64_t elm_id, float* in[], float* out[],
                                     int nsamples) {
  const float* const* sin = (const float* const*)in;
  float** sout = (float**)out;
  int inchs = 0;
  int frame_size = nsamples;
  int i;

  if (binaural_f->h2b_init != 0) {
    for (i = 0; i < N_SOURCE_ELM; i++) {
      if (binaural_f->h2b_elm_id[i] == elm_id) {
        inchs = binaural_f->h2b_inchs[i];
        SetPlanarBufferFloat(binaural_f->h2b_api, binaural_f->h2b_amb_id[i],
                             sin, inchs, frame_size);
        FillPlanarOutputBufferFloat(binaural_f->h2b_api, 2, frame_size,
                                    sout);  // 2 = outchs
        return 0;
      }
    }
  }
  return -1;
}
#endif
