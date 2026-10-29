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
namespace {

void BM_PlayerCreateSync(::benchmark::State& state,
                         SbPlayerOutputMode output_mode) {
  SbPlayerBenchmarkHelper helper;
  SbPlayerCreationParam creation_param;
  SbPlayerBenchmarkHelper::GetCreationParam(&creation_param, output_mode);
  if (!SbPlayerBenchmarkHelper::IsOutputModeSupported(creation_param)) {
    state.SkipWithMessage("Output mode is not supported on this platform");
    return;
  }

  for (auto _ : state) {
    auto start_time = SbPlayerBenchmarkHelper::Clock::now();

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

    auto end_time = SbPlayerBenchmarkHelper::Clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(
        end_time - start_time);
    state.SetIterationTime(elapsed.count());

    SbPlayerDestroy(player);
  }
}

void BM_PlayerCreateAsync(::benchmark::State& state,
                          SbPlayerOutputMode output_mode) {
  SbPlayerBenchmarkHelper helper;
  SbPlayerCreationParam creation_param;
  SbPlayerBenchmarkHelper::GetCreationParam(&creation_param, output_mode);
  if (!SbPlayerBenchmarkHelper::IsOutputModeSupported(creation_param)) {
    state.SkipWithMessage("Output mode is not supported on this platform");
    return;
  }

  for (auto _ : state) {
    PlayerInitializationWaiter waiter;
    auto start_time = SbPlayerBenchmarkHelper::Clock::now();

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

    bool initialized = waiter.WaitForInitialization();

    auto end_time = SbPlayerBenchmarkHelper::Clock::now();

    if (!initialized) {
      state.SkipWithError("SbPlayer initialization failed");
      SbPlayerDestroy(player);
      break;
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(
        end_time - start_time);
    state.SetIterationTime(elapsed.count());

    SbPlayerDestroy(player);
  }
}

// Explicit iteration counts bound total runtime for CreateSync: its manual time
// is only the synchronous SbPlayerCreate() call, while each iteration also runs
// an untimed SbPlayerDestroy() that waits for asynchronous initialization and
// codec teardown (~200-450 ms on Android TV).
BENCHMARK_CAPTURE(BM_PlayerCreateSync, PunchOut, kSbPlayerOutputModePunchOut)
    ->UseManualTime()
    ->Unit(::benchmark::kMicrosecond)
    ->Iterations(20);
BENCHMARK_CAPTURE(BM_PlayerCreateSync,
                  DecodeToTexture,
                  kSbPlayerOutputModeDecodeToTexture)
    ->UseManualTime()
    ->Unit(::benchmark::kMicrosecond)
    ->Iterations(20);

BENCHMARK_CAPTURE(BM_PlayerCreateAsync, PunchOut, kSbPlayerOutputModePunchOut)
    ->UseManualTime()
    ->Unit(::benchmark::kMillisecond);
BENCHMARK_CAPTURE(BM_PlayerCreateAsync,
                  DecodeToTexture,
                  kSbPlayerOutputModeDecodeToTexture)
    ->UseManualTime()
    ->Unit(::benchmark::kMillisecond);

}  // namespace
}  // namespace starboard
