// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_MAC_VULKAN_IMPLEMENTATION_MAC_H_
#define GPU_VULKAN_MAC_VULKAN_IMPLEMENTATION_MAC_H_

#include "base/component_export.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_instance.h"

namespace gpu {

class COMPONENT_EXPORT(VULKAN_MAC) VulkanImplementationMac
    : public VulkanImplementation {
 public:
  explicit VulkanImplementationMac(bool use_swiftshader);

  VulkanImplementationMac(const VulkanImplementationMac&) = delete;
  VulkanImplementationMac& operator=(const VulkanImplementationMac&) = delete;

  ~VulkanImplementationMac() override;

  // VulkanImplementation:
  bool InitializeVulkanInstance(bool using_surface) override;
  VulkanInstance* GetVulkanInstance() override;
  std::unique_ptr<VulkanSurface> CreateViewSurface(
      gfx::AcceleratedWidget window) override;
  bool GetPhysicalDevicePresentationSupport(
      VkPhysicalDevice device,
      const std::vector<VkQueueFamilyProperties>& queue_family_properties,
      uint32_t queue_family_index) override;
  std::vector<const char*> GetRequiredDeviceExtensions() override;
  std::vector<const char*> GetOptionalDeviceExtensions() override;
  VkFence CreateVkFenceForGpuFence(VkDevice vk_device) override;
  std::unique_ptr<gfx::GpuFence> ExportVkFenceToGpuFence(
      VkDevice vk_device,
      VkFence vk_fence) override;
  VkSemaphore CreateExternalSemaphore(VkDevice vk_device) override;
  VkSemaphore ImportSemaphoreHandle(VkDevice vk_device,
                                    SemaphoreHandle handle) override;
  SemaphoreHandle GetSemaphoreHandle(VkDevice vk_device,
                                     VkSemaphore vk_semaphore) override;
  VkExternalMemoryHandleTypeFlagBits GetExternalImageHandleType() override;
  bool CanImportGpuMemoryBuffer(
      VulkanDeviceQueue* device_queue,
      gfx::GpuMemoryBufferType memory_buffer_type) override;
  std::unique_ptr<VulkanImage> CreateImageFromGpuMemoryHandle(
      VulkanDeviceQueue* device_queue,
      gfx::GpuMemoryBufferHandle gmb_handle,
      gfx::Size size,
      VkFormat vk_format,
      const gfx::ColorSpace& color_space) override;

 private:
  VulkanInstance vulkan_instance_;
};

}  // namespace gpu

#endif  // GPU_VULKAN_MAC_VULKAN_IMPLEMENTATION_MAC_H_
