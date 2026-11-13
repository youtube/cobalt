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

#include "starboard/audio_sink.h"
#include "starboard/benchmark/audio_sink/audio_sink_benchmark_helper.h"
#include "starboard/media.h"
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"

namespace starboard {
namespace {

// Callers typically call SbAudioSinkGetMinBufferSizeInFrames() before
// SbAudioSinkCreate() to size the frame buffer, so its cost adds to the time it
// takes to start audio.
void BM_AudioSinkGetMinBufferSizeInFrames(::benchmark::State& state,
                                          AudioSinkBenchmarkConfig config) {
  AudioSinkConfigSupport support = GetAudioSinkConfigSupport(config);
  if (support != AudioSinkConfigSupport::kSupported) {
    state.SkipWithMessage(ToString(support));
    return;
  }

  for (auto _ : state) {
    int frames = SbAudioSinkGetMinBufferSizeInFrames(
        config.channels, config.sample_type, config.sampling_frequency_hz);
    ::benchmark::DoNotOptimize(frames);
  }
}

// The call has no untimed setup, so Google Benchmark picks the iteration count.
// Use wall-clock time, since callers block for the whole call while most of it
// can be spent waiting rather than on the CPU (e.g. a JNI call on Android).
#define AUDIO_SINK_GET_MIN_BUFFER_SIZE_BENCHMARK(name, channels, sample_type, \
                                                 frequency)                   \
  BENCHMARK_CAPTURE(                                                          \
      BM_AudioSinkGetMinBufferSizeInFrames, name,                             \
      (AudioSinkBenchmarkConfig{channels, sample_type, frequency}))           \
      ->UseRealTime()                                                         \
      ->Unit(::benchmark::kMicrosecond);

AUDIO_SINK_BENCHMARK_CONFIGS(AUDIO_SINK_GET_MIN_BUFFER_SIZE_BENCHMARK)

#undef AUDIO_SINK_GET_MIN_BUFFER_SIZE_BENCHMARK

}  // namespace
}  // namespace starboard
