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

#include "cobalt/browser/memory_ablation.h"

#include <string>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "cobalt/browser/features.h"
#include "gpu/command_buffer/common/shm_count.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/shared_image/shared_image_manager.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/config/gpu_feature_info.h"
#include "gpu/config/gpu_preferences.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/gpu_channel_manager_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_surface_stub.h"

namespace cobalt {
namespace {

class MemoryAblationTest : public ::testing::Test {
 protected:
  void SetUp() override { ResetMemoryAblationForTesting(); }

  void TearDown() override { ResetMemoryAblationForTesting(); }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::HistogramTester histogram_tester_;
};

TEST_F(MemoryAblationTest, DisabledByDefault) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kCobaltNativeMemoryAblation);

  MaybeApplyMemoryAblation();
  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.NativeMemoryAblation.Enabled", false, 1);
  histogram_tester_.ExpectTotalCount(
      "Cobalt.Features.NativeMemoryAblation.AllocatedMB", 0);
  histogram_tester_.ExpectTotalCount(
      "Cobalt.Features.NativeMemoryAblation.Result", 0);
}

TEST_F(MemoryAblationTest, EnabledWithZeroSize) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kCobaltNativeMemoryAblation, {{"ablation_size_mb", "0"}});

  MaybeApplyMemoryAblation();
  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.NativeMemoryAblation.Enabled", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.NativeMemoryAblation.AllocatedMB", 0, 1);
  histogram_tester_.ExpectTotalCount(
      "Cobalt.Features.NativeMemoryAblation.Result", 0);
}

TEST_F(MemoryAblationTest, EnabledWithAllocatedSize) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kCobaltNativeMemoryAblation, {{"ablation_size_mb", "1"}});

  MaybeApplyMemoryAblation();
  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.NativeMemoryAblation.Enabled", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.NativeMemoryAblation.AllocatedMB", 1, 1);
  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.NativeMemoryAblation.Result",
      NativeMemoryAblationResult::kSuccess, 1);
}

TEST_F(MemoryAblationTest, EnabledWithDelayedExecution) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kCobaltNativeMemoryAblation,
      {{"ablation_size_mb", "1"}, {"ablation_delay", "5s"}});

  MaybeApplyMemoryAblation();
  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.NativeMemoryAblation.Enabled", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.NativeMemoryAblation.AllocatedMB", 1, 1);
  // Task should not have run yet because of 5s delay.
  histogram_tester_.ExpectTotalCount(
      "Cobalt.Features.NativeMemoryAblation.Result", 0);

  // Fast-forward by 4 seconds (still not run).
  task_environment_.FastForwardBy(base::Seconds(4));
  histogram_tester_.ExpectTotalCount(
      "Cobalt.Features.NativeMemoryAblation.Result", 0);

  // Fast-forward by remaining 1 second (now runs).
  task_environment_.FastForwardBy(base::Seconds(1));
  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.NativeMemoryAblation.Result",
      NativeMemoryAblationResult::kSuccess, 1);
}

TEST_F(MemoryAblationTest, EnabledWithExceedingMaxSize) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kCobaltNativeMemoryAblation,
      {{"ablation_size_mb", base::NumberToString(kMaxAblationSizeMB + 1)}});

  MaybeApplyMemoryAblation();
  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.NativeMemoryAblation.Enabled", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.NativeMemoryAblation.AllocatedMB",
      kMaxAblationSizeMB + 1, 1);
  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.NativeMemoryAblation.Result",
      NativeMemoryAblationResult::kExceedsMaxLimit, 1);
}

TEST_F(MemoryAblationTest, ExecutesAtMostOnce) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kCobaltNativeMemoryAblation, {{"ablation_size_mb", "1"}});

  MaybeApplyMemoryAblation();
  task_environment_.RunUntilIdle();

  // Call second time within same app lifetime
  MaybeApplyMemoryAblation();
  task_environment_.RunUntilIdle();

  // Histograms should only have 1 sample
  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.NativeMemoryAblation.Enabled", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.NativeMemoryAblation.AllocatedMB", 1, 1);
  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.NativeMemoryAblation.Result",
      NativeMemoryAblationResult::kSuccess, 1);
}

// Returns a fake GPU allocator that records the requested size and call count
// and returns |result|.
GpuMemoryAblationAllocator MakeFakeGpuAllocator(GpuMemoryAblationResult result,
                                                int* call_count,
                                                int* requested_mb) {
  return base::BindOnce(
      [](GpuMemoryAblationResult result, int* call_count, int* requested_mb,
         int size_mb) {
        ++(*call_count);
        *requested_mb = size_mb;
        return result;
      },
      result, call_count, requested_mb);
}

