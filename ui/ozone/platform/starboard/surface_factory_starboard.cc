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

class TestPixmap : public gfx::NativePixmap {
 public:
  explicit TestPixmap(gfx::BufferFormat format) : format_(format) {}

  TestPixmap(const TestPixmap&) = delete;
  TestPixmap& operator=(const TestPixmap&) = delete;

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
  ~TestPixmap() override {}

  gfx::BufferFormat format_;
};

SurfaceFactoryStarboard::SurfaceFactoryStarboard() {
  LOG(INFO) << "SurfaceFactoryStarboard::SurfaceFactoryStarboard";
  egl_implementation_ = std::make_unique<GLOzoneEglStarboard>();
}

SurfaceFactoryStarboard::~SurfaceFactoryStarboard() {
  LOG(INFO) << "SurfaceFactoryStarboard::~SurfaceFactoryStarboard";
}

std::vector<gl::GLImplementationParts> SurfaceFactoryStarboard::GetAllowedGLImplementations() {
  LOG(INFO) << "SurfaceFactoryStarboard::GetAllowedGLImplementations";
  return std::vector<gl::GLImplementationParts>{
      gl::GLImplementationParts(gl::kGLImplementationEGLGLES2),
  };
}

GLOzone* SurfaceFactoryStarboard::GetGLOzone(const gl::GLImplementationParts& implementation) {
 LOG(INFO) << "SurfaceFactoryStarboard::GetGLOzone: " << implementation.gl;

 switch (implementation.gl) {
    case gl::kGLImplementationEGLGLES2:
    case gl::kGLImplementationEGLANGLE:
      LOG(INFO) << "Real impl";
      return egl_implementation_.get();
    default:
      LOG(INFO) << "No impl";
      return nullptr;
  } 

}

std::unique_ptr<SurfaceOzoneCanvas> SurfaceFactoryStarboard::CreateCanvasForWidget(
      gfx::AcceleratedWidget widget) {
  LOG(INFO) << "SurfaceFactoryStarboard::CreateCanvasForWidget";
  return nullptr;
}

scoped_refptr<gfx::NativePixmap> SurfaceFactoryStarboard::CreateNativePixmap(
      gfx::AcceleratedWidget widget,
      gpu::VulkanDeviceQueue* device_queue,
      gfx::Size size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      absl::optional<gfx::Size> framebuffer_size) {
  //LOG(INFO) << "SurfaceFactoryStarboard::CreateNativePixmap";
  return new TestPixmap(format);
}

} // namesapce ui
