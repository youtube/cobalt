// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/player/filter/audio_time_stretcher.h"

#include <gtest/gtest.h>

#include <cmath>

#include "starboard/media.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"

namespace starboard {
namespace {

constexpr float kFrequency = 440.0f;
// Define kPi because M_PI isn't standard POSIX.
constexpr float kPi = 3.1415926535f;

scoped_refptr<DecodedAudio> CreateTestDecodedAudio(int frames,
                                                   int channels,
                                                   int sample_rate,
                                                   float frequency,
                                                   float amplitude) {
  const int kBytesPerFrame = channels * sizeof(float);
  const int kBufferSize = frames * kBytesPerFrame;

  auto audio = make_scoped_refptr<DecodedAudio>(
      channels, kSbMediaAudioSampleTypeFloat32,
      kSbMediaAudioFrameStorageTypeInterleaved, 0, kBufferSize);

  float* data = audio->data_as_float32();
  for (int i = 0; i < frames; ++i) {
    for (int c = 0; c < channels; ++c) {
      // Synthesize standard continuous sine wave audio tones spanning
      // [-amplitude, amplitude] to verify WSOLA waveform alignment.
      data[i * channels + c] =
          amplitude * std::sin(2.0f * kPi * frequency * i / sample_rate);
    }
  }
  return audio;
}

scoped_refptr<DecodedAudio> CreateTestStereoDecodedAudio(
    int frames,
    int sample_rate,
    float left_frequency,
    float left_amplitude,
    float right_frequency,
    float right_amplitude) {
  const int kChannels = 2;
  const int kBytesPerFrame = kChannels * sizeof(float);
  const int kBufferSize = frames * kBytesPerFrame;

  auto audio = make_scoped_refptr<DecodedAudio>(
      kChannels, kSbMediaAudioSampleTypeFloat32,
      kSbMediaAudioFrameStorageTypeInterleaved, 0, kBufferSize);

  float* data = audio->data_as_float32();
  for (int i = 0; i < frames; ++i) {
    data[i * 2] = left_amplitude *
                  std::sin(2.0f * kPi * left_frequency * i / sample_rate);
    data[i * 2 + 1] = right_amplitude *
                      std::sin(2.0f * kPi * right_frequency * i / sample_rate);
  }
  return audio;
}

class AudioTimeStretcherTest : public ::testing::TestWithParam<int> {};

TEST_P(AudioTimeStretcherTest, Mono_OldAndNewMatch) {
  const int sample_rate = GetParam();
  const int kChannels = 1;
  const double kPlaybackRate = 1.5;
  const int kInputFrames = 4800;
  const int kRequestedFrames = 500;

  // Run Old Path.
  AudioTimeStretcher stretcher_old;
  stretcher_old.Initialize(kSbMediaAudioSampleTypeFloat32, kChannels,
                           sample_rate, false);
  scoped_refptr<DecodedAudio> input_old = CreateTestDecodedAudio(
      kInputFrames, kChannels, sample_rate, kFrequency, 0.5f);
  stretcher_old.EnqueueBuffer(input_old);
  scoped_refptr<DecodedAudio> output_old =
      stretcher_old.Read(kRequestedFrames, kPlaybackRate);

  // Run New Path.
  AudioTimeStretcher stretcher_new;
  stretcher_new.Initialize(kSbMediaAudioSampleTypeFloat32, kChannels,
                           sample_rate, true);
  scoped_refptr<DecodedAudio> input_new = CreateTestDecodedAudio(
      kInputFrames, kChannels, sample_rate, kFrequency, 0.5f);
  stretcher_new.EnqueueBuffer(input_new);
  scoped_refptr<DecodedAudio> output_new =
      stretcher_new.Read(kRequestedFrames, kPlaybackRate);

  ASSERT_EQ(output_old->frames(), output_new->frames());

  const float* data_old = output_old->data_as_float32();
  const float* data_new = output_new->data_as_float32();
  for (int i = 0; i < output_old->frames() * kChannels; ++i) {
    EXPECT_NEAR(data_old[i], data_new[i], 1e-5f) << "Mismatch at index " << i;
  }
}

TEST_P(AudioTimeStretcherTest, Stereo_NewPath_RunsCorrectly) {
  const int sample_rate = GetParam();
  const int kChannels = 2;
  const double kPlaybackRate = 1.5;
  const int kInputFrames = 4800;
  const int kRequestedFrames = 500;

  AudioTimeStretcher stretcher;
  stretcher.Initialize(kSbMediaAudioSampleTypeFloat32, kChannels, sample_rate,
                       true);

  scoped_refptr<DecodedAudio> input = CreateTestStereoDecodedAudio(
      kInputFrames, sample_rate, 440.0f, 0.5f, 880.0f, 0.5f);
  stretcher.EnqueueBuffer(input);

  scoped_refptr<DecodedAudio> output =
      stretcher.Read(kRequestedFrames, kPlaybackRate);

  EXPECT_GT(output->frames(), 0);
}

TEST_P(AudioTimeStretcherTest, OddInputFrames_RunsCorrectly) {
  const int sample_rate = GetParam();
  const int kChannels = 2;
  const double kPlaybackRate = 1.33;
  const int kInputFrames = 4801;
  const int kRequestedFrames = 500;

  AudioTimeStretcher stretcher;
  stretcher.Initialize(kSbMediaAudioSampleTypeFloat32, kChannels, sample_rate,
                       true);

  scoped_refptr<DecodedAudio> input = CreateTestStereoDecodedAudio(
      kInputFrames, sample_rate, 440.0f, 0.5f, 880.0f, 0.5f);
  stretcher.EnqueueBuffer(input);

  scoped_refptr<DecodedAudio> output =
      stretcher.Read(kRequestedFrames, kPlaybackRate);

  EXPECT_GT(output->frames(), 0);
}

INSTANTIATE_TEST_SUITE_P(AudioTimeStretcherSampleRates,
                         AudioTimeStretcherTest,
                         ::testing::Values(22050, 44100, 48000));

}  // namespace
}  // namespace starboard
