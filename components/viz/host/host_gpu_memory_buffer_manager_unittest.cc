// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/host/host_gpu_memory_buffer_manager.h"

#include <string>
#include <utility>
#include <vector>

#include "base/clang_profiling_buildflags.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/host/gpu_memory_buffer_support.h"
#include "media/media_buildflags.h"
#include "services/viz/privileged/mojom/gl/gpu_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/client_native_pixmap_factory.h"

#if BUILDFLAG(IS_OZONE)
#include "ui/ozone/public/ozone_platform.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/android_hardware_buffer_compat.h"
#endif

namespace viz {

namespace {

bool MustSignalGmbConfigReadyForTest() {
#if BUILDFLAG(IS_OZONE)
  // Some Ozone platforms (Ozone/X11) require GPU process initialization to
  // determine GMB support.
  return ui::OzonePlatform::GetInstance()
      ->GetPlatformProperties()
      .fetch_buffer_formats_for_gmb_on_gpu;
#else
  return false;
#endif
}

class TestGpuService : public mojom::GpuService {
 public:
  TestGpuService() = default;

  TestGpuService(const TestGpuService&) = delete;
  TestGpuService& operator=(const TestGpuService&) = delete;

  ~TestGpuService() override = default;

  mojom::GpuService* GetGpuService(base::OnceClosure connection_error_handler) {
    DCHECK(!connection_error_handler_);
    connection_error_handler_ = std::move(connection_error_handler);
    return this;
  }

  void SimulateConnectionError() {
    if (connection_error_handler_)
      std::move(connection_error_handler_).Run();
  }

  int GetAllocationRequestsCount() const { return allocation_requests_.size(); }

  bool IsAllocationRequestAt(size_t index,
                             gfx::GpuMemoryBufferId id,
                             int client_id) const {
    DCHECK_LT(index, allocation_requests_.size());
    const auto& req = allocation_requests_[index];
    return req.id == id && req.client_id == client_id;
  }

  int GetDestructionRequestsCount() const {
    return destruction_requests_.size();
  }

  bool IsDestructionRequestAt(size_t index,
                              gfx::GpuMemoryBufferId id,
                              int client_id) const {
    DCHECK_LT(index, destruction_requests_.size());
    const auto& req = destruction_requests_[index];
    return req.id == id && req.client_id == client_id;
  }

  void SatisfyAllocationRequestAt(size_t index) {
    DCHECK_LT(index, allocation_requests_.size());
    auto& req = allocation_requests_[index];

    gfx::GpuMemoryBufferHandle handle;
    handle.id = req.id;
    handle.type = gfx::SHARED_MEMORY_BUFFER;
    constexpr size_t kBufferSizeBytes = 100;
    handle.region = base::UnsafeSharedMemoryRegion::Create(kBufferSizeBytes);

    DCHECK(req.callback);
    std::move(req.callback).Run(std::move(handle));
  }

  // mojom::GpuService:
  void EstablishGpuChannel(int32_t client_id,
                           uint64_t client_tracing_id,
                           bool is_gpu_host,
                           EstablishGpuChannelCallback callback) override {}
  void SetChannelClientPid(int32_t client_id,
                           base::ProcessId client_pid) override {}
  void SetChannelDiskCacheHandle(
      int32_t client_id,
      const gpu::GpuDiskCacheHandle& handle) override {}
  void OnDiskCacheHandleDestoyed(
      const gpu::GpuDiskCacheHandle& handle) override {}

  void CloseChannel(int32_t client_id) override {}
#if BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  void CreateArcVideoDecodeAccelerator(
      mojo::PendingReceiver<arc::mojom::VideoDecodeAccelerator> vda_receiver)
      override {}

  void CreateArcVideoDecoder(
      mojo::PendingReceiver<arc::mojom::VideoDecoder> vd_receiver) override {}

  void CreateArcVideoEncodeAccelerator(
      mojo::PendingReceiver<arc::mojom::VideoEncodeAccelerator> vea_receiver)
      override {}

  void CreateArcVideoProtectedBufferAllocator(
      mojo::PendingReceiver<arc::mojom::VideoProtectedBufferAllocator>
          pba_receiver) override {}

