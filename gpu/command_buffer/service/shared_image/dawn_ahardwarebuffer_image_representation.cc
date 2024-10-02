// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/dawn_ahardwarebuffer_image_representation.h"

#include <dawn/native/VulkanBackend.h>

#include "base/logging.h"

namespace gpu {

DawnAHardwareBufferImageRepresentation::DawnAHardwareBufferImageRepresentation(
    SharedImageManager* manager,
    AndroidImageBacking* backing,
    MemoryTypeTracker* tracker,
    WGPUDevice device,
    WGPUTextureFormat format,
    std::vector<WGPUTextureFormat> view_formats,
    AHardwareBuffer* buffer,
    scoped_refptr<base::RefCountedData<DawnProcTable>> dawn_procs)
    : DawnImageRepresentation(manager, backing, tracker),
      device_(device),
      format_(format),
      view_formats_(std::move(view_formats)),
      dawn_procs_(dawn_procs) {
  DCHECK(device_);

  // Keep a reference to the device so that it stays valid (it might become
  // lost in which case operations will be noops).
  dawn_procs_->data.deviceReference(device_);

  handle_ = base::android::ScopedHardwareBufferHandle::Create(buffer);
}

DawnAHardwareBufferImageRepresentation::
    ~DawnAHardwareBufferImageRepresentation() {
  EndAccess();
  dawn_procs_->data.deviceRelease(device_);
}

WGPUTexture DawnAHardwareBufferImageRepresentation::BeginAccess(
    WGPUTextureUsage usage) {
  // It doesn't make sense to have two overlapping BeginAccess calls on the same
  // representation.
  if (texture_) {
    LOG(ERROR) << "Attempting to begin access before ending previous access.";
    return nullptr;
  }

  WGPUTextureDescriptor texture_descriptor = {};
  texture_descriptor.format = format_;
  texture_descriptor.usage = usage;
  texture_descriptor.dimension = WGPUTextureDimension_2D;
  texture_descriptor.size = {static_cast<uint32_t>(size().width()),
                             static_cast<uint32_t>(size().height()), 1};
  texture_descriptor.mipLevelCount = 1;
  texture_descriptor.sampleCount = 1;
  texture_descriptor.viewFormatCount =
      static_cast<uint32_t>(view_formats_.size());
  texture_descriptor.viewFormats = view_formats_.data();

  // We need to have internal usages of CopySrc for copies,
  // RenderAttachment for clears, and TextureBinding for copyTextureForBrowser.
  WGPUDawnTextureInternalUsageDescriptor internalDesc = {};
  internalDesc.chain.sType = WGPUSType_DawnTextureInternalUsageDescriptor;
  internalDesc.internalUsage = WGPUTextureUsage_CopySrc |
                               WGPUTextureUsage_RenderAttachment |
                               WGPUTextureUsage_TextureBinding;

  texture_descriptor.nextInChain =
      reinterpret_cast<WGPUChainedStruct*>(&internalDesc);

  dawn::native::vulkan::ExternalImageDescriptorAHardwareBuffer descriptor = {};
  descriptor.cTextureDescriptor = &texture_descriptor;
  descriptor.isInitialized = IsCleared();
  descriptor.handle = handle_.get();
  descriptor.waitFDs = {};

  // Dawn currently doesn't support read-only access and hence concurrent reads.
  base::ScopedFD sync_fd;
  android_backing()->BeginWrite(&sync_fd);

  // If the semaphore from BeginWrite is valid then pass it to WrapVulkanImage.
  if (sync_fd.is_valid())
    descriptor.waitFDs.push_back(sync_fd.release());

  texture_ = dawn::native::vulkan::WrapVulkanImage(device_, &descriptor);

  if (!texture_) {
    LOG(ERROR) << "Failed to wrap AHardwareBuffer as a Dawn texture.";
    android_backing()->EndWrite(base::ScopedFD());
  }

  return texture_;
}

void DawnAHardwareBufferImageRepresentation::EndAccess() {
  if (!texture_) {
    return;
  }

  dawn::native::vulkan::ExternalImageExportInfoAHardwareBuffer export_info;
  if (!dawn::native::vulkan::ExportVulkanImage(
          texture_, VK_IMAGE_LAYOUT_UNDEFINED, &export_info)) {
    DLOG(ERROR) << "Failed to export Dawn Vulkan image.";
  } else {
    if (export_info.isInitialized)
      SetCleared();

    // TODO(dawn:286): Handle waiting on multiple semaphores from dawn.
    DCHECK_EQ(export_info.semaphoreHandles.size(), 1u);
    base::ScopedFD sync_fd = base::ScopedFD(export_info.semaphoreHandles[0]);
    android_backing()->EndWrite(std::move(sync_fd));
  }

  dawn_procs_->data.textureDestroy(texture_);
  dawn_procs_->data.textureRelease(texture_);
  texture_ = nullptr;
}

}  // namespace gpu
