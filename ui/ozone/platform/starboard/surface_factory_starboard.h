// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_STARBOARD_STARBOARD_SURFACE_FACTORY_H_
#define UI_OZONE_PLATFORM_STARBOARD_STARBOARD_SURFACE_FACTORY_H_

#include <memory>
#include <vector>

#include "ui/ozone/public/gl_ozone.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#include "ui/ozone/platform/starboard/gl_surface_starboard.h"
#include "ui/ozone/platform/starboard/gl_ozone_egl_starboard.h"

namespace ui {

class SurfaceFactoryStarboard : public SurfaceFactoryOzone {
 public:
  explicit SurfaceFactoryStarboard();
  SurfaceFactoryStarboard(const SurfaceFactoryStarboard&) = delete;
  SurfaceFactoryStarboard& operator=(const SurfaceFactoryStarboard&) = delete;
  ~SurfaceFactoryStarboard() override;

  // SurfaceFactoryOzone:
  std::vector<gl::GLImplementationParts> GetAllowedGLImplementations() override;

  GLOzone* GetGLOzone(const gl::GLImplementationParts& implementation) override;

  std::unique_ptr<SurfaceOzoneCanvas> CreateCanvasForWidget(
      gfx::AcceleratedWidget widget) override;

  scoped_refptr<gfx::NativePixmap> CreateNativePixmap(
      gfx::AcceleratedWidget widget,
      gpu::VulkanDeviceQueue* device_queue,
      gfx::Size size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      absl::optional<gfx::Size> framebuffer_size = absl::nullopt) override;

 private:
  std::unique_ptr<GLOzone> swiftshader_implementation_;
  std::unique_ptr<GLOzoneEglStarboard> egl_implementation_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_STARBOARD_STARBOARD_SURFACE_FACTORY_H_
