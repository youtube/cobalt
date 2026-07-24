// Copyright 2025 The Chromium Authors. All rights reserved.
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

#include <gtest/gtest.h>

#include <cmath>
#include <random>
#include <vector>

#include "starboard/common/ref_counted.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"
#include "starboard/shared/starboard/player/filter/testing/test_util.h"

namespace starboard {
namespace {

constexpr int kSampleRate = 48'000;
constexpr float kFrequency = 440.0f;

// ============================================================================
// Reference C implementation for verification
// ============================================================================

bool ReferenceInInterval(int n, Interval q) {
  return n >= q.first && n <= q.second;
}

float ReferenceMultiChannelSimilarityMeasure(const float* dot_prod_a_b,
                                             const float* energy_a,
                                             const float* energy_b,
                                             int channels) {
  constexpr float kEpsilon = 1e-12f;
  float similarity_measure = 0.0f;
  for (int n = 0; n < channels; ++n) {
    similarity_measure +=
        dot_prod_a_b[n] / std::sqrt(energy_a[n] * energy_b[n] + kEpsilon);
  }
  return similarity_measure;
}

void ReferenceMultiChannelDotProduct(const DecodedAudio* a,
                                     int frame_offset_a,
                                     const DecodedAudio* b,
                                     int frame_offset_b,
                                     int num_frames,
                                     float* dot_product) {
  const float* a_frames = reinterpret_cast<const float*>(a->data());
  const float* b_frames = reinterpret_cast<const float*>(b->data());
  const int channels = a->channels();
  memset(dot_product, 0, sizeof(*dot_product) * channels);
  for (int k = 0; k < channels; ++k) {
    const float* ch_a = a_frames + frame_offset_a * channels + k;
    const float* ch_b = b_frames + frame_offset_b * channels + k;
    for (int n = 0; n < num_frames; ++n) {
      dot_product[k] += *ch_a * *ch_b;
      ch_a += channels;
      ch_b += channels;
    }
  }
}

void ReferenceMultiChannelMovingBlockEnergies(const DecodedAudio* input,
                                              int frames_per_block,
                                              float* energy) {
  int num_blocks = input->frames() - (frames_per_block - 1);
  int channels = input->channels();
  const float* input_frames = reinterpret_cast<const float*>(input->data());
  for (int k = 0; k < channels; ++k) {
    const float* input_channel = input_frames + k;
    energy[k] = 0;
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

void ReferenceQuadraticInterpolation(const float* y_values,
                                     float* extremum,
                                     float* extremum_value) {
  float a = 0.5f * (y_values[2] + y_values[0]) - y_values[1];
  float b = 0.5f * (y_values[2] - y_values[0]);
  float c = y_values[1];
  if (a == 0.f) {
    *extremum = 0;
    *extremum_value = y_values[1];
  } else {
    *extremum = -b / (2.f * a);
    *extremum_value = a * (*extremum) * (*extremum) + b * (*extremum) + c;
  }
}

int ReferenceDecimatedSearch(int decimation,
                             Interval exclude_interval,
                             const DecodedAudio* target_block,
                             const DecodedAudio* search_segment,
                             const float* energy_target_block,
                             const float* energy_candidate_blocks,
                             bool enable_optimized) {
  int channels = search_segment->channels();
  int block_size = target_block->frames();
  int num_candidate_blocks = search_segment->frames() - (block_size - 1);
  auto dot_prod = std::make_unique<float[]>(channels);
  float similarity[3];

  int n = 0;
  ReferenceMultiChannelDotProduct(target_block, 0, search_segment, n,
                                  block_size, dot_prod.get());
  similarity[0] = ReferenceMultiChannelSimilarityMeasure(
      dot_prod.get(), energy_target_block,
      &energy_candidate_blocks[n * channels], channels);

  float best_similarity = similarity[0];
  int optimal_index = 0;

  n += decimation;
  if (n >= num_candidate_blocks) {
    return 0;
  }

  ReferenceMultiChannelDotProduct(target_block, 0, search_segment, n,
                                  block_size, dot_prod.get());
  similarity[1] = ReferenceMultiChannelSimilarityMeasure(
      dot_prod.get(), energy_target_block,
      &energy_candidate_blocks[n * channels], channels);

  n += decimation;
  if (n >= num_candidate_blocks) {
    return similarity[1] > similarity[0] ? decimation : 0;
  }

  for (; n < num_candidate_blocks; n += decimation) {
    ReferenceMultiChannelDotProduct(target_block, 0, search_segment, n,
                                    block_size, dot_prod.get());
    similarity[2] = ReferenceMultiChannelSimilarityMeasure(
        dot_prod.get(), energy_target_block,
        &energy_candidate_blocks[n * channels], channels);

    if ((similarity[1] > similarity[0] && similarity[1] >= similarity[2]) ||
        (similarity[1] >= similarity[0] && similarity[1] > similarity[2])) {
      float normalized_candidate_index;
      float candidate_similarity;
      ReferenceQuadraticInterpolation(similarity, &normalized_candidate_index,
                                      &candidate_similarity);
      int candidate_index = 0;
      if (enable_optimized) {
        candidate_index = n - decimation +
                          static_cast<int>(std::round(
                              normalized_candidate_index * decimation));
      } else {
        candidate_index =
            n - decimation +
            static_cast<int>(normalized_candidate_index * decimation + 0.5f);
      }
      if (candidate_similarity > best_similarity &&
          !ReferenceInInterval(candidate_index, exclude_interval)) {
        optimal_index = candidate_index;
        best_similarity = candidate_similarity;
      }
    } else if (n + decimation >= num_candidate_blocks &&
               similarity[2] > best_similarity &&
               !ReferenceInInterval(n, exclude_interval)) {
      optimal_index = n;
      best_similarity = similarity[2];
    }
    memmove(similarity, &similarity[1], 2 * sizeof(*similarity));
  }
  return optimal_index;
}

int ReferenceFullSearch(int low_limit,
                        int high_limit,
                        Interval exclude_interval,
                        const DecodedAudio* target_block,
                        const DecodedAudio* search_block,
                        const float* energy_target_block,
                        const float* energy_candidate_blocks) {
  int channels = search_block->channels();
  int block_size = target_block->frames();
  auto dot_prod = std::make_unique<float[]>(channels);
  float best_similarity = std::numeric_limits<float>::min();
  int optimal_index = 0;
  for (int n = low_limit; n <= high_limit; ++n) {
    if (ReferenceInInterval(n, exclude_interval)) {
      continue;
    }
    ReferenceMultiChannelDotProduct(target_block, 0, search_block, n,
                                    block_size, dot_prod.get());
    float similarity = ReferenceMultiChannelSimilarityMeasure(
        dot_prod.get(), energy_target_block,
        &energy_candidate_blocks[n * channels], channels);
    if (similarity > best_similarity) {
      best_similarity = similarity;
      optimal_index = n;
    }
  }
  return optimal_index;
}

int ReferenceOptimalIndex(const DecodedAudio* search_block,
                          const DecodedAudio* target_block,
                          Interval exclude_interval,
                          bool enable_optimized) {
  int channels = search_block->channels();
  int target_size = target_block->frames();
  int num_candidate_blocks = search_block->frames() - (target_size - 1);
  constexpr int kSearchDecimation = 5;
  auto energy_target_block = std::make_unique<float[]>(channels);
  auto energy_candidate_blocks =
      std::make_unique<float[]>(channels * num_candidate_blocks);
  ReferenceMultiChannelMovingBlockEnergies(search_block, target_size,
                                           energy_candidate_blocks.get());
  ReferenceMultiChannelDotProduct(target_block, 0, target_block, 0, target_size,
                                  energy_target_block.get());
  int optimal_index = ReferenceDecimatedSearch(
      kSearchDecimation, exclude_interval, target_block, search_block,
      energy_target_block.get(), energy_candidate_blocks.get(),
      enable_optimized);
  int lim_low = std::max(0, optimal_index - kSearchDecimation);
  int lim_high =
      std::min(num_candidate_blocks - 1, optimal_index + kSearchDecimation);
  return ReferenceFullSearch(lim_low, lim_high, exclude_interval, target_block,
                             search_block, energy_target_block.get(),
                             energy_candidate_blocks.get());
}
TEST(WsolaInternalTest, OptimalIndex_ExactMatch) {
  constexpr int kFrames = 100;
  constexpr int kChannels = 1;
  constexpr float kAmplitude = 0.5f;

  scoped_refptr<DecodedAudio> target_block = CreateTestDecodedAudio(
      50, kChannels, kSampleRate, kFrequency, kAmplitude);
  scoped_refptr<DecodedAudio> search_block = CreateTestDecodedAudio(
      kFrames, kChannels, kSampleRate, kFrequency, kAmplitude);

  // Place an exact match at index 25
  float* search_data = search_block->data_as_float32();
  float* target_data = target_block->data_as_float32();
  memcpy(search_data + 25 * kChannels, target_data,
         50 * kChannels * sizeof(float));

  Interval exclude_interval = {-1, -1};  // No exclusion

  int optimal_index =
      OptimalIndex(search_block.get(), target_block.get(),
                   kSbMediaAudioFrameStorageTypeInterleaved, exclude_interval,
                   /*enable_optimized=*/true);
  // Due to floating point precision and quadratic interpolation, it might not
  // be exactly 25, but should be very close. We test for a small range.
  EXPECT_NEAR(optimal_index, 25, 1);
}

TEST(WsolaInternalTest, OptimalIndex_Stereo_OldPath_Buggy) {
  constexpr int kChannels = 2;
  scoped_refptr<DecodedAudio> target_block =
      CreateTestStereoDecodedAudio(40, kSampleRate, 440.0f, 0.5f, 880.0f, 0.5f);

  std::vector<float> search_data_values(100 * kChannels, 0.0f);
  scoped_refptr<DecodedAudio> search_block =
      CreateTestDecodedAudioWithData(search_data_values, kChannels);

  float* search_data = search_block->data_as_float32();
  const float* target_data = target_block->data_as_float32();
  memcpy(search_data + 20 * kChannels, target_data,
         20 * kChannels * sizeof(float));
  memcpy(search_data + 60 * kChannels, target_data,
         40 * kChannels * sizeof(float));

  Interval exclude_interval = {-1, -1};

  int optimal_index =
      OptimalIndex(search_block.get(), target_block.get(),
                   kSbMediaAudioFrameStorageTypeInterleaved, exclude_interval,
                   /*enable_optimized=*/false);

  // On platforms with SIMD, the old buggy path will incorrectly choose 20.
  // On platforms without SIMD (C fallback), it will correctly choose 60.
  // We assert that it must be one of these two.
  EXPECT_TRUE(optimal_index == 20 || optimal_index == 60)
      << "Old path returned unexpected index: " << optimal_index;
}

TEST(WsolaInternalTest, OptimalIndex_Stereo_NewPath_Correct) {
  constexpr int kChannels = 2;
  scoped_refptr<DecodedAudio> target_block =
      CreateTestStereoDecodedAudio(40, kSampleRate, 440.0f, 0.5f, 880.0f, 0.5f);

  std::vector<float> search_data_values(100 * kChannels, 0.0f);
  scoped_refptr<DecodedAudio> search_block =
      CreateTestDecodedAudioWithData(search_data_values, kChannels);

  float* search_data = search_block->data_as_float32();
  const float* target_data = target_block->data_as_float32();
  memcpy(search_data + 20 * kChannels, target_data,
         20 * kChannels * sizeof(float));
  memcpy(search_data + 60 * kChannels, target_data,
         40 * kChannels * sizeof(float));

  Interval exclude_interval = {-1, -1};

  int optimal_index =
      OptimalIndex(search_block.get(), target_block.get(),
                   kSbMediaAudioFrameStorageTypeInterleaved, exclude_interval,
                   /*enable_optimized=*/true);

  // The new path must always choose 60 (perfect match for all 40 frames).
  EXPECT_NEAR(optimal_index, 60, 2);
}

TEST(WsolaInternalTest, OptimalIndex_NoMatch) {
  constexpr int kFrames = 100;
  constexpr int kChannels = 1;

  scoped_refptr<DecodedAudio> target_block =
      CreateTestDecodedAudio(50, kChannels, kSampleRate, 440.0f, 0.5f);
  scoped_refptr<DecodedAudio> search_block = CreateTestDecodedAudio(
      kFrames, kChannels, kSampleRate, 1000.0f, 0.5f);  // Different frequency

  Interval exclude_interval = {-1, -1};  // No exclusion

  int optimal_index =
      OptimalIndex(search_block.get(), target_block.get(),
                   kSbMediaAudioFrameStorageTypeInterleaved, exclude_interval,
                   /*enable_optimized=*/true);
  // With no clear match, the result is less predictable, but it shouldn't crash
  // and should return a valid index within the search range.
  EXPECT_GE(optimal_index, 0);
  EXPECT_LE(optimal_index, kFrames - target_block->frames());
}

TEST(WsolaInternalTest, OptimalIndex_ExcludeInterval) {
  constexpr int kFrames = 100;
  constexpr int kChannels = 1;
  constexpr float kAmplitude = 0.5f;

  scoped_refptr<DecodedAudio> target_block = CreateTestDecodedAudio(
      20, kChannels, kSampleRate, kFrequency, kAmplitude);
  scoped_refptr<DecodedAudio> search_block = CreateTestDecodedAudio(
      kFrames, kChannels, kSampleRate, kFrequency, kAmplitude);

  // Create an exact match within the exclusion interval
  float* search_data = search_block->data_as_float32();
  float* target_data = target_block->data_as_float32();
  memcpy(search_data + 10 * kChannels, target_data,
         20 * kChannels * sizeof(float));  // Match at 10

  // Create another exact match outside the exclusion interval
  memcpy(search_data + 50 * kChannels, target_data,
         20 * kChannels * sizeof(float));  // Match at 50

  Interval exclude_interval = {5, 15};  // Exclude indices from 5 to 15

  int optimal_index =
      OptimalIndex(search_block.get(), target_block.get(),
                   kSbMediaAudioFrameStorageTypeInterleaved, exclude_interval,
                   /*enable_optimized=*/true);

  // The match at 10 should be excluded, so it should find the match at 50.
  EXPECT_EQ(optimal_index, 50);
}

TEST(WsolaInternalTest, OptimalIndex_SmallBlocks) {
  constexpr int kFrames = 20;
  constexpr int kChannels = 1;

  scoped_refptr<DecodedAudio> target_block =
      CreateTestDecodedAudio(5, kChannels, kSampleRate, 440.0f, 0.5f);
  scoped_refptr<DecodedAudio> search_block =
      CreateTestDecodedAudio(kFrames, kChannels, kSampleRate, 440.0f, 0.5f);

  float* search_data = search_block->data_as_float32();
  float* target_data = target_block->data_as_float32();
  memcpy(search_data + 10 * kChannels, target_data,
         5 * kChannels * sizeof(float));

  Interval exclude_interval = {-1, -1};

  int optimal_index =
      OptimalIndex(search_block.get(), target_block.get(),
                   kSbMediaAudioFrameStorageTypeInterleaved, exclude_interval,
                   /*enable_optimized=*/true);
  EXPECT_NEAR(optimal_index, 10, 1);
}

TEST(WsolaInternalTest, GetPeriodicHanningWindow_LengthOne) {
  float window[1];
  GetPeriodicHanningWindow(1, window);
  EXPECT_FLOAT_EQ(window[0],
                  0.0f);  // Hann window of length L is 0 at n=0 for periodic
}

TEST(WsolaInternalTest, GetPeriodicHanningWindow_LengthTwo) {
  float window[2];

  GetPeriodicHanningWindow(2, window);

  EXPECT_FLOAT_EQ(window[0], 0.0f);
  EXPECT_FLOAT_EQ(window[1], 1.0f);
}

TEST(WsolaInternalTest, GetPeriodicHanningWindow_AllValuesAreValid) {
  constexpr int kWindowLength = 64;
  std::vector<float> window(kWindowLength);

  GetPeriodicHanningWindow(kWindowLength, window.data());

  for (int i = 0; i < kWindowLength; ++i) {
    EXPECT_GE(window[i], 0.0f);
    EXPECT_LE(window[i], 1.0f);
  }
}

TEST(WsolaInternalTest, ExtensiveRandomTesting) {
  // Seed random number generator with a fixed seed for reproducibility
  std::mt19937 gen(42);
  std::uniform_real_distribution<float> dis(-1.0f, 1.0f);

  constexpr int kSampleRates[] = {22050, 44100, 48000};
  constexpr int kChannelCounts[] = {1, 2, 6};  // Mono, Stereo, 5.1

  for (int channels : kChannelCounts) {
    for (int sample_rate : kSampleRates) {
      // Create random target block (size 120 frames)
      int target_frames = 120;
      std::vector<float> target_data(target_frames * channels);
      for (float& val : target_data) {
        val = dis(gen);
      }
      scoped_refptr<DecodedAudio> target_block =
          CreateTestDecodedAudioWithData(target_data, channels);

      // Create random search block (size 300 frames)
      int search_frames = 300;
      std::vector<float> search_data(search_frames * channels);
      for (float& val : search_data) {
        val = dis(gen);
      }
      // Inject target block at a random position
      std::uniform_int_distribution<int> int_dis(
          0, search_frames - target_frames - 1);
      int inject_index = int_dis(gen);
      memcpy(search_data.data() + inject_index * channels, target_data.data(),
             target_frames * channels * sizeof(float));

      scoped_refptr<DecodedAudio> search_block =
          CreateTestDecodedAudioWithData(search_data, channels);

      Interval exclude_interval = {-1, -1};

      // 1. Run Reference C implementation
      int ref_index = ReferenceOptimalIndex(
          search_block.get(), target_block.get(), exclude_interval,
          /*enable_optimized=*/true);

      // 2. Run New Optimized Path
      int optimized_index = OptimalIndex(
          search_block.get(), target_block.get(),
          kSbMediaAudioFrameStorageTypeInterleaved, exclude_interval,
          /*enable_optimized=*/true);

      // New optimized path MUST match Reference C implementation for all
      // channels!
      EXPECT_EQ(optimized_index, ref_index)
          << "Mismatch in optimized path for channels=" << channels
          << ", sample_rate=" << sample_rate;

      // 3. Run Old Path
      int old_index = OptimalIndex(search_block.get(), target_block.get(),
                                   kSbMediaAudioFrameStorageTypeInterleaved,
                                   exclude_interval,
                                   /*enable_optimized=*/false);

      if (channels == 1) {
        // For Mono, Old Path should ALSO match Reference (with
        // enable_optimized=false)
        int ref_index_old = ReferenceOptimalIndex(
            search_block.get(), target_block.get(), exclude_interval,
            /*enable_optimized=*/false);
        EXPECT_EQ(old_index, ref_index_old)
            << "Mismatch in old path for Mono, sample_rate=" << sample_rate;
      } else {
        // For Multi-channel, we just log if they differ (since it's expected to
        // be buggy on SIMD)
        if (old_index != ref_index) {
          SB_LOG(INFO) << "Old path differed from Reference for channels="
                       << channels << " (Old: " << old_index
                       << ", Ref: " << ref_index << ")";
        }
      }
    }
  }
}

}  // namespace

}  // namespace starboard
