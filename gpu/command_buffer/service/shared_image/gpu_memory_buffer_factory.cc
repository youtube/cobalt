// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/gpu_memory_buffer_factory.h"

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "gpu/ipc/common/gpu_client_ids.h"

#if BUILDFLAG(IS_APPLE)
#include "gpu/command_buffer/service/shared_image/gpu_memory_buffer_factory_io_surface.h"
#endif

<<<<<<< HEAD:gpu/command_buffer/service/shared_image/gpu_memory_buffer_factory.cc
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
#include "gpu/command_buffer/service/shared_image/gpu_memory_buffer_factory_native_pixmap.h"
=======
#if BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_STARBOARD) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
#include "gpu/ipc/service/gpu_memory_buffer_factory_native_pixmap.h"
>>>>>>> parent of c8aad912c2a (CONFLICTED Chromium Cherry pick: Revert Cobalt.):gpu/ipc/service/gpu_memory_buffer_factory.cc
#endif

#if BUILDFLAG(IS_WIN)
#include "gpu/command_buffer/service/shared_image/gpu_memory_buffer_factory_dxgi.h"
#endif

namespace gpu {

// static
std::unique_ptr<GpuMemoryBufferFactory>
GpuMemoryBufferFactory::CreateNativeType(
    viz::VulkanContextProvider* vulkan_context_provider,
    scoped_refptr<base::SingleThreadTaskRunner> io_runner) {
#if BUILDFLAG(IS_APPLE)
  return std::make_unique<GpuMemoryBufferFactoryIOSurface>();
#elif BUILDFLAG(IS_ANDROID)
  // Android does not support creating native GMBs (i.e., from
  // AHardwareBuffers), but the codebase is structured such that it is easier
  // to create a dummy factory than create no factory.
  return std::make_unique<GpuMemoryBufferFactory>();
#elif BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_STARBOARD) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA)
  return std::make_unique<GpuMemoryBufferFactoryNativePixmap>(
      vulkan_context_provider);
#elif BUILDFLAG(IS_WIN)
  return std::make_unique<GpuMemoryBufferFactoryDXGI>(std::move(io_runner));
#else
  return nullptr;
#endif
}

bool GpuMemoryBufferFactory::FillSharedMemoryRegionWithBufferContents(
    gfx::GpuMemoryBufferHandle buffer_handle,
    base::UnsafeSharedMemoryRegion shared_memory) {
  return false;
}

}  // namespace gpu
