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
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"

namespace starboard {
namespace benchmark {

namespace {

constexpr char kAudioFile[] = "beneath_the_canopy_aac_stereo.dmp";
constexpr char kVideoFile[] = "vertical_1080p_30_fps_137_avc.dmp";
constexpr int64_t kFeedDurationUs = 2000000;  // 2 seconds
constexpr int64_t kSeekTargetUs = 1000000;    // 1 second

// Initializes |helper|'s player and prerolls it until it is presenting. On
// failure the error is reported to |state| and false is returned.
bool InitializeAndPreroll(SbPlayerBenchmarkHelper* helper,
                          ::benchmark::State* state) {
  if (!helper->InitializePlayer(kAudioFile, kVideoFile,
                                kSbPlayerOutputModeDecodeToTexture)) {
    state->SkipWithError("Player initialization failed");
    return false;
  }
  if (!helper->FeedData(kFeedDurationUs)) {
    state->SkipWithError("Failed to feed the initial preroll data");
    return false;
  }
  if (!helper->WaitForPlayerState(kSbPlayerStatePresenting,
                                  helper->GetCurrentTicket())) {
    state->SkipWithError("Player did not start presenting");
    return false;
  }
  return true;
}

// Feeds data from the seek target onwards and waits until the player presents
// again for |ticket|. On failure the error is reported to |state| and false is
// returned.
bool FeedAndWaitForPresenting(SbPlayerBenchmarkHelper* helper,
                              int ticket,
                              ::benchmark::State* state) {
  if (!helper->FeedData(kSeekTargetUs + kFeedDurationUs)) {
    state->SkipWithError("Failed to feed data after the seek");
    return false;
  }
  if (!helper->WaitForPlayerState(kSbPlayerStatePresenting, ticket)) {
    state->SkipWithError("Player did not resume presenting after the seek");
    return false;
  }
  return true;
}

void BM_PlayerSeekSync(::benchmark::State& state) {
  SbPlayerBenchmarkHelper helper;
  if (!InitializeAndPreroll(&helper, &state)) {
    return;
  }

  for (auto _ : state) {
    int next_ticket = helper.PrepareSeek(kSeekTargetUs);

    // Measure ONLY the SbPlayerSeek call.
    auto start = std::chrono::high_resolution_clock::now();
    SbPlayerSeek(helper.GetPlayer(), kSeekTargetUs, next_ticket);
    auto end = std::chrono::high_resolution_clock::now();

    auto elapsed =
        std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
    state.SetIterationTime(elapsed.count());

    // Bring the player back to presenting before the next iteration.
    if (!FeedAndWaitForPresenting(&helper, next_ticket, &state)) {
      break;
    }
  }
}
BENCHMARK(BM_PlayerSeekSync)->UseManualTime()->Iterations(20);

void BM_PlayerSeekAsync(::benchmark::State& state) {
  SbPlayerBenchmarkHelper helper;
  if (!InitializeAndPreroll(&helper, &state)) {
    return;
  }

  for (auto _ : state) {
    int next_ticket = helper.PrepareSeek(kSeekTargetUs);

    // Measure from SbPlayerSeek to kSbPlayerStatePresenting
    auto start = std::chrono::high_resolution_clock::now();

    SbPlayerSeek(helper.GetPlayer(), kSeekTargetUs, next_ticket);
    if (!FeedAndWaitForPresenting(&helper, next_ticket, &state)) {
      break;
    }

    auto end = std::chrono::high_resolution_clock::now();

    auto elapsed =
        std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
    state.SetIterationTime(elapsed.count());
  }
}
BENCHMARK(BM_PlayerSeekAsync)->UseManualTime()->Iterations(20);

}  // namespace
}  // namespace benchmark
}  // namespace starboard