  void CreateArcProtectedBufferManager(
      mojo::PendingReceiver<arc::mojom::ProtectedBufferManager> pbm_receiver)
      override {}
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

  void CreateJpegDecodeAccelerator(
      mojo::PendingReceiver<chromeos_camera::mojom::MjpegDecodeAccelerator>
          jda_receiver) override {}

  void CreateJpegEncodeAccelerator(
      mojo::PendingReceiver<chromeos_camera::mojom::JpegEncodeAccelerator>
          jea_receiver) override {}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_WIN)
  void RegisterDCOMPSurfaceHandle(
      mojo::PlatformHandle surface_handle,
      RegisterDCOMPSurfaceHandleCallback callback) override {}
  void UnregisterDCOMPSurfaceHandle(
      const base::UnguessableToken& token) override {}
#endif

  void CreateVideoEncodeAcceleratorProvider(
      mojo::PendingReceiver<media::mojom::VideoEncodeAcceleratorProvider>
          receiver) override {}

  void CreateGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                             const gfx::Size& size,
                             gfx::BufferFormat format,
                             gfx::BufferUsage usage,
                             int client_id,
                             gpu::SurfaceHandle surface_handle,
                             CreateGpuMemoryBufferCallback callback) override {
    allocation_requests_.push_back({id, client_id, std::move(callback)});
  }

  void DestroyGpuMemoryBuffer(gfx::GpuMemoryBufferId id,
                              int client_id) override {
    destruction_requests_.push_back({id, client_id});
  }

  void CopyGpuMemoryBuffer(::gfx::GpuMemoryBufferHandle buffer_handle,
                           ::base::UnsafeSharedMemoryRegion shared_memory,
                           CopyGpuMemoryBufferCallback callback) override {
    std::move(callback).Run(false);
  }

  void GetVideoMemoryUsageStats(
      GetVideoMemoryUsageStatsCallback callback) override {}

  void StartPeakMemoryMonitor(uint32_t sequence_num) override {}

  void GetPeakMemoryUsage(uint32_t sequence_num,
                          GetPeakMemoryUsageCallback callback) override {}

#if BUILDFLAG(IS_WIN)
  void RequestDXGIInfo(RequestDXGIInfoCallback callback) override {}
#endif

  void LoadedBlob(const gpu::GpuDiskCacheHandle& handle,
                  const std::string& key,
                  const std::string& data) override {}

  void WakeUpGpu() override {}

  void GpuSwitched(gl::GpuPreference active_gpu_heuristic) override {}

  void DisplayAdded() override {}

  void DisplayRemoved() override {}

  void DisplayMetricsChanged() override {}

  void DestroyAllChannels() override {}

  void OnBackgroundCleanup() override {}

  void OnBackgrounded() override {}

  void OnForegrounded() override {}

#if !BUILDFLAG(IS_ANDROID)
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level) override {}
#endif

#if BUILDFLAG(IS_APPLE)
  void BeginCATransaction() override {}

  void CommitCATransaction(CommitCATransactionCallback callback) override {}
#endif

#if BUILDFLAG(CLANG_PROFILING_INSIDE_SANDBOX)
  void WriteClangProfilingProfile(
      WriteClangProfilingProfileCallback callback) override {}
#endif

  void GetDawnInfo(GetDawnInfoCallback callback) override {}

  void Crash() override {}

  void Hang() override {}

  void ThrowJavaException() override {}

 private:
  base::OnceClosure connection_error_handler_;

  struct AllocationRequest {
    gfx::GpuMemoryBufferId id;
    int client_id;
    CreateGpuMemoryBufferCallback callback;
  };
  std::vector<AllocationRequest> allocation_requests_;

  struct DestructionRequest {
    const gfx::GpuMemoryBufferId id;
    const int client_id;
  };
  std::vector<DestructionRequest> destruction_requests_;
};

}  // namespace

class HostGpuMemoryBufferManagerTest : public ::testing::Test {
 public:
  HostGpuMemoryBufferManagerTest() = default;

