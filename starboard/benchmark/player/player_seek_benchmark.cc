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
#include <cstdint>

#include "starboard/benchmark/player/player_benchmark_helper.h"
#include "starboard/player.h"
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"

namespace starboard {
namespace {

constexpr char kAudioFile[] = "beneath_the_canopy_aac_stereo.dmp";
// 90 frames (2.967 s) at 30 fps with a single key frame at 0 s; seeking to 1 s
// therefore exercises a mid-GOP seek that decodes and discards frames 0..29
// before presenting at 1.0 s.
constexpr char kVideoFile[] = "vertical_1080p_30_fps_137_avc.dmp";
constexpr int64_t kSeekTargetUs = 1'000'000;  // 1 second

// Initializes |helper|'s player in |output_mode| and prerolls it until it is
// presenting. Reports unsupported modes via SkipWithMessage() and errors via
// SkipWithError(), returning false in either case.
bool InitializeAndPreroll(SbPlayerBenchmarkHelper* helper,
                          SbPlayerOutputMode output_mode,
                          ::benchmark::State* state) {
  switch (helper->InitializePlayer(kAudioFile, kVideoFile, output_mode)) {
    case SbPlayerBenchmarkHelper::InitResult::kOk:
      break;
    case SbPlayerBenchmarkHelper::InitResult::kOutputModeNotSupported:
      state->SkipWithMessage("Output mode is not supported on this platform");
      return false;
    case SbPlayerBenchmarkHelper::InitResult::kFailed:
      state->SkipWithError("Player initialization failed");
      return false;
  }
  if (!helper->FeedUntilPresenting()) {
    state->SkipWithError("Player did not start presenting");
    return false;
  }
  return true;
}

void BM_PlayerSeekSync(::benchmark::State& state,
                       SbPlayerOutputMode output_mode) {
  SbPlayerBenchmarkHelper helper;
  if (!InitializeAndPreroll(&helper, output_mode, &state)) {
    return;
  }

  for (auto _ : state) {
    int next_ticket = helper.PrepareSeek(kSeekTargetUs);

    // Measure ONLY the synchronous SbPlayerSeek() call.
    auto start = SbPlayerBenchmarkHelper::Clock::now();
    SbPlayerSeek(helper.GetPlayer(), kSeekTargetUs, next_ticket);
    auto end = SbPlayerBenchmarkHelper::Clock::now();

    auto elapsed =
        std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
    state.SetIterationTime(elapsed.count());

    // Bring the player back to presenting before the next iteration.
    if (!helper.FeedUntilPresenting()) {
      state.SkipWithError("Player did not resume presenting after the seek");
      break;
    }
  }
}

void BM_PlayerSeekAsync(::benchmark::State& state,
                        SbPlayerOutputMode output_mode) {
  SbPlayerBenchmarkHelper helper;
  if (!InitializeAndPreroll(&helper, output_mode, &state)) {
    return;
  }

  for (auto _ : state) {
    int next_ticket = helper.PrepareSeek(kSeekTargetUs);

    // Measure from SbPlayerSeek() to the kSbPlayerStatePresenting callback.
    auto start = SbPlayerBenchmarkHelper::Clock::now();
    SbPlayerSeek(helper.GetPlayer(), kSeekTargetUs, next_ticket);

    SbPlayerBenchmarkHelper::Clock::time_point presenting_time;
    if (!helper.FeedUntilPresenting(&presenting_time)) {
      state.SkipWithError("Player did not resume presenting after the seek");
      break;
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(
        presenting_time - start);
    state.SetIterationTime(elapsed.count());
  }
}

// Explicit iteration counts bound BM_PlayerSeekSync's untimed per-iteration
// preroll cost (its manual time is only a few microseconds, so min-time
// auto-scaling would run thousands of seeks) and give BM_PlayerSeekAsync a
// matching 20-sample run instead of stopping after 1-2 seeks on Android TV.
BENCHMARK_CAPTURE(BM_PlayerSeekSync, PunchOut, kSbPlayerOutputModePunchOut)
    ->UseManualTime()
    ->Unit(::benchmark::kMicrosecond)
    ->Iterations(20);
BENCHMARK_CAPTURE(BM_PlayerSeekSync,
                  DecodeToTexture,
                  kSbPlayerOutputModeDecodeToTexture)
    ->UseManualTime()
    ->Unit(::benchmark::kMicrosecond)
    ->Iterations(20);

BENCHMARK_CAPTURE(BM_PlayerSeekAsync, PunchOut, kSbPlayerOutputModePunchOut)
    ->UseManualTime()
    ->Unit(::benchmark::kMillisecond)
    ->Iterations(20);
BENCHMARK_CAPTURE(BM_PlayerSeekAsync,
                  DecodeToTexture,
                  kSbPlayerOutputModeDecodeToTexture)
    ->UseManualTime()
    ->Unit(::benchmark::kMillisecond)
    ->Iterations(20);

}  // namespace
}  // namespace starboard
