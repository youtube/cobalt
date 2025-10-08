// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"

namespace gpu {

GpuMemoryBufferManager::GpuMemoryBufferManager() = default;

GpuMemoryBufferManager::~GpuMemoryBufferManager() = default;

<<<<<<< HEAD
void GpuMemoryBufferManager::AddObserver(
    GpuMemoryBufferManagerObserver* observer) {}
=======
GpuMemoryBufferManager::AllocatedBufferInfo::AllocatedBufferInfo(
    const gfx::GpuMemoryBufferHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format)
    : buffer_id_(handle.id),
      type_(handle.type),
      size_in_bytes_(gfx::BufferSizeForBufferFormat(size, format)) {
#if BUILDFLAG(IS_COBALT_HERMETIC_BUILD)
  DCHECK_NE(gfx::EMPTY_BUFFER, type_);
#endif
>>>>>>> 1ee87d70a83 ([Raspi] Prevent GpuMemoryBuffer crash on modular_devel build (#6646))

void GpuMemoryBufferManager::RemoveObserver(
    GpuMemoryBufferManagerObserver* observer) {}

void GpuMemoryBufferManager::NotifyObservers() {
  for (auto& observer : observers_) {
    observer.OnGpuMemoryBufferManagerDestroyed();
  }
}

}  // namespace gpu
