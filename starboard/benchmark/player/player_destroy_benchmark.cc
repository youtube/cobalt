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

#include "starboard/benchmark/player/player_benchmark_helper.h"
#include "starboard/player.h"
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"

namespace starboard {
namespace benchmark {

// Dummy function to force linking of this translation unit
void LinkPlayerDestroyBenchmark() {}

namespace {

void BM_PlayerDestroySync(::benchmark::State& state) {
  SbPlayerBenchmarkHelper helper;
  SbPlayerCreationParam creation_param;
  helper.GetCreationParam(&creation_param, kSbPlayerOutputModePunchOut);

  for (auto _ : state) {
    // 1. Create player (not timed)
    SbPlayer player =
        SbPlayerCreate(helper.GetWindow(), &creation_param,
                       SbPlayerBenchmarkHelper::DummyDeallocateSampleFunc,
                       SbPlayerBenchmarkHelper::DummyDecoderStatusFunc,
                       SbPlayerBenchmarkHelper::DummyPlayerStatusFunc,
                       SbPlayerBenchmarkHelper::DummyPlayerErrorFunc, nullptr,
                       helper.GetDecoderTargetProvider());

    if (!SbPlayerIsValid(player)) {
      state.SkipWithError("SbPlayerCreate failed");
      break;
    }

    // 2. Measure only the destruction time
    auto start_time = std::chrono::high_resolution_clock::now();
    SbPlayerDestroy(player);
    auto end_time = std::chrono::high_resolution_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(
        end_time - start_time);
    state.SetIterationTime(elapsed.count());
  }
}

BENCHMARK(BM_PlayerDestroySync)->UseManualTime();

}  // namespace
}  // namespace benchmark
}  // namespace starboard