  HostGpuMemoryBufferManagerTest(const HostGpuMemoryBufferManagerTest&) =
      delete;
  HostGpuMemoryBufferManagerTest& operator=(
      const HostGpuMemoryBufferManagerTest&) = delete;

  ~HostGpuMemoryBufferManagerTest() override {
    if (gpu_memory_buffer_manager_)
      gpu_memory_buffer_manager_->Shutdown();
  }

  void SetUp() override {
    gpu_service_ = std::make_unique<TestGpuService>();
    auto gpu_service_provider = base::BindRepeating(
        &TestGpuService::GetGpuService, base::Unretained(gpu_service_.get()));
    auto gpu_memory_buffer_support =
        std::make_unique<gpu::GpuMemoryBufferSupport>();
    gpu_memory_buffer_manager_ = std::make_unique<HostGpuMemoryBufferManager>(
        std::move(gpu_service_provider), 1,
        std::move(gpu_memory_buffer_support),
        base::SingleThreadTaskRunner::GetCurrentDefault());
    if (MustSignalGmbConfigReadyForTest())
      gpu_memory_buffer_manager_->native_configurations_initialized_.Set();
  }

  // Not all platforms support native configurations (currently only Windows,
  // Mac and some Ozone platforms). Abort the test in those platforms.
  bool IsNativePixmapConfigSupported() {
    bool native_pixmap_supported = false;
#if BUILDFLAG(IS_OZONE)
    native_pixmap_supported =
        ui::OzonePlatform::GetInstance()->IsNativePixmapConfigSupported(
            gfx::BufferFormat::RGBA_8888, gfx::BufferUsage::GPU_READ);
#elif BUILDFLAG(IS_ANDROID)
    native_pixmap_supported =
        base::AndroidHardwareBufferCompat::IsSupportAvailable();
#elif BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_WIN)
    native_pixmap_supported = true;
#endif

    if (native_pixmap_supported)
      return true;

    gpu::GpuMemoryBufferSupport support;
    DCHECK(gpu::GetNativeGpuMemoryBufferConfigurations(&support).empty());
    return false;
  }

  std::unique_ptr<gfx::GpuMemoryBuffer> AllocateGpuMemoryBufferSync() {
    base::Thread diff_thread("TestThread");
    diff_thread.Start();
    std::unique_ptr<gfx::GpuMemoryBuffer> buffer;
    base::RunLoop run_loop;
    diff_thread.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](HostGpuMemoryBufferManager* manager,
               std::unique_ptr<gfx::GpuMemoryBuffer>* out_buffer,
               base::OnceClosure callback) {
              *out_buffer = manager->CreateGpuMemoryBuffer(
                  gfx::Size(64, 64), gfx::BufferFormat::YVU_420,
                  gfx::BufferUsage::GPU_READ, gpu::kNullSurfaceHandle, nullptr);
              std::move(callback).Run();
            },
            gpu_memory_buffer_manager_.get(), &buffer, run_loop.QuitClosure()));
    run_loop.Run();
    return buffer;
  }

  TestGpuService* gpu_service() const { return gpu_service_.get(); }

  HostGpuMemoryBufferManager* gpu_memory_buffer_manager() const {
    return gpu_memory_buffer_manager_.get();
  }

 protected:
  std::unique_ptr<TestGpuService> gpu_service_;
  std::unique_ptr<HostGpuMemoryBufferManager> gpu_memory_buffer_manager_;
};

