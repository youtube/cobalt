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

const char* kAudioFile = "beneath_the_canopy_aac_stereo.dmp";
const char* kVideoFile = "vertical_1080p_30_fps_137_avc.dmp";
const int64_t kFeedDurationUs = 2000000;  // 2 seconds
const int64_t kSeekTargetUs = 1000000;    // 1 second

void BM_PlayerSeekSync(::benchmark::State& state) {
  SbPlayerBenchmarkHelper helper;
  if (!helper.InitializePlayer(kAudioFile, kVideoFile,
                               kSbPlayerOutputModeDecodeToTexture)) {
    state.SkipWithError("Player initialization failed");
    return;
  }

  // Initial preroll
  helper.FeedData(kFeedDurationUs);

  // Wait for initial presenting to ensure player is stable
  int ticket = helper.GetCurrentTicket();
  helper.WaitForPlayerState(kSbPlayerStatePresenting, ticket);

  for (auto _ : state) {
    state.PauseTiming();
    int next_ticket = helper.PrepareSeek(kSeekTargetUs);
    state.ResumeTiming();

    // Measure ONLY the SbPlayerSeek call
    SbPlayerSeek(helper.GetPlayer(), kSeekTargetUs, next_ticket);

    state.PauseTiming();
    // Feed data from seek target (1s) to target + 2s (3s)
    helper.FeedData(kSeekTargetUs + kFeedDurationUs);
    helper.WaitForPlayerState(kSbPlayerStatePresenting, next_ticket);
    state.ResumeTiming();
  }
}
BENCHMARK(BM_PlayerSeekSync)->Iterations(20);

void BM_PlayerSeekAsync(::benchmark::State& state) {
  SbPlayerBenchmarkHelper helper;
  if (!helper.InitializePlayer(kAudioFile, kVideoFile,
                               kSbPlayerOutputModeDecodeToTexture)) {
    state.SkipWithError("Player initialization failed");
    return;
  }

  // Initial preroll
  helper.FeedData(kFeedDurationUs);
  int ticket = helper.GetCurrentTicket();
  helper.WaitForPlayerState(kSbPlayerStatePresenting, ticket);

  for (auto _ : state) {
    int next_ticket = helper.PrepareSeek(kSeekTargetUs);

    // Measure from SbPlayerSeek to kSbPlayerStatePresenting
    auto start = std::chrono::high_resolution_clock::now();

    SbPlayerSeek(helper.GetPlayer(), kSeekTargetUs, next_ticket);
    helper.FeedData(kSeekTargetUs + kFeedDurationUs);
    helper.WaitForPlayerState(kSbPlayerStatePresenting, next_ticket);

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
