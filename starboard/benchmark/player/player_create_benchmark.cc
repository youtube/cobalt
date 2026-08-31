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
#include <condition_variable>  // NOLINT(build/c++11)
#include <mutex>               // NOLINT(build/c++11)

#include "starboard/benchmark/player/player_benchmark_helper.h"
#include "starboard/player.h"
#include "third_party/google_benchmark/src/include/benchmark/benchmark.h"

namespace starboard {
namespace benchmark {

// Dummy function to force linking of this translation unit
void LinkPlayerCreateBenchmark() {}

namespace {

struct AsyncCreateContext {
  std::mutex mutex;
  std::condition_variable cv;
  bool initialized = false;
  bool failed = false;
};

void AsyncPlayerStatusFunc(SbPlayer player,
                           void* context,
                           SbPlayerState state,
                           int ticket) {
  AsyncCreateContext* ctx = static_cast<AsyncCreateContext*>(context);
  if (state == kSbPlayerStateInitialized) {
    std::unique_lock<std::mutex> lock(ctx->mutex);
    ctx->initialized = true;
    ctx->cv.notify_one();
  }
}

void AsyncPlayerErrorFunc(SbPlayer player,
                          void* context,
                          SbPlayerError error,
                          const char* message) {
  AsyncCreateContext* ctx = static_cast<AsyncCreateContext*>(context);
  std::unique_lock<std::mutex> lock(ctx->mutex);
  ctx->initialized = true;
  ctx->failed = true;
  ctx->cv.notify_one();
}

void BM_PlayerCreateSync(::benchmark::State& state) {
  SbPlayerBenchmarkHelper helper;
  SbPlayerCreationParam creation_param;
  helper.GetCreationParam(&creation_param, kSbPlayerOutputModeDecodeToTexture);

  for (auto _ : state) {
    auto start_time = std::chrono::high_resolution_clock::now();

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

    auto end_time = std::chrono::high_resolution_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(
        end_time - start_time);
    state.SetIterationTime(elapsed.count());

    SbPlayerDestroy(player);
  }
}

void BM_PlayerCreateAsync(::benchmark::State& state) {
  SbPlayerBenchmarkHelper helper;
  SbPlayerCreationParam creation_param;
  helper.GetCreationParam(&creation_param, kSbPlayerOutputModeDecodeToTexture);

  for (auto _ : state) {
    AsyncCreateContext ctx;
    auto start_time = std::chrono::high_resolution_clock::now();

    SbPlayer player = SbPlayerCreate(
        helper.GetWindow(), &creation_param,
        SbPlayerBenchmarkHelper::DummyDeallocateSampleFunc,
        SbPlayerBenchmarkHelper::DummyDecoderStatusFunc, AsyncPlayerStatusFunc,
        AsyncPlayerErrorFunc, &ctx, helper.GetDecoderTargetProvider());

    if (!SbPlayerIsValid(player)) {
      state.SkipWithError("SbPlayerCreate failed");
      break;
    }

    {
      std::unique_lock<std::mutex> lock(ctx.mutex);
      ctx.cv.wait(lock, [&ctx] { return ctx.initialized; });
    }

    auto end_time = std::chrono::high_resolution_clock::now();

    if (ctx.failed) {
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

BENCHMARK(BM_PlayerCreateSync)->UseManualTime();
BENCHMARK(BM_PlayerCreateAsync)->UseManualTime();

}  // namespace
}  // namespace benchmark
}  // namespace starboard
