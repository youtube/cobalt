// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/xr_raster_frame_transport_delegate.h"

#include "gpu/command_buffer/client/raster_interface.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"

namespace blink {

void XRRasterFrameTransportDelegate::WaitOnFence(gfx::GpuFence* fence) {}

gpu::SyncToken XRRasterFrameTransportDelegate::GenerateSyncToken() {
  auto wrapper = SharedGpuContext::ContextProviderWrapper();
  if (!wrapper) {
    return gpu::SyncToken();
  }
  gpu::raster::RasterInterface* ri =
      wrapper->ContextProvider().RasterInterface();
  if (!ri) {
    return gpu::SyncToken();
  }
  gpu::SyncToken sync_token;
  ri->GenSyncTokenCHROMIUM(sync_token.GetData());
  ri->Flush();
  return sync_token;
}

void XRRasterFrameTransportDelegate::VerifySyncToken(
    gpu::SyncToken& sync_token) {}

std::pair<gfx::GpuMemoryBufferHandle, gpu::SyncToken>
XRRasterFrameTransportDelegate::CopyImage(SharedImageHolder* image,
                                          bool last_transfer_succeeded) {
  return {gfx::GpuMemoryBufferHandle(), gpu::SyncToken()};
}

bool XRRasterFrameTransportDelegate::IsContextLost() {
  auto wrapper = SharedGpuContext::ContextProviderWrapper();
  if (!wrapper || wrapper->ContextProvider().IsContextLost()) {
    return true;
  }
  // We rely on the RasterInterface to create sync tokens.
  return !wrapper->ContextProvider().RasterInterface();
}
}  // namespace blink
