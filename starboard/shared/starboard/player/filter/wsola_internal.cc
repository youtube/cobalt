// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Modifications Copyright 2017 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "starboard/shared/starboard/player/filter/wsola_internal.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <vector>

#include "starboard/common/check_op.h"
#include "starboard/common/log.h"

#if SB_IS(ARCH_X86) || SB_IS(ARCH_X64)
#define USE_SIMD 1
#include <xmmintrin.h>
#elif (SB_IS(ARCH_ARM) || SB_IS(ARCH_ARM64)) && defined(USE_NEON)
#define USE_SIMD 1
#include <arm_neon.h>

#endif  // SB_IS(ARCH_X86) || SB_IS(ARCH_X64)

namespace starboard {

namespace {

// Define kPi because M_PI isn't standard POSIX.
constexpr float kPi = 3.1415926535f;

bool InInterval(int n, Interval q) {
  return n >= q.first && n <= q.second;
}

float MultiChannelSimilarityMeasure(const float* dot_prod_a_b,
                                    const float* energy_a,
                                    const float* energy_b,
                                    int channels) {
  const float kEpsilon = 1e-12f;
  float similarity_measure = 0.0f;
  for (int n = 0; n < channels; ++n) {
    similarity_measure +=
        dot_prod_a_b[n] / std::sqrt(energy_a[n] * energy_b[n] + kEpsilon);
  }
  return similarity_measure;
}

#if (SB_IS(ARCH_ARM) || SB_IS(ARCH_ARM64)) && defined(USE_NEON)
void DotProductMono_NEON(const float* a_src,
                         const float* b_src,
                         int num_frames,
                         float* dot_product) {
  const int last_index = num_frames - (num_frames % 4);
  float32x4_t sum = vmovq_n_f32(0);
  for (int s = 0; s < last_index; s += 4) {
    sum = vmlaq_f32(sum, vld1q_f32(a_src + s), vld1q_f32(b_src + s));
  }
  float32x2_t half = vadd_f32(vget_high_f32(sum), vget_low_f32(sum));
  float sum_scalar = vget_lane_f32(vpadd_f32(half, half), 0);
  for (int s = last_index; s < num_frames; ++s) {
    sum_scalar += a_src[s] * b_src[s];
  }
  dot_product[0] = sum_scalar;
}

void DotProductStereo_NEON(const float* a_src,
                           const float* b_src,
                           int num_frames,
                           float* dot_product) {
  const int last_index = num_frames - (num_frames % 4);
  float32x4_t sum_L = vmovq_n_f32(0);
  float32x4_t sum_R = vmovq_n_f32(0);
  for (int s = 0; s < last_index; s += 4) {
    float32x4x2_t a_reg = vld2q_f32(a_src + s * 2);
    float32x4x2_t b_reg = vld2q_f32(b_src + s * 2);
    sum_L = vmlaq_f32(sum_L, a_reg.val[0], b_reg.val[0]);
    sum_R = vmlaq_f32(sum_R, a_reg.val[1], b_reg.val[1]);
  }
  float32x2_t half_L = vadd_f32(vget_high_f32(sum_L), vget_low_f32(sum_L));
  float sum_L_scalar = vget_lane_f32(vpadd_f32(half_L, half_L), 0);

  float32x2_t half_R = vadd_f32(vget_high_f32(sum_R), vget_low_f32(sum_R));
  float sum_R_scalar = vget_lane_f32(vpadd_f32(half_R, half_R), 0);
  for (int s = last_index; s < num_frames; ++s) {
    sum_L_scalar += a_src[s * 2] * b_src[s * 2];
    sum_R_scalar += a_src[s * 2 + 1] * b_src[s * 2 + 1];
  }
  dot_product[0] = sum_L_scalar;
  dot_product[1] = sum_R_scalar;
}
#endif  // (SB_IS(ARCH_ARM) || SB_IS(ARCH_ARM64)) && defined(USE_NEON)

void MultiChannelDotProduct(const DecodedAudio* a,
                            int frame_offset_a,
                            const DecodedAudio* b,
                            int frame_offset_b,
                            int num_frames,
                            float* dot_product,
                            bool enable_optimized) {
  SB_DCHECK_EQ(a->channels(), b->channels());
  SB_DCHECK_GE(frame_offset_a, 0);
  SB_DCHECK_GE(frame_offset_b, 0);
  SB_DCHECK_LE(frame_offset_a + num_frames, a->frames());
  SB_DCHECK_LE(frame_offset_b + num_frames, b->frames());

  const float* a_frames = a->data_as_float32();
  const float* b_frames = b->data_as_float32();
  const int channels = a->channels();

  if (!enable_optimized) {
    // WARNING: This path contains buggy logic for multi-channel audio
    // (processes interleaved data as planar, causing cross-channel
    // contamination). It is kept as-is as a baseline.
#if defined(USE_SIMD)
    const int rem = num_frames % 4;
    const int last_index = num_frames - rem;
    for (int ch = 0; ch < channels; ++ch) {
      const float* a_src = a_frames + frame_offset_a * channels + ch;
      const float* b_src = b_frames + frame_offset_b * channels + ch;

#if SB_IS(ARCH_X86) || SB_IS(ARCH_X64)
      __m128 m_sum = _mm_setzero_ps();
      for (int s = 0; s < last_index; s += 4) {
        m_sum = _mm_add_ps(m_sum, _mm_mul_ps(_mm_loadu_ps(a_src + s),
                                             _mm_loadu_ps(b_src + s)));
      }
      m_sum = _mm_add_ps(_mm_movehl_ps(m_sum, m_sum), m_sum);
      _mm_store_ss(dot_product + ch,
                   _mm_add_ss(m_sum, _mm_shuffle_ps(m_sum, m_sum, 1)));
#elif SB_IS(ARCH_ARM) || SB_IS(ARCH_ARM64)
      float32x4_t m_sum = vmovq_n_f32(0);
      for (int s = 0; s < last_index; s += 4) {
        m_sum = vmlaq_f32(m_sum, vld1q_f32(a_src + s), vld1q_f32(b_src + s));
      }
      float32x2_t m_half = vadd_f32(vget_high_f32(m_sum), vget_low_f32(m_sum));
      dot_product[ch] = vget_lane_f32(vpadd_f32(m_half, m_half), 0);
#endif
    }

    if (!rem) {
      return;
    }
    int rem_num_frames = rem;
    int rem_frame_offset_a = frame_offset_a + last_index;
    int rem_frame_offset_b = frame_offset_b + last_index;
#else   // defined(USE_SIMD)
    memset(dot_product, 0, sizeof(*dot_product) * channels);
    int rem_num_frames = num_frames;
    int rem_frame_offset_a = frame_offset_a;
    int rem_frame_offset_b = frame_offset_b;
#endif  // defined(USE_SIMD)

    for (int k = 0; k < channels; ++k) {
      const float* ch_a = a_frames + rem_frame_offset_a * channels + k;
      const float* ch_b = b_frames + rem_frame_offset_b * channels + k;
      for (int n = 0; n < rem_num_frames; ++n) {
        dot_product[k] += *ch_a * *ch_b;
        ch_a += channels;
        ch_b += channels;
      }
    }
    return;
  }

  // Optimized (correct) path.
  // Note: Currently only ARM NEON SIMD paths are implemented. On x86/x64
  // platforms, this path falls back to scalar C++ loops, which is slower for
  // Mono audio than the legacy SSE implementation in the unoptimized baseline
  // path.
  if (num_frames == 0) {
    return;
  }

#if (SB_IS(ARCH_ARM) || SB_IS(ARCH_ARM64)) && defined(USE_NEON)
  if (channels == 1 && enable_optimized) {
    DotProductMono_NEON(a_frames + frame_offset_a, b_frames + frame_offset_b,
                        num_frames, dot_product);
    return;
  } else if (channels == 2 && enable_optimized) {
    DotProductStereo_NEON(a_frames + frame_offset_a * 2,
                          b_frames + frame_offset_b * 2, num_frames,
                          dot_product);
    return;
  }
#endif  // (SB_IS(ARCH_ARM) || SB_IS(ARCH_ARM64)) && defined(USE_NEON)

  memset(dot_product, 0, sizeof(*dot_product) * channels);

  for (int n = 0; n < num_frames; ++n) {
    const float* ch_a = a_frames + (frame_offset_a + n) * channels;
    const float* ch_b = b_frames + (frame_offset_b + n) * channels;
    for (int k = 0; k < channels; ++k) {
      dot_product[k] += ch_a[k] * ch_b[k];
    }
  }
}

void MultiChannelMovingBlockEnergies(const DecodedAudio* input,
                                     int frames_per_block,
                                     float* energy) {
  int num_blocks = input->frames() - (frames_per_block - 1);
  int channels = input->channels();

  const float* input_frames = input->data_as_float32();
  for (int k = 0; k < channels; ++k) {
    const float* input_channel = input_frames + k;

    energy[k] = 0;

    // First block of channel |k|.
    for (int m = 0; m < frames_per_block; ++m) {
      energy[k] += input_channel[m * channels] * input_channel[m * channels];
    }

    const float* slide_out = input_channel;
    const float* slide_in = input_channel + frames_per_block * channels;
    for (int n = 1; n < num_blocks;
         ++n, slide_in += channels, slide_out += channels) {
      energy[k + n * channels] = energy[k + (n - 1) * channels] -
                                 *slide_out * *slide_out +
                                 *slide_in * *slide_in;
    }
  }
}

// Fit the curve f(x) = a * x^2 + b * x + c such that
//   f(-1) = y[0]
//   f(0) = y[1]
//   f(1) = y[2]
// and return the maximum, assuming that y[0] <= y[1] >= y[2].
void QuadraticInterpolation(const float* y_values,
                            float* extremum,
                            float* extremum_value) {
  float a = 0.5f * (y_values[2] + y_values[0]) - y_values[1];
  float b = 0.5f * (y_values[2] - y_values[0]);
  float c = y_values[1];

  if (a == 0.f) {
    // The coordinates are colinear (within floating-point error).
    *extremum = 0;
    *extremum_value = y_values[1];
  } else {
    *extremum = -b / (2.f * a);
    *extremum_value = a * (*extremum) * (*extremum) + b * (*extremum) + c;
  }
}

int DecimatedSearch(int decimation,
                    Interval exclude_interval,
                    const DecodedAudio* target_block,
                    const DecodedAudio* search_segment,
                    const float* energy_target_block,
                    const float* energy_candidate_blocks,
                    bool enable_optimized) {
  int channels = search_segment->channels();
  int block_size = target_block->frames();
  int num_candidate_blocks = search_segment->frames() - (block_size - 1);
  thread_local std::vector<float> dot_prod;
  dot_prod.resize(channels);
  float similarity[3];  // Three elements for cubic interpolation.

  int n = 0;
  MultiChannelDotProduct(target_block, 0, search_segment, n, block_size,
                         dot_prod.data(), enable_optimized);
  similarity[0] = MultiChannelSimilarityMeasure(
      dot_prod.data(), energy_target_block,
      &energy_candidate_blocks[n * channels], channels);

  // Set the starting point as optimal point.
  float best_similarity = similarity[0];
  int optimal_index = 0;

  n += decimation;
  if (n >= num_candidate_blocks) {
    return 0;
  }

  MultiChannelDotProduct(target_block, 0, search_segment, n, block_size,
                         dot_prod.data(), enable_optimized);
  similarity[1] = MultiChannelSimilarityMeasure(
      dot_prod.data(), energy_target_block,
      &energy_candidate_blocks[n * channels], channels);

  n += decimation;
  if (n >= num_candidate_blocks) {
    // We cannot do any more sampling. Compare these two values and return the
    // optimal index.
    return similarity[1] > similarity[0] ? decimation : 0;
  }

  for (; n < num_candidate_blocks; n += decimation) {
    MultiChannelDotProduct(target_block, 0, search_segment, n, block_size,
                           dot_prod.data(), enable_optimized);

    similarity[2] = MultiChannelSimilarityMeasure(
        dot_prod.data(), energy_target_block,
        &energy_candidate_blocks[n * channels], channels);

    if ((similarity[1] > similarity[0] && similarity[1] >= similarity[2]) ||
        (similarity[1] >= similarity[0] && similarity[1] > similarity[2])) {
      // A local maximum is found. Do a cubic interpolation for a better
      // estimate of candidate maximum.
      float normalized_candidate_index;
      float candidate_similarity;
      QuadraticInterpolation(similarity, &normalized_candidate_index,
                             &candidate_similarity);

      int candidate_index =
          n - decimation +
          static_cast<int>(normalized_candidate_index * decimation + 0.5f);
      if (candidate_similarity > best_similarity &&
          !InInterval(candidate_index, exclude_interval)) {
        optimal_index = candidate_index;
        best_similarity = candidate_similarity;
      }
    } else if (n + decimation >= num_candidate_blocks &&
               similarity[2] > best_similarity &&
               !InInterval(n, exclude_interval)) {
      // If this is the end-point and has a better similarity-measure than
      // optimal, then we accept it as optimal point.
      optimal_index = n;
      best_similarity = similarity[2];
    }
    memmove(similarity, &similarity[1], 2 * sizeof(*similarity));
  }
  return optimal_index;
}

int FullSearch(int low_limit,
               int high_limit,
               Interval exclude_interval,
               const DecodedAudio* target_block,
               const DecodedAudio* search_block,
               const float* energy_target_block,
               const float* energy_candidate_blocks,
               bool enable_optimized) {
  int channels = search_block->channels();
  int block_size = target_block->frames();
  thread_local std::vector<float> dot_prod;
  dot_prod.resize(channels);

  float best_similarity = std::numeric_limits<float>::min();
  int optimal_index = 0;

  for (int n = low_limit; n <= high_limit; ++n) {
    if (InInterval(n, exclude_interval)) {
      continue;
    }
    MultiChannelDotProduct(target_block, 0, search_block, n, block_size,
                           dot_prod.data(), enable_optimized);

    float similarity = MultiChannelSimilarityMeasure(
        dot_prod.data(), energy_target_block,
        &energy_candidate_blocks[n * channels], channels);

    if (similarity > best_similarity) {
      best_similarity = similarity;
      optimal_index = n;
    }
  }

  return optimal_index;
}

}  // namespace

int OptimalIndex(const DecodedAudio* search_block,
                 const DecodedAudio* target_block,
                 SbMediaAudioFrameStorageType storage_type,
                 Interval exclude_interval,
                 bool enable_optimized) {
  int channels = search_block->channels();
  SB_DCHECK_EQ(channels, target_block->channels());
  SB_DCHECK_EQ(storage_type, kSbMediaAudioFrameStorageTypeInterleaved);

  int target_size = target_block->frames();
  int num_candidate_blocks = search_block->frames() - (target_size - 1);

  // This is a compromise between complexity reduction and search accuracy. I
  // don't have a proof that down sample of order 5 is optimal. One can compute
  // a decimation factor that minimizes complexity given the size of
  // |search_block| and |target_block|. However, my experiments show the rate of
  // missing the optimal index is significant. This value is chosen
  // heuristically based on experiments.
  constexpr int kSearchDecimation = 5;

  thread_local std::vector<float> energy_target_block;
  thread_local std::vector<float> energy_candidate_blocks;
  energy_target_block.resize(channels);
  energy_candidate_blocks.resize(channels * num_candidate_blocks);

  // Energy of all candid frames.
  MultiChannelMovingBlockEnergies(search_block, target_size,
                                  energy_candidate_blocks.data());

  // Energy of target frame.
  MultiChannelDotProduct(target_block, 0, target_block, 0, target_size,
                         energy_target_block.data(), enable_optimized);

  int optimal_index =
      DecimatedSearch(kSearchDecimation, exclude_interval, target_block,
                      search_block, energy_target_block.data(),
                      energy_candidate_blocks.data(), enable_optimized);

  int lim_low = std::max(0, optimal_index - kSearchDecimation);
  int lim_high =
      std::min(num_candidate_blocks - 1, optimal_index + kSearchDecimation);
  return FullSearch(lim_low, lim_high, exclude_interval, target_block,
                    search_block, energy_target_block.data(),
                    energy_candidate_blocks.data(), enable_optimized);
}

void GetPeriodicHanningWindow(int window_length, float* window) {
  const float scale = 2.0f * kPi / window_length;
  for (int n = 0; n < window_length; ++n) {
    window[n] = 0.5f * (1.0f - std::cos(n * scale));
  }
}

}  // namespace starboard
