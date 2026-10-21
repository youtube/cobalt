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

namespace {

void BM_PlayerDestroySync(::benchmark::State& state) {
  SbPlayerBenchmarkHelper helper;
  SbPlayerCreationParam creation_param;
  helper.GetCreationParam(&creation_param, kSbPlayerOutputModeDecodeToTexture);

  for (auto _ : state) {
    // 1. Create the player and wait until it is fully initialized (not timed).
    //    SbPlayerDestroy() blocks until all pending work of the player has
    //    completed, so destroying a player that is still initializing would
    //    measure the remainder of its initialization instead of its teardown.
    PlayerInitializationWaiter waiter;
    SbPlayer player =
        SbPlayerCreate(helper.GetWindow(), &creation_param,
                       SbPlayerBenchmarkHelper::DummyDeallocateSampleFunc,
                       SbPlayerBenchmarkHelper::DummyDecoderStatusFunc,
                       PlayerInitializationWaiter::PlayerStatusFunc,
                       PlayerInitializationWaiter::PlayerErrorFunc, &waiter,
                       helper.GetDecoderTargetProvider());

    if (!SbPlayerIsValid(player)) {
      state.SkipWithError("SbPlayerCreate failed");
      break;
    }

    if (!waiter.WaitForInitialization()) {
      state.SkipWithError("SbPlayer initialization failed");
      SbPlayerDestroy(player);
      break;
    }

    // 2. Measure only the destruction time of the initialized player.
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
