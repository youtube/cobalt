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
#include "starboard/benchmark/media/audio_sink/audio_sink_benchmark_helper.h"
#include "starboard/media.h"
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"

namespace starboard {
namespace {

// Measures the first SbAudioSinkCreate() call in the process, which can be
// slower than later ones (e.g. due to lazy platform initialization). This
// mirrors the Cobalt.Media.LatencyTiming.SbAudioSinkCreate UMA, which records
// the first call separately from later ones (...SbAudioSinkCreate.Subsequent).
// BM_AudioSinkCreate below measures later calls.
//
// Only one sample can be taken per process, so run the benchmark binary
// repeatedly for more samples.
void BM_AudioSinkCreateFirstCall(::benchmark::State& state) {
  if (HasCreatedAudioSink()) {
    state.SkipWithMessage(
        "An SbAudioSink was already created in this process; run with "
        "--benchmark_filter=BM_AudioSinkCreateFirstCall");
    return;
  }

  // Stereo is what WebAudio creates through CobaltAudioRendererSink.
  const AudioSinkBenchmarkConfig config{/*channels=*/2,
                                        kSbMediaAudioSampleTypeFloat32,
                                        /*sampling_frequency_hz=*/48'000};
  AudioSinkConfigSupport support = GetAudioSinkConfigSupport(config);
  if (support != AudioSinkConfigSupport::kSupported) {
    state.SkipWithMessage(ToString(support));
    return;
  }

  AudioSinkBenchmarkFrameBuffers frame_buffers(config);

  for (auto _ : state) {
    double elapsed_seconds = 0;
    SbAudioSink audio_sink =
        TimedAudioSinkCreate(config, frame_buffers, &elapsed_seconds);

    if (!SbAudioSinkIsValid(audio_sink)) {
      state.SkipWithError("SbAudioSinkCreate failed");
      break;
    }

    state.SetIterationTime(elapsed_seconds);

    // Not timed. SbAudioSinkDestroy() joins the sink's audio thread, which can
    // cost more than the creation measured above.
    SbAudioSinkDestroy(audio_sink);
  }
}
// Registered before the other benchmarks in this file so that, without a
// filter, it runs before they create any sink.
BENCHMARK(BM_AudioSinkCreateFirstCall)
    ->UseManualTime()
    ->Unit(::benchmark::kMicrosecond)
    ->Iterations(1);

void BM_AudioSinkCreate(::benchmark::State& state,
                        AudioSinkBenchmarkConfig config) {
  AudioSinkConfigSupport support = GetAudioSinkConfigSupport(config);
  if (support != AudioSinkConfigSupport::kSupported) {
    state.SkipWithMessage(ToString(support));
    return;
  }

  AudioSinkBenchmarkFrameBuffers frame_buffers(config);

  for (auto _ : state) {
    double elapsed_seconds = 0;
    SbAudioSink audio_sink =
        TimedAudioSinkCreate(config, frame_buffers, &elapsed_seconds);

    if (!SbAudioSinkIsValid(audio_sink)) {
      state.SkipWithError("SbAudioSinkCreate failed");
      break;
    }

    state.SetIterationTime(elapsed_seconds);

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
      ->Iterations(20)

// Stereo is what WebAudio creates through CobaltAudioRendererSink; mono and
// 5.1 cover the other channel layouts platforms commonly support.
AUDIO_SINK_CREATE_BENCHMARK(Mono_Int16_44100,
                            /*channels=*/1,
                            kSbMediaAudioSampleTypeInt16Deprecated,
                            /*frequency=*/44'100);
AUDIO_SINK_CREATE_BENCHMARK(Mono_Int16_48000,
                            /*channels=*/1,
                            kSbMediaAudioSampleTypeInt16Deprecated,
                            /*frequency=*/48'000);
AUDIO_SINK_CREATE_BENCHMARK(Mono_Float32_44100,
                            /*channels=*/1,
                            kSbMediaAudioSampleTypeFloat32,
                            /*frequency=*/44'100);
AUDIO_SINK_CREATE_BENCHMARK(Mono_Float32_48000,
                            /*channels=*/1,
                            kSbMediaAudioSampleTypeFloat32,
                            /*frequency=*/48'000);

AUDIO_SINK_CREATE_BENCHMARK(Stereo_Int16_44100,
                            /*channels=*/2,
                            kSbMediaAudioSampleTypeInt16Deprecated,
                            /*frequency=*/44'100);
AUDIO_SINK_CREATE_BENCHMARK(Stereo_Int16_48000,
                            /*channels=*/2,
                            kSbMediaAudioSampleTypeInt16Deprecated,
                            /*frequency=*/48'000);
AUDIO_SINK_CREATE_BENCHMARK(Stereo_Float32_44100,
                            /*channels=*/2,
                            kSbMediaAudioSampleTypeFloat32,
                            /*frequency=*/44'100);
AUDIO_SINK_CREATE_BENCHMARK(Stereo_Float32_48000,
                            /*channels=*/2,
                            kSbMediaAudioSampleTypeFloat32,
                            /*frequency=*/48'000);

AUDIO_SINK_CREATE_BENCHMARK(FiveOne_Int16_44100,
                            /*channels=*/6,
                            kSbMediaAudioSampleTypeInt16Deprecated,
                            /*frequency=*/44'100);
AUDIO_SINK_CREATE_BENCHMARK(FiveOne_Int16_48000,
                            /*channels=*/6,
                            kSbMediaAudioSampleTypeInt16Deprecated,
                            /*frequency=*/48'000);
AUDIO_SINK_CREATE_BENCHMARK(FiveOne_Float32_44100,
                            /*channels=*/6,
                            kSbMediaAudioSampleTypeFloat32,
                            /*frequency=*/44'100);
AUDIO_SINK_CREATE_BENCHMARK(FiveOne_Float32_48000,
                            /*channels=*/6,
                            kSbMediaAudioSampleTypeFloat32,
                            /*frequency=*/48'000);

#undef AUDIO_SINK_CREATE_BENCHMARK

}  // namespace
}  // namespace starboard
