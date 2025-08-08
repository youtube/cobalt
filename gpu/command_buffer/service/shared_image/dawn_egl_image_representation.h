// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_EGL_IMAGE_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_EGL_IMAGE_REPRESENTATION_H_

#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"

typedef void* EGLImage;

namespace gpu {

class GPU_GLES2_EXPORT DawnEGLImageRepresentation
    : public DawnImageRepresentation {
 public:
  DawnEGLImageRepresentation(
      std::unique_ptr<GLTextureImageRepresentationBase> gl_representation,
      void* egl_image,
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker,
      const wgpu::Device& device);
  ~DawnEGLImageRepresentation() override;

 private:
  wgpu::Texture BeginAccess(wgpu::TextureUsage usage) override;
  void EndAccess() override;

 private:
  std::unique_ptr<GLTextureImageRepresentationBase> gl_representation_;
  raw_ptr<void> egl_image_ = nullptr;  // EGLImageKHR
  const wgpu::Device device_;
  wgpu::Texture texture_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_DAWN_EGL_IMAGE_REPRESENTATION_H_