// Tests that allocation requests from a client that goes away before allocation
// completes are cleaned up correctly.
TEST_F(HostGpuMemoryBufferManagerTest, AllocationRequestsForDestroyedClient) {
  if (!IsNativePixmapConfigSupported())
    return;

  // Note: HostGpuMemoryBufferManager normally operates on a mojom::GpuService
  // implementation over mojo. Which means the communication from HGMBManager to
  // GpuService is asynchronous. In this test, the mojom::GpuService is not
  // bound to a mojo pipe, which means those calls are all synchronous.

  const auto buffer_id = static_cast<gfx::GpuMemoryBufferId>(1);
  const int client_id = 2;
  const gfx::Size size(10, 20);
  const gfx::BufferFormat format = gfx::BufferFormat::RGBA_8888;
  const gfx::BufferUsage usage = gfx::BufferUsage::GPU_READ;
  gpu_memory_buffer_manager()->AllocateGpuMemoryBuffer(
      buffer_id, client_id, size, format, usage, gpu::kNullSurfaceHandle,
      base::DoNothing());
  EXPECT_EQ(1, gpu_service()->GetAllocationRequestsCount());
  EXPECT_TRUE(gpu_service()->IsAllocationRequestAt(0, buffer_id, client_id));
  EXPECT_EQ(0, gpu_service()->GetDestructionRequestsCount());

  // Destroy the client. Since no memory has been allocated yet, there will be
  // no request for freeing memory.
  gpu_memory_buffer_manager()->DestroyAllGpuMemoryBufferForClient(client_id);
  EXPECT_EQ(1, gpu_service()->GetAllocationRequestsCount());
  EXPECT_EQ(0, gpu_service()->GetDestructionRequestsCount());

  // When the host receives the allocated memory for the destroyed client, it
  // should request the allocated memory to be freed.
  gpu_service()->SatisfyAllocationRequestAt(0);
  EXPECT_EQ(1, gpu_service()->GetAllocationRequestsCount());
  EXPECT_EQ(1, gpu_service()->GetDestructionRequestsCount());
  EXPECT_TRUE(gpu_service()->IsDestructionRequestAt(0, buffer_id, client_id));
}

TEST_F(HostGpuMemoryBufferManagerTest, RequestsFromUntrustedClientsValidated) {
  const auto buffer_id = static_cast<gfx::GpuMemoryBufferId>(1);
  const int client_id = 2;
  // SCANOUT cannot be used if native gpu memory buffer is not supported.
  struct {
    gfx::BufferUsage usage;
    gfx::BufferFormat format;
    gfx::Size size;
    bool expect_null_handle;
  } configs[] = {
      {gfx::BufferUsage::SCANOUT, gfx::BufferFormat::YVU_420, {10, 20}, true},
      {gfx::BufferUsage::GPU_READ, gfx::BufferFormat::YVU_420, {64, 64}, false},
  };
  for (const auto& config : configs) {
    gfx::GpuMemoryBufferHandle allocated_handle;
    base::RunLoop runloop;
    gpu_memory_buffer_manager()->AllocateGpuMemoryBuffer(
        buffer_id, client_id, config.size, config.format, config.usage,
        gpu::kNullSurfaceHandle,
        base::BindOnce(
            [](gfx::GpuMemoryBufferHandle* allocated_handle,
               base::OnceClosure callback, gfx::GpuMemoryBufferHandle handle) {
              *allocated_handle = std::move(handle);
              std::move(callback).Run();
            },
            &allocated_handle, runloop.QuitClosure()));
    // Since native gpu memory buffers are not supported, the mojom.GpuService
    // should not receive any allocation requests.
    EXPECT_EQ(0, gpu_service()->GetAllocationRequestsCount());
    runloop.Run();
    if (config.expect_null_handle) {
      EXPECT_TRUE(allocated_handle.is_null());
    } else {
      EXPECT_FALSE(allocated_handle.is_null());
      EXPECT_EQ(gfx::GpuMemoryBufferType::SHARED_MEMORY_BUFFER,
                allocated_handle.type);
    }
  }
}

TEST_F(HostGpuMemoryBufferManagerTest, GpuMemoryBufferDestroyed) {
  auto buffer = AllocateGpuMemoryBufferSync();
  EXPECT_TRUE(buffer);
  buffer.reset();
}

TEST_F(HostGpuMemoryBufferManagerTest,
       GpuMemoryBufferDestroyedOnDifferentThread) {
  auto buffer = AllocateGpuMemoryBufferSync();
  EXPECT_TRUE(buffer);
  // Destroy the buffer in a different thread.
  base::Thread diff_thread("DestroyThread");
  ASSERT_TRUE(diff_thread.Start());
  diff_thread.task_runner()->DeleteSoon(FROM_HERE, std::move(buffer));
  diff_thread.Stop();
}