TEST_F(MemoryAblationTest, GpuDisabledByDefault) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(features::kCobaltGpuMemoryAblation);

  int call_count = 0;
  int requested_mb = 0;
  MaybeApplyGpuMemoryAblationWithAllocator(MakeFakeGpuAllocator(
      GpuMemoryAblationResult::kSuccess, &call_count, &requested_mb));
  task_environment_.RunUntilIdle();

  EXPECT_EQ(call_count, 0);
  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.GpuMemoryAblation.Enabled", false, 1);
  histogram_tester_.ExpectTotalCount(
      "Cobalt.Features.GpuMemoryAblation.AllocatedMB", 0);
  histogram_tester_.ExpectTotalCount("Cobalt.Features.GpuMemoryAblation.Result",
                                     0);
}

TEST_F(MemoryAblationTest, GpuEnabledWithZeroSize) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kCobaltGpuMemoryAblation,
      {{"CobaltGpuMemoryAblation_ablation_size_mb", "0"}});

  int call_count = 0;
  int requested_mb = 0;
  MaybeApplyGpuMemoryAblationWithAllocator(MakeFakeGpuAllocator(
      GpuMemoryAblationResult::kSuccess, &call_count, &requested_mb));
  task_environment_.RunUntilIdle();

  EXPECT_EQ(call_count, 0);
  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.GpuMemoryAblation.Enabled", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.GpuMemoryAblation.AllocatedMB", 0, 1);
  histogram_tester_.ExpectTotalCount("Cobalt.Features.GpuMemoryAblation.Result",
                                     0);
}

TEST_F(MemoryAblationTest, GpuEnabledWithAllocatedSize) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kCobaltGpuMemoryAblation,
      {{"CobaltGpuMemoryAblation_ablation_size_mb", "8"}});

  int call_count = 0;
  int requested_mb = 0;
  MaybeApplyGpuMemoryAblationWithAllocator(MakeFakeGpuAllocator(
      GpuMemoryAblationResult::kSuccess, &call_count, &requested_mb));
  task_environment_.RunUntilIdle();

  EXPECT_EQ(call_count, 1);
  EXPECT_EQ(requested_mb, 8);
  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.GpuMemoryAblation.AllocatedMB", 8, 1);
  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.GpuMemoryAblation.Result",
      GpuMemoryAblationResult::kSuccess, 1);
}

TEST_F(MemoryAblationTest, GpuEnabledWithDelayedExecution) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kCobaltGpuMemoryAblation,
      {{"CobaltGpuMemoryAblation_ablation_size_mb", "4"},
       {"CobaltGpuMemoryAblation_ablation_delay", "5s"}});

  int call_count = 0;
  int requested_mb = 0;
  MaybeApplyGpuMemoryAblationWithAllocator(MakeFakeGpuAllocator(
      GpuMemoryAblationResult::kSuccess, &call_count, &requested_mb));
  task_environment_.RunUntilIdle();

  // Task should not have run yet because of 5s delay.
  EXPECT_EQ(call_count, 0);
  histogram_tester_.ExpectTotalCount("Cobalt.Features.GpuMemoryAblation.Result",
                                     0);

  // Fast-forward by 4 seconds (still not run).
  task_environment_.FastForwardBy(base::Seconds(4));
  EXPECT_EQ(call_count, 0);
  histogram_tester_.ExpectTotalCount("Cobalt.Features.GpuMemoryAblation.Result",
                                     0);

  // Fast-forward by remaining 1 second (now runs).
  task_environment_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(call_count, 1);
  EXPECT_EQ(requested_mb, 4);
  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.GpuMemoryAblation.Result",
      GpuMemoryAblationResult::kSuccess, 1);
}

TEST_F(MemoryAblationTest, GpuEnabledWithExceedingMaxSize) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kCobaltGpuMemoryAblation,
      {{"CobaltGpuMemoryAblation_ablation_size_mb",
        base::NumberToString(kMaxAblationSizeMB + 1)}});

  int call_count = 0;
  int requested_mb = 0;
  MaybeApplyGpuMemoryAblationWithAllocator(MakeFakeGpuAllocator(
      GpuMemoryAblationResult::kSuccess, &call_count, &requested_mb));
  task_environment_.RunUntilIdle();

  EXPECT_EQ(call_count, 0);
  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.GpuMemoryAblation.Result",
      GpuMemoryAblationResult::kExceedsMaxLimit, 1);
}

TEST_F(MemoryAblationTest, GpuAllocationFailure) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kCobaltGpuMemoryAblation,
      {{"CobaltGpuMemoryAblation_ablation_size_mb", "16"}});

  int call_count = 0;
  int requested_mb = 0;
  MaybeApplyGpuMemoryAblationWithAllocator(MakeFakeGpuAllocator(
      GpuMemoryAblationResult::kGlOutOfMemory, &call_count, &requested_mb));
  task_environment_.RunUntilIdle();

  EXPECT_EQ(call_count, 1);
  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.GpuMemoryAblation.Result",
      GpuMemoryAblationResult::kGlOutOfMemory, 1);
}

TEST_F(MemoryAblationTest, GpuChannelManagerUnavailable) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kCobaltGpuMemoryAblation,
      {{"CobaltGpuMemoryAblation_ablation_size_mb", "4"}});

  // Exercises the production GL allocator, which bails out before touching GL
  // when there is no GpuChannelManager.
  MaybeApplyGpuMemoryAblation(nullptr);
  task_environment_.RunUntilIdle();

  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.GpuMemoryAblation.Result",
      GpuMemoryAblationResult::kChannelManagerUnavailable, 1);
}

