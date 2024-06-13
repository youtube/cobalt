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
 * @file ae_rdr.h
 * @brief Rendering APIs.
 * @version 0.1
 * @date Created 03/03/2023
 **/

#ifndef AE_RDR_H_
#define AE_RDR_H_

#include <stdint.h>

typedef enum {
  BS2051_A = 0x020,        // 2ch output
  BS2051_B = 0x050,        // 6ch output
  BS2051_C = 0x250,        // 8ch output
  BS2051_D = 0x450,        // 10ch output
  BS2051_E = 0x451,        // 11ch output
  BS2051_F = 0x370,        // 12ch output
  BS2051_G = 0x490,        // 14ch output
  BS2051_H = 0x9A3,        // 24ch output
  BS2051_I = 0x070,        // 8ch output
  BS2051_J = 0x470,        // 12ch output
  IAMF_STEREO = 0x200,     // 2ch input
  IAMF_51 = 0x510,         // 6ch input
  IAMF_512 = 0x512,        // 8ch input
  IAMF_514 = 0x514,        // 10ch input
  IAMF_71 = 0x710,         // 8ch input
  IAMF_714 = 0x714,        // 12ch input
  IAMF_MONO = 0x100,       // 1ch input, AOM only
  IAMF_712 = 0x712,        // 10ch input, AOM only
  IAMF_312 = 0x312,        // 6ch input, AOM only
  IAMF_BINAURAL = 0x1020,  // binaural input/output AOM only
} IAMF_SOUND_SYSTEM;

#ifndef DISABLE_LFE_HOA
#define DISABLE_LFE_HOA 1
#endif

// BINAURAL feature
#ifndef ENABLE_HOA_TO_BINAURAL
#define ENABLE_HOA_TO_BINAURAL 0
#endif

#ifndef ENABLE_MULTICHANNEL_TO_BINAURAL
#define ENABLE_MULTICHANNEL_TO_BINAURAL 0
#endif

#if ENABLE_HOA_TO_BINAURAL || ENABLE_MULTICHANNEL_TO_BINAURAL
#define N_SOURCE_ELM \
  2  // maximum number of audio element == maximum source id array size
typedef struct {
#if ENABLE_MULTICHANNEL_TO_BINAURAL
  int m2b_init;
  void *m2b_api;
  int m2b_elm_id[N_SOURCE_ELM];
  int m2b_source_id[N_SOURCE_ELM];
#endif
#if ENABLE_HOA_TO_BINAURAL
  int h2b_init;
  void *h2b_api;
  int h2b_elm_id[N_SOURCE_ELM];
  int h2b_amb_id[N_SOURCE_ELM];
  int h2b_inchs[N_SOURCE_ELM];
#endif
} binaural_filter_t;
#endif

typedef struct {
  int init;
  float c, a1, a2, a3, b1, b2;
  float input_history[2], output_history[2];
} lfe_filter_t;

typedef struct {
  IAMF_SOUND_SYSTEM system;
  int lfe1;
  int lfe2;
} IAMF_PREDEFINED_SP_LAYOUT;

typedef struct {
  int undefined;  // not yet defined.
} IAMF_CUSTOM_SP_LAYOUT;

typedef struct {
  int sp_type;  // 0: predefined_sp, 1: custom_sp
  union {
    IAMF_PREDEFINED_SP_LAYOUT *predefined_sp;
    IAMF_CUSTOM_SP_LAYOUT *custom_sp;
  } sp_layout;
  lfe_filter_t lfe_f;  // for H2M lfe
#if ENABLE_HOA_TO_BINAURAL || ENABLE_MULTICHANNEL_TO_BINAURAL
  binaural_filter_t binaural_f;
#endif
} IAMF_SP_LAYOUT;

typedef enum {
  IAMF_ZOA = 0,
  IAMF_FOA = 1,
  IAMF_SOA = 2,
  IAMF_TOA = 3
} HOA_ORDER;

typedef struct {
  HOA_ORDER order;
  int lfe_on;  // HOA lfe on/off
} IAMF_HOA_LAYOUT;

struct m2m_rdr_t {
  IAMF_SOUND_SYSTEM in;
  IAMF_SOUND_SYSTEM out;
  float *mat;
  int m;
  int n;
};

struct h2m_rdr_t {
  HOA_ORDER in;
  IAMF_SOUND_SYSTEM out;
  int channels;
  int lfe1;
  int lfe2;
  float *mat;
  int m;
  int n;
};

// Multichannel to Multichannel
/**
 * @brief     Get the ear render conversion matrix of multichannel to
 * multichannel according to predefined direct speaker input and output layout.
 * @param     [in] in : predefined direct speaker input channel layout.
 * @param     [in] out : predefined direct speaker output channel layout
 * @param     [in] outMatrix : conversion matrix.
 * @return    @0: success,@others: fail
 */
int IAMF_element_renderer_get_M2M_matrix(IAMF_SP_LAYOUT *in,
                                         IAMF_SP_LAYOUT *out,
                                         struct m2m_rdr_t *outMatrix);

/**
 * @brief     Get the ear render conversion matrix of multichannel to
 * multichannel according to predefined direct speaker input and custom output
 * layout.
 * @param     [in] in : custom speaker input layout.
 * @param     [in] out : custom speaker output layout
 * @param     [in] outMatrix : conversion matrix.
 * @return    @0: success,@others: fail
 */
