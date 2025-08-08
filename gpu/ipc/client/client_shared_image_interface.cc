// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/client/client_shared_image_interface.h"

#include "build/build_config.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "gpu/command_buffer/client/client_shared_image.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/shared_image_capabilities.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/ipc/client/shared_image_interface_proxy.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {

ClientSharedImageInterface::ClientSharedImageInterface(
    SharedImageInterfaceProxy* proxy)
    : proxy_(proxy) {}

ClientSharedImageInterface::~ClientSharedImageInterface() {
  gpu::SyncToken sync_token;
  auto mailboxes_to_delete = mailboxes_;
  for (const auto& mailbox : mailboxes_to_delete)
    DestroySharedImage(sync_token, mailbox);
}

void ClientSharedImageInterface::UpdateSharedImage(const SyncToken& sync_token,
                                                   const Mailbox& mailbox) {
  proxy_->UpdateSharedImage(sync_token, mailbox);
}

void ClientSharedImageInterface::UpdateSharedImage(
    const SyncToken& sync_token,
    std::unique_ptr<gfx::GpuFence> acquire_fence,
    const Mailbox& mailbox) {
  proxy_->UpdateSharedImage(sync_token, std::move(acquire_fence), mailbox);
}

void ClientSharedImageInterface::PresentSwapChain(const SyncToken& sync_token,
                                                  const Mailbox& mailbox) {
  proxy_->PresentSwapChain(sync_token, mailbox);
}

#if BUILDFLAG(IS_FUCHSIA)
void ClientSharedImageInterface::RegisterSysmemBufferCollection(
    zx::eventpair service_handle,
    zx::channel sysmem_token,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    bool register_with_image_pipe) {
  proxy_->RegisterSysmemBufferCollection(std::move(service_handle),
                                         std::move(sysmem_token), format, usage,
                                         register_with_image_pipe);
}
#endif  // BUILDFLAG(IS_FUCHSIA)

SyncToken ClientSharedImageInterface::GenUnverifiedSyncToken() {
  return proxy_->GenUnverifiedSyncToken();
}

SyncToken ClientSharedImageInterface::GenVerifiedSyncToken() {
  return proxy_->GenVerifiedSyncToken();
}

void ClientSharedImageInterface::WaitSyncToken(
    const gpu::SyncToken& sync_token) {
  proxy_->WaitSyncToken(sync_token);
}

void ClientSharedImageInterface::Flush() {
  proxy_->Flush();
}

scoped_refptr<gfx::NativePixmap> ClientSharedImageInterface::GetNativePixmap(
    const gpu::Mailbox& mailbox) {
  return proxy_->GetNativePixmap(mailbox);
}

Mailbox ClientSharedImageInterface::CreateSharedImage(
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::StringPiece debug_label,
    gpu::SurfaceHandle surface_handle) {
  DCHECK_EQ(surface_handle, kNullSurfaceHandle);
  DCHECK(gpu::IsValidClientUsage(usage)) << usage;
  return AddMailbox(proxy_->CreateSharedImage(format, size, color_space,
                                              surface_origin, alpha_type, usage,
                                              debug_label));
}

scoped_refptr<ClientSharedImage> ClientSharedImageInterface::CreateSharedImage(
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::StringPiece debug_label,
    base::span<const uint8_t> pixel_data) {
  // Pixel upload path only supports single-planar formats.
  DCHECK(format.is_single_plane()) << format.ToString();
  DCHECK(gpu::IsValidClientUsage(usage)) << usage;

  // EstimatedSizeInBytes() returns the minimum size in bytes needed to store
  // `format` at `size` so if span is smaller there is a problem.
  CHECK_GE(pixel_data.size(), format.EstimatedSizeInBytes(size));

  return base::MakeRefCounted<ClientSharedImage>(AddMailbox(
      proxy_->CreateSharedImage(format, size, color_space, surface_origin,
                                alpha_type, usage, debug_label, pixel_data)));
}

scoped_refptr<ClientSharedImage> ClientSharedImageInterface::CreateSharedImage(
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::StringPiece debug_label,
    gpu::SurfaceHandle surface_handle,
    gfx::BufferUsage buffer_usage) {
  DCHECK_EQ(surface_handle, kNullSurfaceHandle);
  DCHECK(gpu::IsValidClientUsage(usage)) << usage;
  auto mailbox =
      proxy_->CreateSharedImage(format, size, color_space, surface_origin,
                                alpha_type, usage, debug_label, buffer_usage);
  if (mailbox.IsZero()) {
    return nullptr;
  }
  return base::MakeRefCounted<ClientSharedImage>(AddMailbox(mailbox));
}

