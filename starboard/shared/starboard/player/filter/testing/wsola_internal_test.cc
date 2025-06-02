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
#include <vector>

#include "starboard/media.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"
#include "starboard/types.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {
namespace internal {
namespace {

// A helper function to create a simple DecodedAudio object for testing.
// It allocates memory and fills it with a simple sine wave.
scoped_refptr<DecodedAudio> CreateTestDecodedAudio(int frames,
                                                   int channels,
                                                   int sample_rate,
                                                   float frequency,
                                                   float amplitude) {
  const int kBytesPerFrame = channels * sizeof(float);
  const int kBufferSize = frames * kBytesPerFrame;

  scoped_refptr<DecodedAudio> audio = new DecodedAudio(
      channels, kSbMediaAudioSampleTypeFloat32,
      kSbMediaAudioFrameStorageTypeInterleaved, 0, kBufferSize);

  float* data = reinterpret_cast<float*>(audio->data());
  for (int i = 0; i < frames; ++i) {
    for (int c = 0; c < channels; ++c) {
      data[i * channels + c] =
          amplitude * sinf(2.0f * M_PI * frequency * i / sample_rate);
    }
  }
  return audio;
}

// A helper to create a DecodedAudio object with specific data.
scoped_refptr<DecodedAudio> CreateTestDecodedAudioWithData(
    const std::vector<float>& data_values,
    int channels) {
  int frames = data_values.size() / channels;
  const int kBytesPerFrame = channels * sizeof(float);
  const int kBufferSize = frames * kBytesPerFrame;

  scoped_refptr<DecodedAudio> audio = new DecodedAudio(
      channels, kSbMediaAudioSampleTypeFloat32,
      kSbMediaAudioFrameStorageTypeInterleaved, 0, kBufferSize);

  memcpy(audio->data(), data_values.data(), kBufferSize);
  return audio;
}

// ---
// ## OptimalIndex Tests

TEST(WsolaInternalTest, OptimalIndex_ExactMatch) {
  const int kFrames = 100;
  const int kChannels = 1;
  const int kSampleRate = 48000;
  const float kFrequency = 440.0f;
  const float kAmplitude = 0.5f;

  scoped_refptr<DecodedAudio> target_block = CreateTestDecodedAudio(
      50, kChannels, kSampleRate, kFrequency, kAmplitude);
  scoped_refptr<DecodedAudio> search_block = CreateTestDecodedAudio(
      kFrames, kChannels, kSampleRate, kFrequency, kAmplitude);

  // Place an exact match at index 25
  float* search_data = reinterpret_cast<float*>(search_block->data());
  float* target_data = reinterpret_cast<float*>(target_block->data());
  memcpy(search_data + 25 * kChannels, target_data,
         50 * kChannels * sizeof(float));

  Interval exclude_interval = {-1, -1};  // No exclusion

  int optimal_index =
      OptimalIndex(search_block, target_block,
                   kSbMediaAudioFrameStorageTypeInterleaved, exclude_interval);
  // Due to floating point precision and quadratic interpolation, it might not
  // be exactly 25, but should be very close. We test for a small range.
  EXPECT_NEAR(optimal_index, 25, 1);
}

TEST(WsolaInternalTest, OptimalIndex_NoMatch) {
  const int kFrames = 100;
  const int kChannels = 1;
  const int kSampleRate = 48000;

  scoped_refptr<DecodedAudio> target_block =
      CreateTestDecodedAudio(50, kChannels, kSampleRate, 440.0f, 0.5f);
  scoped_refptr<DecodedAudio> search_block = CreateTestDecodedAudio(
      kFrames, kChannels, kSampleRate, 1000.0f, 0.5f);  // Different frequency

  Interval exclude_interval = {-1, -1};  // No exclusion

  int optimal_index =
      OptimalIndex(search_block, target_block,
                   kSbMediaAudioFrameStorageTypeInterleaved, exclude_interval);
  // With no clear match, the result is less predictable, but it shouldn't crash
  // and should return a valid index within the search range.
  EXPECT_GE(optimal_index, 0);
  EXPECT_LE(optimal_index, kFrames - target_block->frames());
}

TEST(WsolaInternalTest, OptimalIndex_ExcludeInterval) {
  const int kFrames = 100;
  const int kChannels = 1;
  const int kSampleRate = 48000;
  const float kFrequency = 440.0f;
  const float kAmplitude = 0.5f;

  scoped_refptr<DecodedAudio> target_block = CreateTestDecodedAudio(
      20, kChannels, kSampleRate, kFrequency, kAmplitude);
  scoped_refptr<DecodedAudio> search_block = CreateTestDecodedAudio(
      kFrames, kChannels, kSampleRate, kFrequency, kAmplitude);

  // Create an exact match within the exclusion interval
  float* search_data = reinterpret_cast<float*>(search_block->data());
  float* target_data = reinterpret_cast<float*>(target_block->data());
  memcpy(search_data + 10 * kChannels, target_data,
         20 * kChannels * sizeof(float));  // Match at 10

  // Create another exact match outside the exclusion interval
  memcpy(search_data + 50 * kChannels, target_data,
         20 * kChannels * sizeof(float));  // Match at 50

  Interval exclude_interval = {5, 15};  // Exclude indices from 5 to 15

  int optimal_index =
      OptimalIndex(search_block, target_block,
                   kSbMediaAudioFrameStorageTypeInterleaved, exclude_interval);
  // The match at 10 should be excluded, so it should find the match at 50.
  EXPECT_NEAR(optimal_index, 50, 1);
}

TEST(WsolaInternalTest, OptimalIndex_MultiChannel) {
  const int kFrames = 100;
  const int kChannels = 2;  // Stereo
  const int kSampleRate = 48000;
  const float kFrequency = 440.0f;
  const float kAmplitude = 0.5f;

  scoped_refptr<DecodedAudio> target_block = CreateTestDecodedAudio(
      50, kChannels, kSampleRate, kFrequency, kAmplitude);
  scoped_refptr<DecodedAudio> search_block = CreateTestDecodedAudio(
      kFrames, kChannels, kSampleRate, kFrequency, kAmplitude);

  // Place an exact match at index 25
  float* search_data = reinterpret_cast<float*>(search_block->data());
  float* target_data = reinterpret_cast<float*>(target_block->data());
  memcpy(search_data + 25 * kChannels, target_data,
         50 * kChannels * sizeof(float));

  Interval exclude_interval = {-1, -1};  // No exclusion

  int optimal_index =
      OptimalIndex(search_block, target_block,
                   kSbMediaAudioFrameStorageTypeInterleaved, exclude_interval);
  EXPECT_NEAR(optimal_index, 25, 1);
}

TEST(WsolaInternalTest, OptimalIndex_SmallBlocks) {
  const int kFrames = 20;
  const int kChannels = 1;
  const int kSampleRate = 48000;

  scoped_refptr<DecodedAudio> target_block =
      CreateTestDecodedAudio(5, kChannels, kSampleRate, 440.0f, 0.5f);
  scoped_refptr<DecodedAudio> search_block =
      CreateTestDecodedAudio(kFrames, kChannels, kSampleRate, 440.0f, 0.5f);

  float* search_data = reinterpret_cast<float*>(search_block->data());
  float* target_data = reinterpret_cast<float*>(target_block->data());
  memcpy(search_data + 10 * kChannels, target_data,
         5 * kChannels * sizeof(float));

  Interval exclude_interval = {-1, -1};

  int optimal_index =
      OptimalIndex(search_block, target_block,
                   kSbMediaAudioFrameStorageTypeInterleaved, exclude_interval);
  EXPECT_NEAR(optimal_index, 10, 1);
}

// ---
// ## GetSymmetricHanningWindow Tests

TEST(WsolaInternalTest, GetSymmetricHanningWindow_LengthOne) {
  float window[1];
  GetSymmetricHanningWindow(1, window);
  EXPECT_FLOAT_EQ(window[0],
                  0.0f);  // Hann window of length L is 0 at n=0 for periodic
}

TEST(WsolaInternalTest, GetSymmetricHanningWindow_LengthTwo) {
  float window[2];
  GetSymmetricHanningWindow(2, window);
  // For L=2, window[n] = 0.5 * (1 - cos(n * 2pi / 2)) = 0.5 * (1 - cos(n * pi))
  // n=0: 0.5 * (1 - cos(0)) = 0.5 * (1 - 1) = 0
  // n=1: 0.5 * (1 - cos(pi)) = 0.5 * (1 - (-1)) = 1
  EXPECT_FLOAT_EQ(window[0], 0.0f);
  EXPECT_FLOAT_EQ(window[1], 1.0f);
}

TEST(WsolaInternalTest, GetSymmetricHanningWindow_TypicalLength) {
  const int kWindowLength = 10;
  float window[kWindowLength];
  GetSymmetricHanningWindow(kWindowLength, window);

  // Check first and last values (should be close to 0)
  EXPECT_NEAR(window[0], 0.0f, 1e-6);
  // The periodic Hann window of length L is the first L samples of an L+1 Hann
  // window. So the last sample (index L-1) should be equivalent to a full Hann
  // window at (L-1)/L. For n = L-1: 0.5 * (1 - cos((L-1) * 2pi / L))
  EXPECT_NEAR(
      window[kWindowLength - 1],
      0.5f * (1.0f - cosf((kWindowLength - 1) * 2.0f * M_PI / kWindowLength)),
      1e-6);

  // Check the middle value (should be close to 1 for even length, or near peak
  // for odd) For n = L/2: 0.5 * (1 - cos(L/2 * 2pi / L)) = 0.5 * (1 - cos(pi))
  // = 1
  EXPECT_NEAR(window[kWindowLength / 2], 1.0f, 1e-6);

  // Check for symmetry (approximately)
  for (int i = 1; i < kWindowLength / 2; ++i) {
    EXPECT_NEAR(window[i], window[kWindowLength - 1 - i], 1e-6);
  }
}

TEST(WsolaInternalTest, GetSymmetricHanningWindow_AllValuesAreValid) {
  const int kWindowLength = 64;
  std::vector<float> window(kWindowLength);
  GetSymmetricHanningWindow(kWindowLength, window.data());

  for (int i = 0; i < kWindowLength; ++i) {
    // All values should be between 0 and 1, inclusive.
    EXPECT_GE(window[i], 0.0f);
    EXPECT_LE(window[i], 1.0f);
  }
}

}  // namespace

}  // namespace internal
}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
