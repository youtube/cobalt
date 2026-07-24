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

#include "starboard/common/ref_counted.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/player/decoded_audio_internal.h"
#include "starboard/shared/starboard/player/filter/audio_time_stretcher.h"
#include "starboard/shared/starboard/player/filter/testing/test_util.h"
#include "starboard/shared/starboard/player/filter/wsola_internal.h"
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"

namespace starboard {
namespace {

void RunOptimalIndexBenchmark(::benchmark::State& state,
                              int channels,
                              bool enable_optimized) {
  constexpr int kSampleRate = 48000;
  constexpr int kTargetFrames = 960;
  constexpr int kSearchFrames = 2400;

  scoped_refptr<DecodedAudio> target_block;
  scoped_refptr<DecodedAudio> search_block;

  if (channels == 2) {
    target_block = CreateTestStereoDecodedAudio(kTargetFrames, kSampleRate,
                                                440.0f, 0.5f, 880.0f, 0.5f);
    search_block = CreateTestStereoDecodedAudio(kSearchFrames, kSampleRate,
                                                440.0f, 0.5f, 880.0f, 0.5f);
  } else {
    target_block = CreateTestDecodedAudio(kTargetFrames, channels, kSampleRate,
                                          440.0f, 0.5f);
    search_block = CreateTestDecodedAudio(kSearchFrames, channels, kSampleRate,
                                          440.0f, 0.5f);
  }

  float* search_data = search_block->data_as_float32();
  const float* target_data = target_block->data_as_float32();
  constexpr int kInjectIndex = 400;
  memcpy(search_data + kInjectIndex * channels, target_data,
         kTargetFrames * channels * sizeof(float));

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
  constexpr int kSampleRate = 48000;
  constexpr double kPlaybackRate = 1.5;
  constexpr int kInputFrames = 4800;
  constexpr int kRequestedFrames = 2400;

  scoped_refptr<DecodedAudio> input;
  if (channels == 2) {
    input = CreateTestStereoDecodedAudio(kInputFrames, kSampleRate, 440.0f,
                                         0.5f, 880.0f, 0.5f);
  } else {
    input = CreateTestDecodedAudio(kInputFrames, channels, kSampleRate, 440.0f,
                                   0.5f);
  }

  AudioTimeStretcher stretcher;
  stretcher.Initialize(kSbMediaAudioSampleTypeFloat32, channels, kSampleRate,
                       enable_optimized);

  for (auto _ : state) {
    state.PauseTiming();
    stretcher.FlushBuffers();
    stretcher.EnqueueBuffer(input);
    state.ResumeTiming();

    scoped_refptr<DecodedAudio> output =
        stretcher.Read(kRequestedFrames, kPlaybackRate);
    ::benchmark::DoNotOptimize(output);
  }
}

}  // namespace
}  // namespace starboard

void BM_OptimalIndex_Mono_Old(::benchmark::State& state) {
  starboard::RunOptimalIndexBenchmark(state, 1, /*enable_optimized=*/false);
}
BENCHMARK(BM_OptimalIndex_Mono_Old);

void BM_OptimalIndex_Mono_New(::benchmark::State& state) {
  starboard::RunOptimalIndexBenchmark(state, 1, /*enable_optimized=*/true);
}
BENCHMARK(BM_OptimalIndex_Mono_New);

void BM_OptimalIndex_Stereo_Old(::benchmark::State& state) {
  starboard::RunOptimalIndexBenchmark(state, 2, /*enable_optimized=*/false);
}
BENCHMARK(BM_OptimalIndex_Stereo_Old);

void BM_OptimalIndex_Stereo_New(::benchmark::State& state) {
  starboard::RunOptimalIndexBenchmark(state, 2, /*enable_optimized=*/true);
}
BENCHMARK(BM_OptimalIndex_Stereo_New);

void BM_OptimalIndex_5_1_Old(::benchmark::State& state) {
  starboard::RunOptimalIndexBenchmark(state, 6, /*enable_optimized=*/false);
}
BENCHMARK(BM_OptimalIndex_5_1_Old);

void BM_OptimalIndex_5_1_New(::benchmark::State& state) {
  starboard::RunOptimalIndexBenchmark(state, 6, /*enable_optimized=*/true);
}
BENCHMARK(BM_OptimalIndex_5_1_New);

void BM_AudioTimeStretcher_Mono_Old(::benchmark::State& state) {
  starboard::RunAudioTimeStretcherBenchmark(state, 1,
                                            /*enable_optimized=*/false);
}
BENCHMARK(BM_AudioTimeStretcher_Mono_Old);

void BM_AudioTimeStretcher_Mono_New(::benchmark::State& state) {
  starboard::RunAudioTimeStretcherBenchmark(state, 1,
                                            /*enable_optimized=*/true);
}
BENCHMARK(BM_AudioTimeStretcher_Mono_New);

void BM_AudioTimeStretcher_Stereo_Old(::benchmark::State& state) {
  starboard::RunAudioTimeStretcherBenchmark(state, 2,
                                            /*enable_optimized=*/false);
}
BENCHMARK(BM_AudioTimeStretcher_Stereo_Old);

void BM_AudioTimeStretcher_Stereo_New(::benchmark::State& state) {
  starboard::RunAudioTimeStretcherBenchmark(state, 2,
                                            /*enable_optimized=*/true);
}
BENCHMARK(BM_AudioTimeStretcher_Stereo_New);