scoped_refptr<ClientSharedImage> ClientSharedImageInterface::CreateSharedImage(
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::StringPiece debug_label,
    gfx::GpuMemoryBufferHandle buffer_handle) {
  DCHECK(gpu::IsValidClientUsage(usage)) << usage;
  DCHECK(viz::HasEquivalentBufferFormat(format)) << format.ToString();
  CHECK(!format.IsLegacyMultiplanar()) << format.ToString();
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  CHECK(!format.PrefersExternalSampler()) << format.ToString();
#endif
  return base::MakeRefCounted<ClientSharedImage>(
      AddMailbox(proxy_->CreateSharedImage(
          format, size, color_space, surface_origin, alpha_type, usage,
          debug_label, std::move(buffer_handle))));
}

Mailbox ClientSharedImageInterface::CreateSharedImage(
    gfx::GpuMemoryBuffer* gpu_memory_buffer,
    GpuMemoryBufferManager* gpu_memory_buffer_manager,
    gfx::BufferPlane plane,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::StringPiece debug_label) {
  DCHECK(gpu::IsValidClientUsage(usage)) << usage;
  auto buffer_format = gpu_memory_buffer->GetFormat();
  CHECK(gpu::IsPlaneValidForGpuMemoryBufferFormat(plane, buffer_format));
  return AddMailbox(proxy_->CreateSharedImage(
      buffer_format, plane, gpu_memory_buffer->GetSize(), color_space,
      surface_origin, alpha_type, usage, debug_label,
      gpu_memory_buffer->CloneHandle()));
}

#if BUILDFLAG(IS_WIN)
void ClientSharedImageInterface::CopyToGpuMemoryBuffer(
    const SyncToken& sync_token,
    const Mailbox& mailbox) {
  proxy_->CopyToGpuMemoryBuffer(sync_token, mailbox);
}
#endif

ClientSharedImageInterface::SwapChainMailboxes
ClientSharedImageInterface::CreateSwapChain(viz::SharedImageFormat format,
                                            const gfx::Size& size,
                                            const gfx::ColorSpace& color_space,
                                            GrSurfaceOrigin surface_origin,
                                            SkAlphaType alpha_type,
                                            uint32_t usage) {
  DCHECK(gpu::IsValidClientUsage(usage));
  auto mailboxes = proxy_->CreateSwapChain(format, size, color_space,
                                           surface_origin, alpha_type, usage);
  AddMailbox(mailboxes.front_buffer);
  AddMailbox(mailboxes.back_buffer);
  return mailboxes;
}

void ClientSharedImageInterface::DestroySharedImage(const SyncToken& sync_token,
                                                    const Mailbox& mailbox) {
  DCHECK(!mailbox.IsZero());

  {
    base::AutoLock lock(lock_);
    auto it = mailboxes_.find(mailbox);
    CHECK(it != mailboxes_.end());
    mailboxes_.erase(it);
  }
  proxy_->DestroySharedImage(sync_token, mailbox);
}

void ClientSharedImageInterface::AddReferenceToSharedImage(
    const SyncToken& sync_token,
    const Mailbox& mailbox,
    uint32_t usage) {
  DCHECK(!mailbox.IsZero());
  AddMailbox(mailbox);
  proxy_->AddReferenceToSharedImage(sync_token, mailbox, usage);
}

std::unique_ptr<SharedImageInterface::ScopedMapping>
ClientSharedImageInterface::MapSharedImage(const Mailbox& mailbox) {
  auto* gpu_memory_buffer = proxy_->GetGpuMemoryBuffer(mailbox);
  if (!gpu_memory_buffer) {
    LOG(ERROR) << "Buffer is null.";
    return nullptr;
  }
  auto scoped_mapping =
      SharedImageInterface::ScopedMapping::Create(gpu_memory_buffer);
  if (!scoped_mapping) {
    LOG(ERROR) << "Unable to create ScopedMapping.";
    return nullptr;
  }
  return scoped_mapping;
}

uint32_t ClientSharedImageInterface::UsageForMailbox(const Mailbox& mailbox) {
  return proxy_->UsageForMailbox(mailbox);
}

void ClientSharedImageInterface::NotifyMailboxAdded(const Mailbox& mailbox,
                                                    uint32_t usage) {
  AddMailbox(mailbox);
  proxy_->NotifyMailboxAdded(mailbox, usage);
}

Mailbox ClientSharedImageInterface::AddMailbox(const gpu::Mailbox& mailbox) {
  if (mailbox.IsZero())
    return mailbox;

  base::AutoLock lock(lock_);
  mailboxes_.insert(mailbox);
  return mailbox;
}

const SharedImageCapabilities& ClientSharedImageInterface::GetCapabilities() {
  return proxy_->GetCapabilities();
}

}  // namespace gpu