TEST_F(MemoryAblationTest, GpuExecutesAtMostOnce) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kCobaltGpuMemoryAblation,
      {{"CobaltGpuMemoryAblation_ablation_size_mb", "2"}});

  int call_count = 0;
  int requested_mb = 0;
  MaybeApplyGpuMemoryAblationWithAllocator(MakeFakeGpuAllocator(
      GpuMemoryAblationResult::kSuccess, &call_count, &requested_mb));
  task_environment_.RunUntilIdle();

  MaybeApplyGpuMemoryAblationWithAllocator(MakeFakeGpuAllocator(
      GpuMemoryAblationResult::kSuccess, &call_count, &requested_mb));
  task_environment_.RunUntilIdle();

  EXPECT_EQ(call_count, 1);
  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.GpuMemoryAblation.Enabled", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.GpuMemoryAblation.Result",
      GpuMemoryAblationResult::kSuccess, 1);
}

// Covers the user journey where Cobalt starts concealed (Preload) with EGL
// initialization deferred so default_offscreen_surface() is initially null,
// verifying that MaybeApplyGpuMemoryAblation waits until the first Reveal
// provides an offscreen surface and runs exactly once.
TEST_F(MemoryAblationTest, GpuDefersUntilDefaultOffscreenSurfaceReady) {
  class StubGpuChannelManagerDelegate : public gpu::GpuChannelManagerDelegate {
   public:
    explicit StubGpuChannelManagerDelegate(gpu::Scheduler* scheduler)
        : scheduler_(scheduler) {}
    void LoseAllContexts() override {}
    void DidCreateContextSuccessfully() override {}
    void DidCreateOffscreenContext(const GURL& active_url) override {}
    void DidDestroyChannel(int client_id) override {}
    void DidDestroyAllChannels() override {}
    void DidDestroyOffscreenContext(const GURL& active_url) override {}
    void DidLoseContext(gpu::error::ContextLostReason reason,
                        const GURL& active_url) override {}
    void StoreBlobToDisk(const gpu::GpuDiskCacheHandle& handle,
                         const std::string& key,
                         const std::string& shader) override {}
    void GetIsolationKey(int client_id,
                         const blink::WebGPUExecutionContextToken& token,
                         GetIsolationKeyCallback cb) override {}
    void MaybeExitOnContextLost(
        gpu::error::ContextLostReason context_lost_reason) override {}
    bool IsExiting() const override { return false; }
    gpu::Scheduler* GetGpuScheduler() override { return scheduler_; }

   private:
    raw_ptr<gpu::Scheduler> scheduler_;
  };

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kCobaltGpuMemoryAblation,
      {{"CobaltGpuMemoryAblation_ablation_size_mb", "0"}});

  gpu::SyncPointManager sync_point_manager;
  gpu::SharedImageManager shared_image_manager(/*thread_safe=*/false);
  gpu::Scheduler scheduler(&sync_point_manager);
  StubGpuChannelManagerDelegate delegate(&scheduler);
  gpu::GpuChannelManager channel_manager(
      gpu::GpuPreferences(), &delegate, /*watchdog=*/nullptr,
      task_environment_.GetMainThreadTaskRunner(),
      task_environment_.GetMainThreadTaskRunner(), &scheduler,
      &sync_point_manager, &shared_image_manager,
      /*gpu_memory_buffer_factory=*/nullptr, gpu::GpuFeatureInfo(),
      gpu::GpuProcessShmCount(), /*default_offscreen_surface=*/nullptr,
      /*image_decode_accelerator_worker=*/nullptr);

  // While default_offscreen_surface() is null (deferred preload), ablation must
  // not run or emit its Enabled histogram yet.
  MaybeApplyGpuMemoryAblation(&channel_manager);
  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectTotalCount(
      "Cobalt.Features.GpuMemoryAblation.Enabled", 0);

  // Setting the default offscreen surface on first Reveal runs the deferred
  // ablation exactly once.
  auto stub_surface = base::MakeRefCounted<gl::GLSurfaceStub>();
  channel_manager.SetDefaultOffscreenSurface(stub_surface);
  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.GpuMemoryAblation.Enabled", true, 1);
  histogram_tester_.ExpectUniqueSample(
      "Cobalt.Features.GpuMemoryAblation.AllocatedMB", 0, 1);

  // Subsequent Conceal -> Reveal surface resets must not re-run ablation.
  channel_manager.SetDefaultOffscreenSurface(nullptr);
  channel_manager.SetDefaultOffscreenSurface(stub_surface);
  task_environment_.RunUntilIdle();
  histogram_tester_.ExpectTotalCount(
      "Cobalt.Features.GpuMemoryAblation.Enabled", 1);
}

}  // namespace
}  // namespace cobalt
