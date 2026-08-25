// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_NATIVE_PIXMAP_H_
#define GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_NATIVE_PIXMAP_H_

<<<<<<< HEAD
#include <vulkan/vulkan_core.h>

=======
#include <unordered_map>
#include <utility>

#include "base/hash/hash.h"
#include "base/synchronization/lock.h"
>>>>>>> parent of cfab790ea65 (CONFLICTED Chromium Cherry pick: Revert Cobalt.)
#include "gpu/ipc/service/gpu_ipc_service_export.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "ui/gfx/native_pixmap.h"

// TODO: (cobalt b/409766462): Exclude this file from the build entirely.
#include "build/build_config.h"
#if !BUILDFLAG(ENABLE_COBALT_HERMETIC_HACKS)
#include <vulkan/vulkan_core.h>
#endif

namespace gpu {

class VulkanDeviceQueue;

class GPU_IPC_SERVICE_EXPORT GpuMemoryBufferFactoryNativePixmap
    : public GpuMemoryBufferFactory {
 public:
  GpuMemoryBufferFactoryNativePixmap();
  explicit GpuMemoryBufferFactoryNativePixmap(
      viz::VulkanContextProvider* vulkan_context_provider);

  GpuMemoryBufferFactoryNativePixmap(
      const GpuMemoryBufferFactoryNativePixmap&) = delete;
  GpuMemoryBufferFactoryNativePixmap& operator=(
      const GpuMemoryBufferFactoryNativePixmap&) = delete;

  ~GpuMemoryBufferFactoryNativePixmap() override;

  // Overridden from GpuMemoryBufferFactory:
  gfx::GpuMemoryBufferHandle CreateNativeGmbHandle(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override;
  bool FillSharedMemoryRegionWithBufferContents(
      gfx::GpuMemoryBufferHandle buffer_handle,
      base::UnsafeSharedMemoryRegion shared_memory) override;

 private:
  gfx::GpuMemoryBufferHandle CreateNativeGmbHandleFromNativePixmap(
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      scoped_refptr<gfx::NativePixmap> pixmap);

  VulkanDeviceQueue* GetVulkanDeviceQueue();

  scoped_refptr<viz::VulkanContextProvider> vulkan_context_provider_;

  base::WeakPtrFactory<GpuMemoryBufferFactoryNativePixmap> weak_factory_{this};
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_GPU_MEMORY_BUFFER_FACTORY_NATIVE_PIXMAP_H_
