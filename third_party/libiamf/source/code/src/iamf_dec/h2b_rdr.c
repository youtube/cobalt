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
#if defined(_WIN32)
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
      DestroyResonanceAudioApi2(binaural_f->h2b_api);
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
