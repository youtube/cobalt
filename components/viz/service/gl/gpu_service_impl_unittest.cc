// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/gl/gpu_service_impl.h"

#include <memory>
#include <tuple>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "components/viz/common/resources/peak_gpu_memory_tracker_util.h"
#include "components/viz/service/input/peak_gpu_memory_tracker_impl.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/viz/public/mojom/gpu.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/init/gl_factory.h"

namespace viz {

namespace {

const uint64_t kPeakMemoryMB = 42u;
const uint64_t kPeakMemory = kPeakMemoryMB * 1048576u;

}  // namespace

class MockGpuServiceImpl : public GpuServiceImpl {
 public:
  explicit MockGpuServiceImpl(
      const gpu::GpuPreferences& gpu_preferences,
      const gpu::GPUInfo& gpu_info,
      const gpu::GpuFeatureInfo& gpu_feature_info,
      const std::optional<gpu::GPUInfo>& gpu_info_for_hardware_gpu,
      const std::optional<gpu::GpuFeatureInfo>&
          gpu_feature_info_for_hardware_gpu,
      const gfx::GpuExtraInfo& gpu_extra_info,
      InitParams init_params)
      : GpuServiceImpl(gpu_preferences,
                       gpu_info,
                       gpu_feature_info,
                       gpu_info_for_hardware_gpu,
                       gpu_feature_info_for_hardware_gpu,
                       gpu_extra_info,
                       std::move(init_params)) {}
  ~MockGpuServiceImpl() override = default;

  MOCK_METHOD(void,
              StartPeakMemoryMonitor,
              (uint32_t sequence_num),
              (override));

  MOCK_METHOD(void,
              GetPeakMemoryUsageOnMainThread,
              (uint32_t sequence_num, GetPeakMemoryUsageCallback callback),
              (override));
};

class GpuServiceTest : public testing::Test {
 public:
  GpuServiceTest()
      : io_thread_("TestIOThread"),
        wait_(base::WaitableEvent::ResetPolicy::MANUAL,
              base::WaitableEvent::InitialState::NOT_SIGNALED) {}

  GpuServiceTest(const GpuServiceTest&) = delete;
  GpuServiceTest& operator=(const GpuServiceTest&) = delete;

  ~GpuServiceTest() override = default;

  MockGpuServiceImpl* mock_gpu_service() {
    return static_cast<MockGpuServiceImpl*>(gpu_service_.get());
  }
  GpuServiceImpl* gpu_service() { return gpu_service_.get(); }

  void DestroyService() { gpu_service_ = nullptr; }

  void BlockIOThread() {
    wait_.Reset();
    io_runner()->PostTask(FROM_HERE, base::BindOnce(&base::WaitableEvent::Wait,
                                                    base::Unretained(&wait_)));
  }

  void UnblockIOThread() {
    DCHECK(!wait_.IsSignaled());
    wait_.Signal();
  }

  scoped_refptr<base::SingleThreadTaskRunner> io_runner() {
    return io_thread_.task_runner();
  }

  // testing::Test
  void SetUp() override {
    ASSERT_TRUE(io_thread_.Start());
    gpu::GPUInfo gpu_info;
    gpu_info.in_process_gpu = false;
    GpuServiceImpl::InitParams init_params;
    init_params.io_runner = io_thread_.task_runner();
    gpu_service_ = std::make_unique<testing::NiceMock<MockGpuServiceImpl>>(
        gpu::GpuPreferences(), gpu_info, gpu::GpuFeatureInfo(), gpu::GPUInfo(),
        gpu::GpuFeatureInfo(), gfx::GpuExtraInfo(), std::move(init_params));
  }

  void TearDown() override {
    DestroyService();
    base::RunLoop runloop;
    runloop.RunUntilIdle();
    io_thread_.Stop();
  }

  std::optional<bool> visible_;

