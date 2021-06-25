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

// MSVC++ requires this to be set before any other includes to get M_PI.
#if !defined(_USE_MATH_DEFINES)
#define _USE_MATH_DEFINES
#endif

#include "starboard/shared/starboard/player/filter/wsola_internal.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

#include "starboard/common/log.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/memory.h"

#include <cstring>

// TODO: Detect Neon on ARM platform and enable SIMD.
#if SB_IS(ARCH_X86) || SB_IS(ARCH_X64)
#define USE_SIMD 1
#include <xmmintrin.h>
#endif  // SB_IS(ARCH_X86) || SB_IS(ARCH_X64)

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {
namespace internal {

namespace {

bool InInterval(int n, Interval q) { return n >= q.first && n <= q.second; }

float MultiChannelSimilarityMeasure(const float* dot_prod_a_b,
                                    const float* energy_a,
                                    const float* energy_b, int channels) {
  const float kEpsilon = 1e-12f;
  float similarity_measure = 0.0f;
  for (int n = 0; n < channels; ++n) {
    similarity_measure +=
        dot_prod_a_b[n] / sqrt(energy_a[n] * energy_b[n] + kEpsilon);
  }
  return similarity_measure;
}

void MultiChannelDotProduct(const scoped_refptr<DecodedAudio>& a,
                            int frame_offset_a,
                            const scoped_refptr<DecodedAudio>& b,
                            int frame_offset_b,
                            int num_frames,
                            float* dot_product) {
  SB_DCHECK(a->channels() == b->channels());
  SB_DCHECK(frame_offset_a >= 0) << frame_offset_a;
  SB_DCHECK(frame_offset_b >= 0) << frame_offset_b;
  SB_DCHECK(frame_offset_a + num_frames <= a->frames());
  SB_DCHECK(frame_offset_b + num_frames <= b->frames());

  const float* a_frames = reinterpret_cast<const float*>(a->buffer());
  const float* b_frames = reinterpret_cast<const float*>(b->buffer());

// SIMD optimized variants can provide a massive speedup to this operation.
#if defined(USE_SIMD)
  const int rem = num_frames % 4;
  const int last_index = num_frames - rem;
  const int channels = a->channels();
  for (int ch = 0; ch < channels; ++ch) {
    const float* a_src = a_frames + frame_offset_a * a->channels() + ch;
    const float* b_src = b_frames + frame_offset_b * b->channels() + ch;

#if SB_IS(ARCH_X86) || SB_IS(ARCH_X64)
    // First sum all components.
    __m128 m_sum = _mm_setzero_ps();
    for (int s = 0; s < last_index; s += 4) {
      m_sum = _mm_add_ps(
          m_sum, _mm_mul_ps(_mm_loadu_ps(a_src + s), _mm_loadu_ps(b_src + s)));
    }

    // Reduce to a single float for this channel. Sadly, SSE1,2 doesn't have a
    // horizontal sum function, so we have to condense manually.
    m_sum = _mm_add_ps(_mm_movehl_ps(m_sum, m_sum), m_sum);
    _mm_store_ss(dot_product + ch,
                 _mm_add_ss(m_sum, _mm_shuffle_ps(m_sum, m_sum, 1)));
#elif SB_IS(ARCH_ARM) || SB_IS(ARCH_ARM64)
    // First sum all components.
    float32x4_t m_sum = vmovq_n_f32(0);
    for (int s = 0; s < last_index; s += 4)
      m_sum = vmlaq_f32(m_sum, vld1q_f32(a_src + s), vld1q_f32(b_src + s));

    // Reduce to a single float for this channel.
    float32x2_t m_half = vadd_f32(vget_high_f32(m_sum), vget_low_f32(m_sum));
    dot_product[ch] = vget_lane_f32(vpadd_f32(m_half, m_half), 0);
#endif  // SB_IS(ARCH_X86) || SB_IS(ARCH_X64)
  }

  if (!rem) {
    return;
  }
  num_frames = rem;
  frame_offset_a += last_index;
  frame_offset_b += last_index;
#else   // defined(USE_SIMD)
  memset(dot_product, 0, sizeof(*dot_product) * a->channels());
#endif  // defined(USE_SIMD)

  for (int k = 0; k < a->channels(); ++k) {
    const float* ch_a = a_frames + frame_offset_a * a->channels() + k;
    const float* ch_b = b_frames + frame_offset_b * b->channels() + k;
    for (int n = 0; n < num_frames; ++n) {
      dot_product[k] += *ch_a * *ch_b;
      ch_a += a->channels();
      ch_b += b->channels();
    }
  }
}

void MultiChannelMovingBlockEnergies(const scoped_refptr<DecodedAudio>& input,
                                     int frames_per_block,
                                     float* energy) {
  int num_blocks = input->frames() - (frames_per_block - 1);
  int channels = input->channels();

  const float* input_frames = reinterpret_cast<const float*>(input->buffer());
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
void QuadraticInterpolation(const float* y_values, float* extremum,
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
                    const scoped_refptr<DecodedAudio>& target_block,
                    const scoped_refptr<DecodedAudio>& search_segment,
                    const float* energy_target_block,
                    const float* energy_candidate_blocks) {
  int channels = search_segment->channels();
  int block_size = target_block->frames();
  int num_candidate_blocks = search_segment->frames() - (block_size - 1);
  scoped_array<float> dot_prod(new float[channels]);
  float similarity[3];  // Three elements for cubic interpolation.

  int n = 0;
  MultiChannelDotProduct(target_block, 0, search_segment, n, block_size,
                         dot_prod.get());
  similarity[0] = MultiChannelSimilarityMeasure(
      dot_prod.get(), energy_target_block,
      &energy_candidate_blocks[n * channels], channels);

  // Set the starting point as optimal point.
  float best_similarity = similarity[0];
  int optimal_index = 0;

  n += decimation;
  if (n >= num_candidate_blocks) {
    return 0;
  }

  MultiChannelDotProduct(target_block, 0, search_segment, n, block_size,
                         dot_prod.get());
  similarity[1] = MultiChannelSimilarityMeasure(
      dot_prod.get(), energy_target_block,
      &energy_candidate_blocks[n * channels], channels);

  n += decimation;
  if (n >= num_candidate_blocks) {
    // We cannot do any more sampling. Compare these two values and return the
    // optimal index.
    return similarity[1] > similarity[0] ? decimation : 0;
  }

  for (; n < num_candidate_blocks; n += decimation) {
    MultiChannelDotProduct(target_block, 0, search_segment, n, block_size,
                           dot_prod.get());

    similarity[2] = MultiChannelSimilarityMeasure(
        dot_prod.get(), energy_target_block,
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
               const scoped_refptr<DecodedAudio>& target_block,
               const scoped_refptr<DecodedAudio>& search_block,
               const float* energy_target_block,
               const float* energy_candidate_blocks) {
  int channels = search_block->channels();
  int block_size = target_block->frames();
  scoped_array<float> dot_prod(new float[channels]);

  float best_similarity = std::numeric_limits<float>::min();
  int optimal_index = 0;

  for (int n = low_limit; n <= high_limit; ++n) {
    if (InInterval(n, exclude_interval)) {
      continue;
    }
    MultiChannelDotProduct(target_block, 0, search_block, n, block_size,
                           dot_prod.get());

    float similarity = MultiChannelSimilarityMeasure(
        dot_prod.get(), energy_target_block,
        &energy_candidate_blocks[n * channels], channels);

    if (similarity > best_similarity) {
      best_similarity = similarity;
      optimal_index = n;
    }
  }

  return optimal_index;
}

}  // namespace

int OptimalIndex(const scoped_refptr<DecodedAudio>& search_block,
                 const scoped_refptr<DecodedAudio>& target_block,
                 SbMediaAudioFrameStorageType storage_type,
                 Interval exclude_interval) {
  int channels = search_block->channels();
  SB_DCHECK(channels == target_block->channels());
  SB_DCHECK(storage_type == kSbMediaAudioFrameStorageTypeInterleaved);

  int target_size = target_block->frames();
  int num_candidate_blocks = search_block->frames() - (target_size - 1);

  // This is a compromise between complexity reduction and search accuracy. I
  // don't have a proof that down sample of order 5 is optimal. One can compute
  // a decimation factor that minimizes complexity given the size of
  // |search_block| and |target_block|. However, my experiments show the rate of
  // missing the optimal index is significant. This value is chosen
  // heuristically based on experiments.
  const int kSearchDecimation = 5;

  scoped_array<float> energy_target_block(new float[channels]);
  scoped_array<float> energy_candidate_blocks(
      new float[channels * num_candidate_blocks]);

  // Energy of all candid frames.
  MultiChannelMovingBlockEnergies(search_block, target_size,
                                  energy_candidate_blocks.get());

  // Energy of target frame.
  MultiChannelDotProduct(target_block, 0, target_block, 0, target_size,
                         energy_target_block.get());

  int optimal_index = DecimatedSearch(
      kSearchDecimation, exclude_interval, target_block, search_block,
      energy_target_block.get(), energy_candidate_blocks.get());

  int lim_low = std::max(0, optimal_index - kSearchDecimation);
  int lim_high =
      std::min(num_candidate_blocks - 1, optimal_index + kSearchDecimation);
  return FullSearch(lim_low, lim_high, exclude_interval, target_block,
                    search_block, energy_target_block.get(),
                    energy_candidate_blocks.get());
}

void GetSymmetricHanningWindow(int window_length, float* window) {
  const float scale = static_cast<float>(2.0 * M_PI) / window_length;
  for (int n = 0; n < window_length; ++n)
    window[n] = 0.5f * (1.0f - cosf(n * scale));
}

}  // namespace internal
}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
