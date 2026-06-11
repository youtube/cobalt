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

#include <cmath>
#include <cstring>

#include "starboard/common/log.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"
#include "starboard/shared/starboard/player/filter/audio_time_stretcher.h"
#include "starboard/shared/starboard/player/filter/wsola_internal.h"
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"

namespace starboard {
namespace {

// Define kPi because M_PI isn't standard POSIX.
constexpr float kPi = 3.1415926535f;

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
      data[i * channels + c] =
          amplitude * std::sin(2.0f * kPi * frequency * i / sample_rate);
    }
  }
  return audio;
}

void RunOptimalIndexBenchmark(::benchmark::State& state,
                              int channels,
                              bool enable_optimized) {
  const int kSampleRate = 48000;
  int target_frames = 960;
  int search_frames = 2400;

  scoped_refptr<DecodedAudio> target_block;
  scoped_refptr<DecodedAudio> search_block;

  if (channels == 1) {
    target_block =
        CreateTestDecodedAudio(target_frames, 1, kSampleRate, 440.0f, 0.5f);
    search_block =
        CreateTestDecodedAudio(search_frames, 1, kSampleRate, 440.0f, 0.5f);
  } else if (channels == 2) {
    target_block = CreateTestStereoDecodedAudio(target_frames, kSampleRate,
                                                440.0f, 0.5f, 880.0f, 0.5f);
    search_block = CreateTestStereoDecodedAudio(search_frames, kSampleRate,
                                                440.0f, 0.5f, 880.0f, 0.5f);
  } else {
    target_block = CreateTestDecodedAudio(target_frames, channels, kSampleRate,
                                          440.0f, 0.5f);
    search_block = CreateTestDecodedAudio(search_frames, channels, kSampleRate,
                                          440.0f, 0.5f);
  }

  float* search_data = search_block->data_as_float32();
  const float* target_data = target_block->data_as_float32();
  int inject_index = 400;
  memcpy(search_data + inject_index * channels, target_data,
         target_frames * channels * sizeof(float));

  Interval exclude_interval = {-1, -1};

  for (auto _ : state) {
    int index = OptimalIndex(search_block.get(), target_block.get(),
                             kSbMediaAudioFrameStorageTypeInterleaved,
                             exclude_interval, enable_optimized);
    ::benchmark::DoNotOptimize(index);
  }
}

void RunAudioTimeStretcherBenchmark(::benchmark::State& state,
                                    int channels,
                                    bool enable_optimized) {
  const int kSampleRate = 48000;
  const double kPlaybackRate = 1.5;
  const int kInputFrames = 4800;
  const int kRequestedFrames = 2400;

  scoped_refptr<DecodedAudio> input;
  if (channels == 1) {
    input = CreateTestDecodedAudio(kInputFrames, 1, kSampleRate, 440.0f, 0.5f);
  } else if (channels == 2) {
    input = CreateTestStereoDecodedAudio(kInputFrames, kSampleRate, 440.0f,
                                         0.5f, 880.0f, 0.5f);
  } else {
    input = CreateTestDecodedAudio(kInputFrames, channels, kSampleRate, 440.0f,
                                   0.5f);
  }

  for (auto _ : state) {
    state.PauseTiming();
    AudioTimeStretcher stretcher;
    stretcher.Initialize(kSbMediaAudioSampleTypeFloat32, channels, kSampleRate,
                         enable_optimized);
    stretcher.EnqueueBuffer(input);
    state.ResumeTiming();

    auto output = stretcher.Read(kRequestedFrames, kPlaybackRate);
    ::benchmark::DoNotOptimize(output);
  }
}

}  // namespace
}  // namespace starboard

void BM_OptimalIndex_Mono_Old(::benchmark::State& state) {
  starboard::RunOptimalIndexBenchmark(state, 1, false);
}
BENCHMARK(BM_OptimalIndex_Mono_Old);

void BM_OptimalIndex_Mono_New(::benchmark::State& state) {
  starboard::RunOptimalIndexBenchmark(state, 1, true);
}
BENCHMARK(BM_OptimalIndex_Mono_New);

void BM_OptimalIndex_Stereo_Old(::benchmark::State& state) {
  starboard::RunOptimalIndexBenchmark(state, 2, false);
}
BENCHMARK(BM_OptimalIndex_Stereo_Old);

void BM_OptimalIndex_Stereo_New(::benchmark::State& state) {
  starboard::RunOptimalIndexBenchmark(state, 2, true);
}
BENCHMARK(BM_OptimalIndex_Stereo_New);

void BM_OptimalIndex_5_1_Old(::benchmark::State& state) {
  starboard::RunOptimalIndexBenchmark(state, 6, false);
}
BENCHMARK(BM_OptimalIndex_5_1_Old);

void BM_OptimalIndex_5_1_New(::benchmark::State& state) {
  starboard::RunOptimalIndexBenchmark(state, 6, true);
}
BENCHMARK(BM_OptimalIndex_5_1_New);

void BM_AudioTimeStretcher_Mono_Old(::benchmark::State& state) {
  starboard::RunAudioTimeStretcherBenchmark(state, 1, false);
}
BENCHMARK(BM_AudioTimeStretcher_Mono_Old);

void BM_AudioTimeStretcher_Mono_New(::benchmark::State& state) {
  starboard::RunAudioTimeStretcherBenchmark(state, 1, true);
}
BENCHMARK(BM_AudioTimeStretcher_Mono_New);

void BM_AudioTimeStretcher_Stereo_Old(::benchmark::State& state) {
  starboard::RunAudioTimeStretcherBenchmark(state, 2, false);
}
BENCHMARK(BM_AudioTimeStretcher_Stereo_Old);

void BM_AudioTimeStretcher_Stereo_New(::benchmark::State& state) {
  starboard::RunAudioTimeStretcherBenchmark(state, 2, true);
}
BENCHMARK(BM_AudioTimeStretcher_Stereo_New);
