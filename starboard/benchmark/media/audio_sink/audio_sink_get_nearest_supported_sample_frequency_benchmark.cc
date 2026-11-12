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
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"

namespace starboard {
namespace {

// SbAudioSinkGetNearestSupportedSampleFrequency() is called each time an audio
// sink is started: by the audio renderer on playback start and after every
// seek, and by CobaltAudioRendererSink (WebAudio). Implementations may handle
// sample rates differently (e.g. clamp to a supported range), so the benchmark
// covers a range of input rates.
void BM_AudioSinkGetNearestSupportedSampleFrequency(
    ::benchmark::State& state) {
  const int sampling_frequency_hz = static_cast<int>(state.range(0));
  for (auto _ : state) {
    ::benchmark::DoNotOptimize(
        SbAudioSinkGetNearestSupportedSampleFrequency(sampling_frequency_hz));
  }
}
BENCHMARK(BM_AudioSinkGetNearestSupportedSampleFrequency)
    ->ArgName("hz")
    ->Arg(8'000)
    ->Arg(22'050)
    ->Arg(44'100)
    ->Arg(48'000)
    ->Arg(96'000)
    ->Unit(::benchmark::kNanosecond);

}  // namespace
}  // namespace starboard
