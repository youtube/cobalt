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
#include <vector>

#include "starboard/media.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"

namespace starboard {
namespace {

constexpr int kSampleRate = 48'000;
constexpr float kFrequency = 440.0f;

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

  float* data = audio->data_as_float32();
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

TEST(WsolaInternalTest, OptimalIndex_ExactMatch) {
  const int kFrames = 100;
  const int kChannels = 1;
  const float kAmplitude = 0.5f;

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
                   kSbMediaAudioFrameStorageTypeInterleaved, exclude_interval);
  // Due to floating point precision and quadratic interpolation, it might not
  // be exactly 25, but should be very close. We test for a small range.
  EXPECT_NEAR(optimal_index, 25, 1);
}

TEST(WsolaInternalTest, OptimalIndex_NoMatch) {
  const int kFrames = 100;
  const int kChannels = 1;

  scoped_refptr<DecodedAudio> target_block =
      CreateTestDecodedAudio(50, kChannels, kSampleRate, 440.0f, 0.5f);
  scoped_refptr<DecodedAudio> search_block = CreateTestDecodedAudio(
      kFrames, kChannels, kSampleRate, 1000.0f, 0.5f);  // Different frequency

  Interval exclude_interval = {-1, -1};  // No exclusion

  int optimal_index =
      OptimalIndex(search_block.get(), target_block.get(),
                   kSbMediaAudioFrameStorageTypeInterleaved, exclude_interval);
  // With no clear match, the result is less predictable, but it shouldn't crash
  // and should return a valid index within the search range.
  EXPECT_GE(optimal_index, 0);
  EXPECT_LE(optimal_index, kFrames - target_block->frames());
}

TEST(WsolaInternalTest, OptimalIndex_ExcludeInterval) {
  const int kFrames = 100;
  const int kChannels = 1;
  const float kAmplitude = 0.5f;

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
                   kSbMediaAudioFrameStorageTypeInterleaved, exclude_interval);

  // The match at 10 should be excluded, so it should find the match at 50.
  EXPECT_EQ(optimal_index, 50);
}

TEST(WsolaInternalTest, OptimalIndex_SmallBlocks) {
  const int kFrames = 20;
  const int kChannels = 1;

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
                   kSbMediaAudioFrameStorageTypeInterleaved, exclude_interval);
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
  const int kWindowLength = 64;
  std::vector<float> window(kWindowLength);

  GetPeriodicHanningWindow(kWindowLength, window.data());

  for (int i = 0; i < kWindowLength; ++i) {
    EXPECT_GE(window[i], 0.0f);
    EXPECT_LE(window[i], 1.0f);
  }
}

}  // namespace

}  // namespace starboard
