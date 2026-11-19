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

// Only guards against a sink whose audio thread never runs. On Android the
// wait takes a few milliseconds, but PulseAudio on Linux only calls back once
// the stream is ready, which took up to ~7 s after BM_AudioSinkCreate's rapid
// create/destroy cycles.
constexpr std::chrono::seconds kAudioThreadStartTimeout{10};

void BM_AudioSinkDestroy(::benchmark::State& state,
                         AudioSinkBenchmarkConfig config) {
  AudioSinkConfigSupport support = GetAudioSinkConfigSupport(config);
  if (support != AudioSinkConfigSupport::kSupported) {
    state.SkipWithMessage(ToString(support));
    return;
  }

  AudioSinkBenchmarkFrameBuffers frame_buffers(config);

  for (auto _ : state) {
    // 1. Create the sink and wait until its audio thread is running (not
    //    timed). SbAudioSinkDestroy() joins the audio thread, so destroying a
    //    sink whose thread has not started yet would also measure the thread
    //    startup instead of only the teardown.
    AudioSinkBenchmarkThreadWaiter waiter;
    SbAudioSink audio_sink = SbAudioSinkCreate(
        config.channels, config.sampling_frequency_hz, config.sample_type,
        kSbMediaAudioFrameStorageTypeInterleaved, frame_buffers.frame_buffers(),
        frame_buffers.frames_per_channel(),
        AudioSinkBenchmarkThreadWaiter::UpdateSourceStatusFunc,
        AudioSinkBenchmarkConsumeFramesFunc, &waiter);

    if (!SbAudioSinkIsValid(audio_sink)) {
      state.SkipWithError("SbAudioSinkCreate failed");
      break;
    }

    if (!waiter.WaitForAudioThread(kAudioThreadStartTimeout)) {
      state.SkipWithError("SbAudioSink audio thread did not start");
      SbAudioSinkDestroy(audio_sink);
      break;
    }

    // 2. Measure only the destruction. This includes the time the audio thread
    //    takes to notice the request to quit, which real callers pay too (e.g.
    //    up to 10 ms per pass while idle on Android).
    auto start_time = std::chrono::steady_clock::now();
    SbAudioSinkDestroy(audio_sink);
    auto end_time = std::chrono::steady_clock::now();

    std::chrono::duration<double> elapsed = end_time - start_time;
    state.SetIterationTime(elapsed.count());
  }
}

// Explicit iteration counts bound total runtime and provide a uniform 20-sample
// run across platforms, since every iteration also runs the untimed
// SbAudioSinkCreate() and audio thread startup.
#define AUDIO_SINK_DESTROY_BENCHMARK(name, channels, sample_type, frequency) \
  BENCHMARK_CAPTURE(                                                         \
      BM_AudioSinkDestroy, name,                                             \
      (AudioSinkBenchmarkConfig{channels, sample_type, frequency}))          \
      ->UseManualTime()                                                      \
      ->Unit(::benchmark::kMicrosecond)                                      \
      ->Iterations(20);

AUDIO_SINK_BENCHMARK_CONFIGS(AUDIO_SINK_DESTROY_BENCHMARK)

#undef AUDIO_SINK_DESTROY_BENCHMARK

}  // namespace
}  // namespace starboard
