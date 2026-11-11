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

#include <chrono>

#include "starboard/audio_sink.h"
#include "starboard/benchmark/audio_sink/audio_sink_benchmark_helper.h"
#include "starboard/media.h"
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"

namespace starboard {
namespace {

void BM_AudioSinkCreate(::benchmark::State& state,
                        AudioSinkBenchmarkConfig config) {
  AudioSinkConfigSupport support = GetAudioSinkConfigSupport(config);
  if (support != AudioSinkConfigSupport::kSupported) {
    state.SkipWithMessage(ToString(support));
    return;
  }

  AudioSinkBenchmarkFrameBuffers frame_buffers(config);

  for (auto _ : state) {
    auto start_time = std::chrono::steady_clock::now();

    SbAudioSink audio_sink = SbAudioSinkCreate(
        config.channels, config.sampling_frequency_hz, config.sample_type,
        kSbMediaAudioFrameStorageTypeInterleaved, frame_buffers.frame_buffers(),
        frame_buffers.frames_per_channel(),
        AudioSinkBenchmarkUpdateSourceStatusFunc,
        AudioSinkBenchmarkConsumeFramesFunc, /*context=*/nullptr);

    auto end_time = std::chrono::steady_clock::now();

    if (!SbAudioSinkIsValid(audio_sink)) {
      state.SkipWithError("SbAudioSinkCreate failed");
      break;
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(
        end_time - start_time);
    state.SetIterationTime(elapsed.count());

    // Not timed. SbAudioSinkDestroy() joins the sink's audio thread, which can
    // cost more than the creation measured above.
    SbAudioSinkDestroy(audio_sink);
  }
}

// Explicit iteration counts bound total runtime and provide a uniform 20-sample
// run across platforms, since every iteration also runs the untimed
// SbAudioSinkDestroy().
#define AUDIO_SINK_CREATE_BENCHMARK(name, channels, sample_type, frequency) \
  BENCHMARK_CAPTURE(                                                        \
      BM_AudioSinkCreate, name,                                             \
      (AudioSinkBenchmarkConfig{channels, sample_type, frequency}))         \
      ->UseManualTime()                                                     \
      ->Unit(::benchmark::kMicrosecond)                                     \
      ->Iterations(20);

AUDIO_SINK_BENCHMARK_CONFIGS(AUDIO_SINK_CREATE_BENCHMARK)

#undef AUDIO_SINK_CREATE_BENCHMARK

}  // namespace
}  // namespace starboard
