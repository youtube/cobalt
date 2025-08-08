// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/shared_image_backing_factory.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"

namespace gpu {

SharedImageBackingFactory::SharedImageBackingFactory(uint32_t valid_usages)
    : invalid_usages_(~valid_usages) {}

SharedImageBackingFactory::~SharedImageBackingFactory() = default;

base::WeakPtr<SharedImageBackingFactory>
SharedImageBackingFactory::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

std::unique_ptr<SharedImageBacking>
SharedImageBackingFactory::CreateSharedImage(const Mailbox& mailbox,
                                             viz::SharedImageFormat format,
                                             SurfaceHandle surface_handle,
                                             const gfx::Size& size,
                                             const gfx::ColorSpace& color_space,
                                             GrSurfaceOrigin surface_origin,
                                             SkAlphaType alpha_type,
                                             uint32_t usage,
                                             std::string debug_label,
                                             bool is_thread_safe,
                                             gfx::BufferUsage buffer_usage) {
  NOTREACHED();
  return nullptr;
}

bool SharedImageBackingFactory::CanCreateSharedImage(
    uint32_t usage,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    bool thread_safe,
    gfx::GpuMemoryBufferType gmb_type,
    GrContextType gr_context_type,
    base::span<const uint8_t> pixel_data) {
  if (invalid_usages_ & usage) {
    // This factory doesn't support all the usages.
    return false;
  }

  return IsSupported(usage, format, size, thread_safe, gmb_type,
                     gr_context_type, pixel_data);
}

void SharedImageBackingFactory::InvalidateWeakPtrsForTesting() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

}  // namespace gpu
