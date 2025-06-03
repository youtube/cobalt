// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_VIDEO_CAPTURE_GPU_MEMORY_BUFFER_TEST_SUPPORT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_VIDEO_CAPTURE_GPU_MEMORY_BUFFER_TEST_SUPPORT_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "third_party/blink/renderer/platform/testing/io_task_runner_testing_platform_support.h"

namespace media {
class MockGpuVideoAcceleratorFactories;
}  // namespace media

namespace viz {
class TestSharedImageInterface;
}  // namespace viz

namespace gpu {
struct Capabilities;
struct SharedImageCapabilities;
}  // namespace gpu

namespace blink {

class FakeGpuMemoryBufferSupport : public gpu::GpuMemoryBufferSupport {
 public:
  std::unique_ptr<gpu::GpuMemoryBufferImpl> CreateGpuMemoryBufferImplFromHandle(
      gfx::GpuMemoryBufferHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      gpu::GpuMemoryBufferImpl::DestructionCallback callback,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager = nullptr,
      scoped_refptr<base::UnsafeSharedMemoryPool> pool = nullptr,
      base::span<uint8_t> premapped_memory = base::span<uint8_t>()) override;
};

class TestingPlatformSupportForGpuMemoryBuffer
    : public IOTaskRunnerTestingPlatformSupport {
 public:
  TestingPlatformSupportForGpuMemoryBuffer();
  ~TestingPlatformSupportForGpuMemoryBuffer() override;
  media::GpuVideoAcceleratorFactories* GetGpuFactories() override;

  void SetGpuCapabilities(gpu::Capabilities* capabilities);
  void SetSharedImageCapabilities(
      const gpu::SharedImageCapabilities& capabilities);

 private:
  std::unique_ptr<viz::TestSharedImageInterface> sii_;
  std::unique_ptr<media::MockGpuVideoAcceleratorFactories> gpu_factories_;
  base::Thread media_thread_;
  raw_ptr<gpu::Capabilities, ExperimentalRenderer> capabilities_ = nullptr;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_VIDEO_CAPTURE_GPU_MEMORY_BUFFER_TEST_SUPPORT_H_
