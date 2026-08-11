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
 * @file m2b_rdr.c
 * @brief Multichannels to Binaural rendering.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#include "ae_rdr.h"

#if ENABLE_MULTICHANNEL_TO_BINAURAL
#if defined(_WIN32)
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
          DestroyBearChannel(binaural_f->m2b_api, binaural_f->m2b_source_id[i]);
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
      DestroyBearAPI(binaural_f->m2b_api);
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
