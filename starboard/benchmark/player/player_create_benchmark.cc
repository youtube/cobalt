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

void BM_PlayerCreate(::benchmark::State& state,
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

    auto end_time = SbPlayerBenchmarkHelper::Clock::now();

    // SbPlayerDestroy() synchronizes with the player worker thread, so checking
    // |waiter.HasError()| afterwards catches asynchronous initialization errors
    // without perturbing the timed [start_time, end_time] window.
    SbPlayerDestroy(player);
    if (waiter.HasError()) {
      state.SkipWithError("SbPlayer reported an error before teardown");
      break;
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(
        end_time - start_time);
    state.SetIterationTime(elapsed.count());
  }
}

void BM_PlayerCreateToInitialized(::benchmark::State& state,
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

struct StartupStreamSpec {
  const char* audio_file;
  const char* video_file;
  SbPlayerOutputMode output_mode;
  std::optional<SbMediaColorMetadata> color_metadata_override;
  bool initialize_codec_with_color_metadata = false;
};

SbMediaColorMetadata MakeFullRangeBt709Metadata() {
  SbMediaColorMetadata metadata = {};
  metadata.bits_per_channel = 8;
  metadata.primaries = kSbMediaPrimaryIdBt709;
  metadata.transfer = kSbMediaTransferIdBt709;
  metadata.matrix = kSbMediaMatrixIdBt709;
  metadata.range = kSbMediaRangeIdFull;
  return metadata;
}

SbMediaColorMetadata MakeHdrPqMetadata() {
  SbMediaColorMetadata metadata = {};
  metadata.bits_per_channel = 8;
  metadata.primaries = kSbMediaPrimaryIdBt2020;
  metadata.transfer = kSbMediaTransferIdSmpteSt2084;
  metadata.matrix = kSbMediaMatrixIdBt2020NonconstantLuminance;
  metadata.range = kSbMediaRangeIdLimited;
  metadata.max_cll = 1000;
  metadata.max_fall = 200;
  metadata.mastering_metadata.luminance_max = 1000.0f;
  metadata.mastering_metadata.luminance_min = 0.005f;
  return metadata;
}

void BM_PlayerCreateToPresenting(::benchmark::State& state,
                                 const StartupStreamSpec& spec) {
  SbPlayerBenchmarkHelper helper;
  if (!helper.LoadDmpFiles(spec.audio_file, spec.video_file)) {
    state.SkipWithError("Failed to load DMP files");
    return;
  }

  const SbMediaColorMetadata* color_override =
      spec.color_metadata_override ? &*spec.color_metadata_override : nullptr;

  for (auto _ : state) {
    auto start_time = SbPlayerBenchmarkHelper::Clock::now();

    switch (helper.CreatePlayerFromLoadedDmp(
        spec.output_mode, color_override,
        spec.initialize_codec_with_color_metadata)) {
      case SbPlayerBenchmarkHelper::InitResult::kOk:
        break;
      case SbPlayerBenchmarkHelper::InitResult::kOutputModeNotSupported:
        state.SkipWithMessage("Output mode is not supported on this platform");
        return;
      case SbPlayerBenchmarkHelper::InitResult::kFailed:
        state.SkipWithError("SbPlayer creation/initialization failed");
        return;
    }

    SbPlayerBenchmarkHelper::Clock::time_point presenting_time;
    if (!helper.FeedUntilPresenting(&presenting_time)) {
      helper.DestroyPlayerInstance();
      state.SkipWithError("SbPlayer did not reach kSbPlayerStatePresenting");
      return;
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(
        presenting_time - start_time);
    state.SetIterationTime(elapsed.count());

    helper.DestroyPlayerInstance();
  }
}

// Explicit iteration counts bound total runtime and provide a uniform 20-sample
// run across platforms: each iteration runs an untimed SbPlayerDestroy() whose
// asynchronous teardown cost can vastly exceed the manually timed region.
BENCHMARK_CAPTURE(BM_PlayerCreate, PunchOut, kSbPlayerOutputModePunchOut)
    ->UseManualTime()
    ->Unit(::benchmark::kMicrosecond)
    ->Iterations(20);
BENCHMARK_CAPTURE(BM_PlayerCreate,
                  DecodeToTexture,
                  kSbPlayerOutputModeDecodeToTexture)
    ->UseManualTime()
    ->Unit(::benchmark::kMicrosecond)
    ->Iterations(20);

BENCHMARK_CAPTURE(BM_PlayerCreateToInitialized,
                  PunchOut,
                  kSbPlayerOutputModePunchOut)
    ->UseManualTime()
    ->Unit(::benchmark::kMillisecond)
    ->Iterations(20);
BENCHMARK_CAPTURE(BM_PlayerCreateToInitialized,
                  DecodeToTexture,
                  kSbPlayerOutputModeDecodeToTexture)
    ->UseManualTime()
    ->Unit(::benchmark::kMillisecond)
    ->Iterations(20);

BENCHMARK_CAPTURE(BM_PlayerCreateToPresenting,
                  PunchOut_AvcIdentitySdr,
                  StartupStreamSpec{"beneath_the_canopy_aac_stereo.dmp",
                                    "vertical_1080p_30_fps_137_avc.dmp",
                                    kSbPlayerOutputModePunchOut, std::nullopt})
    ->UseManualTime()
    ->Unit(::benchmark::kMillisecond)
    ->Iterations(20);
BENCHMARK_CAPTURE(BM_PlayerCreateToPresenting,
                  DecodeToTexture_AvcIdentitySdr,
                  StartupStreamSpec{"beneath_the_canopy_aac_stereo.dmp",
                                    "vertical_1080p_30_fps_137_avc.dmp",
                                    kSbPlayerOutputModeDecodeToTexture,
                                    std::nullopt})
    ->UseManualTime()
    ->Unit(::benchmark::kMillisecond)
    ->Iterations(20);

BENCHMARK_CAPTURE(BM_PlayerCreateToPresenting,
                  PunchOut_AvcFullRangeSdr,
                  StartupStreamSpec{"beneath_the_canopy_aac_stereo.dmp",
                                    "vertical_1080p_30_fps_137_avc.dmp",
                                    kSbPlayerOutputModePunchOut,
                                    MakeFullRangeBt709Metadata(), false})
    ->UseManualTime()
    ->Unit(::benchmark::kMillisecond)
    ->Iterations(20);
BENCHMARK_CAPTURE(BM_PlayerCreateToPresenting,
                  PunchOut_AvcFullRangeSdr_WithColorMetadataOpt,
                  StartupStreamSpec{"beneath_the_canopy_aac_stereo.dmp",
                                    "vertical_1080p_30_fps_137_avc.dmp",
                                    kSbPlayerOutputModePunchOut,
                                    MakeFullRangeBt709Metadata(), true})
    ->UseManualTime()
    ->Unit(::benchmark::kMillisecond)
    ->Iterations(20);
BENCHMARK_CAPTURE(BM_PlayerCreateToPresenting,
                  DecodeToTexture_AvcFullRangeSdr,
                  StartupStreamSpec{"beneath_the_canopy_aac_stereo.dmp",
                                    "vertical_1080p_30_fps_137_avc.dmp",
                                    kSbPlayerOutputModeDecodeToTexture,
                                    MakeFullRangeBt709Metadata(), false})
    ->UseManualTime()
    ->Unit(::benchmark::kMillisecond)
    ->Iterations(20);
BENCHMARK_CAPTURE(BM_PlayerCreateToPresenting,
                  DecodeToTexture_AvcFullRangeSdr_WithColorMetadataOpt,
                  StartupStreamSpec{"beneath_the_canopy_aac_stereo.dmp",
                                    "vertical_1080p_30_fps_137_avc.dmp",
                                    kSbPlayerOutputModeDecodeToTexture,
                                    MakeFullRangeBt709Metadata(), true})
    ->UseManualTime()
    ->Unit(::benchmark::kMillisecond)
    ->Iterations(20);

BENCHMARK_CAPTURE(BM_PlayerCreateToPresenting,
                  PunchOut_Vp9NonIdentityDmp,
                  StartupStreamSpec{"beneath_the_canopy_opus_stereo.dmp",
                                    "vertical_480p_60_fps_244_vp9.dmp",
                                    kSbPlayerOutputModePunchOut, std::nullopt,
                                    false})
    ->UseManualTime()
    ->Unit(::benchmark::kMillisecond)
    ->Iterations(20);
BENCHMARK_CAPTURE(BM_PlayerCreateToPresenting,
                  PunchOut_Vp9NonIdentityDmp_WithColorMetadataOpt,
                  StartupStreamSpec{"beneath_the_canopy_opus_stereo.dmp",
                                    "vertical_480p_60_fps_244_vp9.dmp",
                                    kSbPlayerOutputModePunchOut, std::nullopt,
                                    true})
    ->UseManualTime()
    ->Unit(::benchmark::kMillisecond)
    ->Iterations(20);

BENCHMARK_CAPTURE(BM_PlayerCreateToPresenting,
                  PunchOut_Vp9HdrPq,
                  StartupStreamSpec{"beneath_the_canopy_opus_stereo.dmp",
                                    "beneath_the_canopy_248_vp9.dmp",
                                    kSbPlayerOutputModePunchOut,
                                    MakeHdrPqMetadata(), false})
    ->UseManualTime()
    ->Unit(::benchmark::kMillisecond)
    ->Iterations(20);
BENCHMARK_CAPTURE(BM_PlayerCreateToPresenting,
                  PunchOut_Vp9HdrPq_WithColorMetadataOpt,
                  StartupStreamSpec{"beneath_the_canopy_opus_stereo.dmp",
                                    "beneath_the_canopy_248_vp9.dmp",
                                    kSbPlayerOutputModePunchOut,
                                    MakeHdrPqMetadata(), true})
    ->UseManualTime()
    ->Unit(::benchmark::kMillisecond)
    ->Iterations(20);

}  // namespace
}  // namespace starboard
