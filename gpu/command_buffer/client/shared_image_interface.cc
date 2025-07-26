// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/shared_image_interface.h"

#include <GLES2/gl2.h>

#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/process/memory.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "ui/gfx/buffer_format_util.h"

#if BUILDFLAG(IS_WIN)
#include "ui/gfx/win/d3d_shared_fence.h"
#endif

namespace gpu {

// static
void SharedImageInterface::CreateSharedMemoryRegionFromSIInfo(
    const SharedImageInfo& si_info,
    base::WritableSharedMemoryMapping& mapping,
    gfx::GpuMemoryBufferHandle& handle) {
  DCHECK(gpu::IsValidClientUsage(si_info.meta.usage))
      << uint32_t(si_info.meta.usage);
  DCHECK_EQ(si_info.meta.usage,
            gpu::SharedImageUsageSet(gpu::SHARED_IMAGE_USAGE_CPU_WRITE_ONLY));
  DCHECK(viz::HasEquivalentBufferFormat(si_info.meta.format))
      << si_info.meta.format.ToString();
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  CHECK(!si_info.meta.format.PrefersExternalSampler())
      << si_info.meta.format.ToString();
#endif

  gfx::BufferFormat buffer_format =
      viz::SinglePlaneSharedImageFormatToBufferFormat(si_info.meta.format);
  const size_t buffer_size =
      gfx::BufferSizeForBufferFormat(si_info.meta.size, buffer_format);
  auto shared_memory_region =
      base::UnsafeSharedMemoryRegion::Create(buffer_size);

  if (!shared_memory_region.IsValid()) {
    DLOG(ERROR) << "base::UnsafeSharedMemoryRegion::Create() for SharedImage "
                   "with SHARED_IMAGE_USAGE_CPU_WRITE_ONLY fails!";
    base::TerminateBecauseOutOfMemory(buffer_size);
  }

  mapping = shared_memory_region.Map();
  if (!mapping.IsValid()) {
    DLOG(ERROR) << "shared_memory_region.Map() for "
                   "SHARED_IMAGE_USAGE_CPU_WRITE_ONLY fails!";
    base::TerminateBecauseOutOfMemory(buffer_size);
  }

  handle.type = gfx::SHARED_MEMORY_BUFFER;
  handle.offset = 0;
  handle.stride = static_cast<int32_t>(
      gfx::RowSizeForBufferFormat(si_info.meta.size.width(), buffer_format, 0));
  handle.set_region(std::move(shared_memory_region));
}

SharedImageInterface::SwapChainSharedImages::SwapChainSharedImages(
    scoped_refptr<gpu::ClientSharedImage> front_buffer,
    scoped_refptr<gpu::ClientSharedImage> back_buffer)
    : front_buffer(std::move(front_buffer)),
      back_buffer(std::move(back_buffer)) {}
SharedImageInterface::SwapChainSharedImages::SwapChainSharedImages(
    const SwapChainSharedImages& shared_images) = default;
SharedImageInterface::SwapChainSharedImages::~SwapChainSharedImages() = default;

SharedImageInterface::SharedImageInterface()
    : holder_(base::MakeRefCounted<SharedImageInterfaceHolder>(this)) {}
SharedImageInterface::~SharedImageInterface() = default;

scoped_refptr<ClientSharedImage> SharedImageInterface::CreateSharedImage(
    const SharedImageInfo& si_info,
    gpu::SurfaceHandle surface_handle,
    gfx::BufferUsage buffer_usage,
    std::optional<SharedImagePoolId> pool_id) {
  NOTREACHED();
}

scoped_refptr<ClientSharedImage> SharedImageInterface::NotifyMailboxAdded(
    const Mailbox& /*mailbox*/,
    viz::SharedImageFormat /*format*/,
    const gfx::Size& /*size*/,
    const gfx::ColorSpace& /*color_space*/,
    GrSurfaceOrigin /*surface_origin*/,
    SkAlphaType /*alpha_type*/,
    SharedImageUsageSet /*usage*/) {
  return nullptr;
}

scoped_refptr<ClientSharedImage> SharedImageInterface::NotifyMailboxAdded(
    const Mailbox& /*mailbox*/,
    viz::SharedImageFormat /*format*/,
    const gfx::Size& /*size*/,
    const gfx::ColorSpace& /*color_space*/,
    GrSurfaceOrigin /*surface_origin*/,
    SkAlphaType /*alpha_type*/,
    SharedImageUsageSet /*usage*/,
    uint32_t /*texture_target*/) {
  return nullptr;
}

void SharedImageInterface::CopyToGpuMemoryBuffer(const SyncToken& sync_token,
                                                 const Mailbox& mailbox) {
  NOTREACHED();
}

void SharedImageInterface::CopyToGpuMemoryBufferAsync(
    const SyncToken& sync_token,
    const Mailbox& mailbox,
    base::OnceCallback<void(bool)> callback) {
  NOTREACHED();
}

bool SharedImageInterface::CopyNativeGmbToSharedMemorySync(
    gfx::GpuMemoryBufferHandle buffer_handle,
    base::UnsafeSharedMemoryRegion memory_region) {
  NOTREACHED();
}

void SharedImageInterface::CopyNativeGmbToSharedMemoryAsync(
    gfx::GpuMemoryBufferHandle buffer_handle,
    base::UnsafeSharedMemoryRegion memory_region,
    base::OnceCallback<void(bool)> callback) {
  NOTREACHED();
}

bool SharedImageInterface::IsConnected() {
  NOTREACHED();
}

void SharedImageInterface::Release() const {
  bool should_destroy = false;

  {
    base::AutoLock auto_lock(holder_->lock_);
    if (base::subtle::RefCountedThreadSafeBase::Release()) {
      ANALYZER_SKIP_THIS_PATH();
      holder_->OnDestroy();
      should_destroy = true;
    }
  }

  if (should_destroy) {
    delete this;
  }
}

#if BUILDFLAG(IS_WIN)
void SharedImageInterface::UpdateSharedImage(
    const SyncToken& sync_token,
    scoped_refptr<gfx::D3DSharedFence> d3d_shared_fence,
    const Mailbox& mailbox) {
  NOTIMPLEMENTED_LOG_ONCE();
}
#endif  // BUILDFLAG(IS_WIN)

void SharedImageInterface::CreateSharedImagePool(
    const SharedImagePoolId& pool_id,
    mojo::PendingRemote<mojom::SharedImagePoolClientInterface> client_remote) {
  NOTREACHED();
}

void SharedImageInterface::DestroySharedImagePool(
    const SharedImagePoolId& pool_id) {
  NOTREACHED();
}

SharedImageInterfaceHolder::SharedImageInterfaceHolder(
    SharedImageInterface* sii)
    : sii_(sii) {}
SharedImageInterfaceHolder::~SharedImageInterfaceHolder() = default;

scoped_refptr<SharedImageInterface> SharedImageInterfaceHolder::Get() {
  base::AutoLock auto_lock(lock_);
  return scoped_refptr<SharedImageInterface>(sii_);
}

void SharedImageInterfaceHolder::OnDestroy() {
  lock_.AssertAcquired();
  sii_ = nullptr;
}

}  // namespace gpu
