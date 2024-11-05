// Copyright 2024 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "ui/ozone/platform/starboard/surface_factory_starboard.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/ozone/common/stub_client_native_pixmap_factory.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

namespace ui {
class StarboardPixmap : public gfx::NativePixmap {
 public:
  explicit StarboardPixmap(gfx::BufferFormat format) : format_(format) {}

  StarboardPixmap(const StarboardPixmap&) = delete;
  StarboardPixmap& operator=(const StarboardPixmap&) = delete;

  bool AreDmaBufFdsValid() const override { return false; }
  int GetDmaBufFd(size_t plane) const override { return -1; }
  uint32_t GetDmaBufPitch(size_t plane) const override { return 0; }
  size_t GetDmaBufOffset(size_t plane) const override { return 0; }
  size_t GetDmaBufPlaneSize(size_t plane) const override { return 0; }
  uint64_t GetBufferFormatModifier() const override { return 0; }
  gfx::BufferFormat GetBufferFormat() const override { return format_; }
  size_t GetNumberOfPlanes() const override {
    return gfx::NumberOfPlanesForLinearBufferFormat(format_);
  }
  bool SupportsZeroCopyWebGPUImport() const override { return false; }
  gfx::Size GetBufferSize() const override { return gfx::Size(); }
  uint32_t GetUniqueId() const override { return 0; }
  bool ScheduleOverlayPlane(
      gfx::AcceleratedWidget widget,
      const gfx::OverlayPlaneData& overlay_plane_data,
      std::vector<gfx::GpuFence> acquire_fences,
      std::vector<gfx::GpuFence> release_fences) override {
    return true;
  }
  gfx::NativePixmapHandle ExportHandle() override {
    return gfx::NativePixmapHandle();
  }

 private:
  ~StarboardPixmap() override {}

  gfx::BufferFormat format_;
};

SurfaceFactoryStarboard::SurfaceFactoryStarboard() {
  egl_implementation_ = std::make_unique<GLOzoneEglStarboard>();
}

SurfaceFactoryStarboard::~SurfaceFactoryStarboard() {}

std::vector<gl::GLImplementationParts> SurfaceFactoryStarboard::GetAllowedGLImplementations() {
  return std::vector<gl::GLImplementationParts>{
      gl::GLImplementationParts(gl::kGLImplementationEGLGLES2),
  };
}

GLOzone* SurfaceFactoryStarboard::GetGLOzone(const gl::GLImplementationParts& implementation) {
 switch (implementation.gl) {
    case gl::kGLImplementationEGLGLES2:
    case gl::kGLImplementationEGLANGLE:
      return egl_implementation_.get();
    default:
      return nullptr;
  }
}

std::unique_ptr<SurfaceOzoneCanvas> SurfaceFactoryStarboard::CreateCanvasForWidget(
      gfx::AcceleratedWidget widget) {
  return nullptr;
}

scoped_refptr<gfx::NativePixmap> SurfaceFactoryStarboard::CreateNativePixmap(
      gfx::AcceleratedWidget widget,
      gpu::VulkanDeviceQueue* device_queue,
      gfx::Size size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      absl::optional<gfx::Size> framebuffer_size) {
  return new StarboardPixmap(format);
}

} // namesapce ui