int IAMF_element_renderer_get_M2M_custom_matrix(IAMF_SP_LAYOUT *in,
                                                IAMF_CUSTOM_SP_LAYOUT *out,
                                                struct m2m_rdr_t *outMatrix);

/**
 * @brief     Multichannel to Multichannel Renderer.
 * @param     [in] in : the pcm signal of input.
 * @param     [in] out : the pcm signal of output
 * @param     [in] nsamples : the processed samples of pcm signal.
 * @return    @0: success,@others: fail
 */
int IAMF_element_renderer_render_M2M(struct m2m_rdr_t *m2mMatrix, float *in[],
                                     float *out[], int nsamples);

// HOA to Multichannel
#if DISABLE_LFE_HOA == 0
void lfefilter_init(lfe_filter_t *lfe_f, float cutoff_freq,
                    float sampling_rate);
#endif
/**
 * @brief     Get the ear render conversion matrix of hoa to multichannel
 *            according to hoa input and direct speaker output layout.
 * @param     [in] in : HOA channel layout.
 * @param     [in] out : direct speaker output channel layout
 * @param     [in] outMatrix : conversion matrix.
 * @return    @0: success,@others: fail
 */
int IAMF_element_renderer_get_H2M_matrix(IAMF_HOA_LAYOUT *in,
                                         IAMF_PREDEFINED_SP_LAYOUT *out,
                                         struct h2m_rdr_t *outMatrix);

/**
 * @brief     Get the ear render conversion matrix of hoa to multichannel
 *            according to hoa input and custom output layout.
 * @param     [in] in : custom output channel layout
 * @param     [in] outMatrix : conversion matrix.
 * @return    @0: success,@others: fail
 */
int IAMF_element_renderer_get_H2M_custom_matrix(
    IAMF_HOA_LAYOUT *in_layout, IAMF_CUSTOM_SP_LAYOUT *out_layout,
    struct h2m_rdr_t *outMatrix);

/**
 * @brief     Hoa to Multichannel Renderer.
 * @param     [in] in : the pcm signal of input.
 * @param     [in] out : the pcm signal of output
 * @param     [in] nsamples : the processed samples of pcm signal.
 * @param     [in] lfe : the filter to prcoess lfe channel.
 * @return    @0: success,@others: fail
 */
int IAMF_element_renderer_render_H2M(struct h2m_rdr_t *h2mMatrix, float *in[],
                                     float *out[], int nsamples,
                                     lfe_filter_t *lfe);

#if ENABLE_MULTICHANNEL_TO_BINAURAL
// Multichannel to Binaural(BEAR)
/**
 * @brief     Initialize the conversion filter of multichannel to binaural
 *            according to predefined direct speaker input layout.
 * @param     [in] binaural_f : the binaural filter.
 * @param     [in] in_layout : predefined direct speaker input channel layout
 * @param     [in] elm_id : the element id.
 * @param     [in] frame_size : the size of one pcm frame.
 * @param     [in] sample_rate : the sample rate of pcm signal.
 */
void IAMF_element_renderer_init_M2B(binaural_filter_t *binaural_f,
                                    uint32_t in_layout, uint64_t elm_id,
                                    int frame_size, int sample_rate);

/**
 * @brief     De-initialize the conversion filter of multichannel to binaural.
 * @param     [in] binaural_f : the binaural filter.
 * @param     [in] elm_id : the element id.
 */
void IAMF_element_renderer_deinit_M2B(binaural_filter_t *binaural_f,
                                      uint64_t elm_id);

/**
 * @brief     Multichannel to Binaural Renderer.
 * @param     [in] binaural_f : the binaural filter.
 * @param     [in] elm_id : the element id.
 * @param     [in] in : the input pcm signal.
 * @param     [in] in : the output pcm signal.
 */
int IAMF_element_renderer_render_M2B(binaural_filter_t *binaural_f,
                                     uint64_t elm_id, float *in[], float *out[],
                                     int nsamples);
#endif

#if ENABLE_HOA_TO_BINAURAL
// HOA to Binaural(Resonance)
/**
 * @brief     Initialize the conversion filter of hoa to binaural according to
 * hoa input.
 * @param     [in] binaural_f : the binaural filter.
 * @param     [in] in_channels : the channels of input hoa.
 * @param     [in] elm_id : the element id.
 * @param     [in] frame_size : the size of one pcm frame.
 * @param     [in] sample_rate : the sample rate of pcm signal.
 */
void IAMF_element_renderer_init_H2B(binaural_filter_t *binaural_f,
                                    int in_channels, uint64_t elm_id,
                                    int frame_size, int sample_rate);

/**
 * @brief     De-initialize the conversion filter of hoa to binaural.
 * @param     [in] binaural_f : the binaural filter.
 * @param     [in] elm_id : the element id.
 */
void IAMF_element_renderer_deinit_H2B(binaural_filter_t *binaural_f,
                                      uint64_t elm_id);

/**
 * @brief     HOA to Binaural Renderer.
 * @param     [in] binaural_f : the binaural filter.
 * @param     [in] elm_id : the element id.
 * @param     [in] in : the input pcm signal.
 * @param     [in] in : the output pcm signal.
 */
int IAMF_element_renderer_render_H2B(binaural_filter_t *binaural_f,
                                     uint64_t elm_id, float *in[], float *out[],
                                     int nsamples);
#endif

#endif