 private:
  base::Thread io_thread_;
  std::unique_ptr<GpuServiceImpl> gpu_service_;
  base::WaitableEvent wait_;
};

// Tests that GpuServiceImpl can be destroyed before Bind() succeeds on the IO
// thread.
TEST_F(GpuServiceTest, ServiceDestroyedBeforeBind) {
  // Block the IO thread to make sure that the GpuServiceImpl is destroyed
  // before the binding happens on the IO thread.
  mojo::Remote<mojom::GpuService> gpu_service_remote;
  BlockIOThread();
  gpu_service()->Bind(gpu_service_remote.BindNewPipeAndPassReceiver());
  UnblockIOThread();
  DestroyService();
}

// Tests that GpuServiceImpl can be destroyed after Bind() succeeds on the IO
// thread.
TEST_F(GpuServiceTest, ServiceDestroyedAfterBind) {
  mojo::Remote<mojom::GpuService> gpu_service_remote;
  gpu_service()->Bind(gpu_service_remote.BindNewPipeAndPassReceiver());
  base::WaitableEvent wait(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  io_runner()->PostTask(FROM_HERE, base::BindOnce(&base::WaitableEvent::Signal,
                                                  base::Unretained(&wait)));
  wait.Wait();
  DestroyService();
}

// Tests that the visibility callback gets called when visibility changes.
TEST_F(GpuServiceTest, VisibilityCallbackCalled) {
  mojo::Remote<mojom::GpuService> gpu_service_remote;
  gpu_service()->Bind(gpu_service_remote.BindNewPipeAndPassReceiver());

  mojo::PendingRemote<mojom::GpuHost> gpu_host_proxy;
  std::ignore = gpu_host_proxy.InitWithNewPipeAndPassReceiver();
  gpu_service()->InitializeWithHost(
      std::move(gpu_host_proxy), gpu::GpuProcessShmCount(),
      gl::init::CreateOffscreenGLSurface(gl::GetDefaultDisplay(), gfx::Size()),
      mojom::GpuServiceCreationParams::New());
  gpu_service_remote.FlushForTesting();

  gpu_service()->SetVisibilityChangedCallback(base::BindRepeating(
      [](GpuServiceTest* test, bool visible) { test->visible_ = visible; },
      base::Unretained(this)));
  EXPECT_FALSE(visible_.has_value());

  gpu_service_remote->OnForegrounded();
  gpu_service_remote.FlushForTesting();

  EXPECT_TRUE(visible_.has_value());
  EXPECT_TRUE(*visible_);

  gpu_service_remote->OnBackgrounded();
  gpu_service_remote.FlushForTesting();

  EXPECT_TRUE(visible_.has_value());
  EXPECT_FALSE(*visible_);
}

// Tests that when a PeakGpuMemoryTracker is destroyed, GpuService properly
// updates the histograms.
TEST_F(GpuServiceTest, PeakGpuMemoryCallback) {
  mojo::Remote<mojom::GpuService> gpu_service_remote;
  gpu_service()->Bind(gpu_service_remote.BindNewPipeAndPassReceiver());

  mojo::PendingRemote<mojom::GpuHost> gpu_host_proxy;
  std::ignore = gpu_host_proxy.InitWithNewPipeAndPassReceiver();
  gpu_service()->InitializeWithHost(
      std::move(gpu_host_proxy), gpu::GpuProcessShmCount(),
      gl::init::CreateOffscreenGLSurface(gl::GetDefaultDisplay(), gfx::Size()),
      mojom::GpuServiceCreationParams::New());
  gpu_service_remote.FlushForTesting();

  ON_CALL(*mock_gpu_service(), GetPeakMemoryUsageOnMainThread)
      .WillByDefault(testing::Invoke(
          [](uint32_t sequence_num,
             GpuServiceImpl::GetPeakMemoryUsageCallback callback) {
            ASSERT_EQ(GetPeakMemoryUsageRequestLocation(sequence_num),
                      SequenceLocation::kGpuProcess);
            base::flat_map<gpu::GpuPeakMemoryAllocationSource, uint64_t>
                allocation_per_source;
            allocation_per_source[gpu::GpuPeakMemoryAllocationSource::UNKNOWN] =
                kPeakMemory;
            std::move(callback).Run(kPeakMemory, allocation_per_source);
          }));

  base::HistogramTester histogram;
  auto tracker = std::make_unique<PeakGpuMemoryTrackerImpl>(
      PeakGpuMemoryTracker::Usage::PAGE_LOAD, gpu_service());

  // No report in response to creation.
  histogram.ExpectTotalCount("Memory.GPU.PeakMemoryUsage2.PageLoad", 0);
  histogram.ExpectTotalCount(
      "Memory.GPU.PeakMemoryAllocationSource.PageLoad.Unknown", 0);

  // Deleting the tracker should start a request for peak Gpu memory usage,
  // with the callback being a posted task.
  tracker.reset();
  gpu_service_remote.FlushForTesting();

  histogram.ExpectUniqueSample("Memory.GPU.PeakMemoryUsage2.PageLoad",
                               kPeakMemoryMB, 1);
  histogram.ExpectUniqueSample(
      "Memory.GPU.PeakMemoryAllocationSource2.PageLoad.Unknown", kPeakMemoryMB,
      1);
}

}  // namespace viz
