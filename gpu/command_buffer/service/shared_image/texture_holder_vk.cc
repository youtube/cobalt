// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/texture_holder_vk.h"

#include "base/check.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/vulkan/vulkan_image.h"

namespace gpu {

TextureHolderVk::TextureHolderVk(std::unique_ptr<VulkanImage> image)
    : vulkan_image(std::move(image)) {
  gfx::Size size = vulkan_image->size();
  GrVkImageInfo vk_image_info = CreateGrVkImageInfo(vulkan_image.get());
  backend_texture =
      GrBackendTexture(size.width(), size.height(), vk_image_info);
  promise_texture = SkPromiseImageTexture::Make(backend_texture);
}

TextureHolderVk::TextureHolderVk(TextureHolderVk&& other) = default;
TextureHolderVk& TextureHolderVk::operator=(TextureHolderVk&& other) = default;
TextureHolderVk::~TextureHolderVk() = default;

GrVkImageInfo TextureHolderVk::GetGrVkImageInfo() const {
  GrVkImageInfo info;
  bool result = backend_texture.getVkImageInfo(&info);
  CHECK(result);
  return info;
}

}  // namespace gpu