// Tests that if an allocated buffer is received after the gpu service issuing
// it has died, HGMBManager retries the allocation request properly.
TEST_F(HostGpuMemoryBufferManagerTest, AllocationRequestFromDeadGpuService) {
  if (!IsNativePixmapConfigSupported())
    return;

  // Request allocation. No allocation should happen yet.
  gfx::GpuMemoryBufferHandle allocated_handle;
  const auto buffer_id = static_cast<gfx::GpuMemoryBufferId>(1);
  const int client_id = 2;
  const gfx::Size size(10, 20);
  const gfx::BufferFormat format = gfx::BufferFormat::RGBA_8888;
  const gfx::BufferUsage usage = gfx::BufferUsage::GPU_READ;
  gpu_memory_buffer_manager()->AllocateGpuMemoryBuffer(
      buffer_id, client_id, size, format, usage, gpu::kNullSurfaceHandle,
      base::BindOnce(
          [](gfx::GpuMemoryBufferHandle* allocated_handle,
             gfx::GpuMemoryBufferHandle handle) {
            *allocated_handle = std::move(handle);
          },
          &allocated_handle));
  EXPECT_EQ(1, gpu_service()->GetAllocationRequestsCount());
  EXPECT_TRUE(gpu_service()->IsAllocationRequestAt(0, buffer_id, client_id));
  EXPECT_TRUE(allocated_handle.is_null());

  // Simulate a connection error from gpu. HGMBManager should retry allocation
  // request.
  gpu_service()->SimulateConnectionError();
  EXPECT_EQ(2, gpu_service()->GetAllocationRequestsCount());
  EXPECT_TRUE(gpu_service()->IsAllocationRequestAt(1, buffer_id, client_id));
  EXPECT_TRUE(allocated_handle.is_null());

  // Send an allocated buffer corresponding to the first request on the old gpu.
  // This should not result in a buffer handle.
  gpu_service()->SatisfyAllocationRequestAt(0);
  EXPECT_EQ(2, gpu_service()->GetAllocationRequestsCount());
  EXPECT_TRUE(allocated_handle.is_null());

  // Send an allocated buffer corresponding to the retried request on the new
  // gpu. This should result in a buffer handle.
  gpu_service()->SatisfyAllocationRequestAt(1);
  EXPECT_EQ(2, gpu_service()->GetAllocationRequestsCount());
  EXPECT_FALSE(allocated_handle.is_null());
}

// Test that any pending CreateGpuMemoryBuffer() requests are cancelled, so
// blocked threads stop waiting, on shutdown.
TEST_F(HostGpuMemoryBufferManagerTest, CancelRequestsForShutdown) {
  base::Thread threads[2] = {base::Thread("Thread1"), base::Thread("Thread2")};

  for (auto& thread : threads) {
    ASSERT_TRUE(thread.Start());
    base::WaitableEvent create_wait;

    // Call CreateGpuMemoryBuffer() from each thread. This thread will be
    // waiting inside CreateGpuMemoryBuffer() when `gpu_memory_buffer_manager_`
    // is destroyed.
    thread.task_runner()->PostTask(
        FROM_HERE, base::BindLambdaForTesting([this, &create_wait]() {
          create_wait.Signal();
          // This should block.
          gpu_memory_buffer_manager_->CreateGpuMemoryBuffer(
              gfx::Size(100, 100), gfx::BufferFormat::RGBA_8888,
              gfx::BufferUsage::SCANOUT, gpu::kNullSurfaceHandle, nullptr);
        }));
    create_wait.Wait();
  }

  // This should shutdown HostGpuMemoryBufferManager and unblock the other
  // threads.
  gpu_memory_buffer_manager_->Shutdown();

  // Stop the other threads to verify they aren't waiting.
  for (auto& thread : threads)
    thread.Stop();

  // HostGpuMemoryBufferManager should be able to be safely destroyed after
  //
  gpu_memory_buffer_manager_.reset();

  // Flush tasks posted back to main thread from CreateGpuMemoryBuffer() to make
  // sure they are harmless.
  base::RunLoop loop;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                           loop.QuitClosure());
  loop.Run();
}

}  // namespace viz
