// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/gbm_surface_factory.h"

#include <gbm.h>
#include <xf86drm.h>

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "third_party/khronos/EGL/egl.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/extension_set.h"
#include "ui/gfx/linux/drm_util_linux.h"
#include "ui/gfx/linux/gbm_defines.h"
#include "ui/gfx/linux/scoped_gbm_device.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_surface_egl.h"
#include "ui/gl/presenter.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/common/gl_ozone_egl.h"
#include "ui/ozone/common/native_pixmap_egl_binding.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/common/scoped_drm_types.h"
#include "ui/ozone/platform/drm/gpu/drm_gpu_util.h"
#include "ui/ozone/platform/drm/gpu/drm_thread_proxy.h"
#include "ui/ozone/platform/drm/gpu/drm_window_proxy.h"
#include "ui/ozone/platform/drm/gpu/gbm_overlay_surface.h"
#include "ui/ozone/platform/drm/gpu/gbm_pixmap.h"
#include "ui/ozone/platform/drm/gpu/gbm_surfaceless.h"
#include "ui/ozone/platform/drm/gpu/proxy_helpers.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ui/gfx/linux/gbm_util.h"  // nogncheck
#endif

#if BUILDFLAG(ENABLE_VULKAN)
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "ui/ozone/platform/drm/gpu/vulkan_implementation_gbm.h"
#define VK_STRUCTURE_TYPE_DMA_BUF_IMAGE_CREATE_INFO_INTEL 1024
typedef struct VkDmaBufImageCreateInfo_ {
  VkStructureType sType;
  raw_ptr<const void, ExperimentalAsh> pNext;
  int fd;
  VkFormat format;
  VkExtent3D extent;
  uint32_t strideInBytes;
} VkDmaBufImageCreateInfo;

typedef VkResult(VKAPI_PTR* PFN_vkCreateDmaBufImageINTEL)(
    VkDevice device,
    const VkDmaBufImageCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDeviceMemory* pMem,
    VkImage* pImage);
#endif

namespace ui {

class ScopedAllowBlockingForGbmSurface : public base::ScopedAllowBlocking {};

namespace {

EGLDeviceEXT GetPreferredEGLDevice() {
  std::vector<EGLDeviceEXT> devices(DRM_MAX_MINOR, EGL_NO_DEVICE_EXT);
  EGLint num_devices = 0;

  eglQueryDevicesEXT(DRM_MAX_MINOR, devices.data(), &num_devices);
  devices.resize(num_devices);

  std::map<EGLDeviceEXT, std::string> device_drivers;
  for (EGLDeviceEXT device : devices) {
    const char* filename =
        eglQueryDeviceStringEXT(device, EGL_DRM_DEVICE_FILE_EXT);
    if (!filename)  // Not a DRM device.
      continue;

    const auto driver_name = GetDrmDriverNameFromPath(filename);
    if (driver_name)
      device_drivers.insert({device, driver_name.value()});
  }

  // Find the device with the most preferred driver.
  const auto preferred_drivers = GetPreferredDrmDrivers();
  for (const auto* preferred_driver : preferred_drivers) {
    for (EGLDeviceEXT device : devices) {
      const auto driver = device_drivers.find(device);
      if (driver != device_drivers.end() &&
          driver->second == preferred_driver) {
        return device;
      }
    }
  }

  // Fall back to the first device.
  if (!devices.empty())
    return devices[0];

  return EGL_NO_DEVICE_EXT;
}

class GLOzoneEGLGbm : public GLOzoneEGL {
 public:
  GLOzoneEGLGbm(GbmSurfaceFactory* surface_factory,
                DrmThreadProxy* drm_thread_proxy)
      : surface_factory_(surface_factory),
        drm_thread_proxy_(drm_thread_proxy) {}

  GLOzoneEGLGbm(const GLOzoneEGLGbm&) = delete;
  GLOzoneEGLGbm& operator=(const GLOzoneEGLGbm&) = delete;

  ~GLOzoneEGLGbm() override = default;

  bool CanImportNativePixmap() override {
    return gl::GLSurfaceEGL::GetGLDisplayEGL()
        ->ext->b_EGL_EXT_image_dma_buf_import;
  }

  std::unique_ptr<NativePixmapGLBinding> ImportNativePixmap(
      scoped_refptr<gfx::NativePixmap> pixmap,
      gfx::BufferFormat plane_format,
      gfx::BufferPlane plane,
      gfx::Size plane_size,
      const gfx::ColorSpace& color_space,
      GLenum target,
      GLuint texture_id) override {
    return NativePixmapEGLBinding::Create(pixmap, plane_format, plane,
                                          plane_size, color_space, target,
                                          texture_id);
  }

  scoped_refptr<gl::GLSurface> CreateViewGLSurface(
      gl::GLDisplay* display,
      gfx::AcceleratedWidget window) override {
    return nullptr;
  }

  scoped_refptr<gl::Presenter> CreateSurfacelessViewGLSurface(
      gl::GLDisplay* display,
      gfx::AcceleratedWidget window) override {
    return base::MakeRefCounted<GbmSurfaceless>(
        surface_factory_, display->GetAs<gl::GLDisplayEGL>(),
        drm_thread_proxy_->CreateDrmWindowProxy(window), window);
  }

  scoped_refptr<gl::GLSurface> CreateOffscreenGLSurface(
      gl::GLDisplay* display,
      const gfx::Size& size) override {
    DCHECK_EQ(size.width(), 0);
    DCHECK_EQ(size.height(), 0);
    return gl::InitializeGLSurface(
        new gl::SurfacelessEGL(display->GetAs<gl::GLDisplayEGL>(), size));
  }

 protected:
  gl::EGLDisplayPlatform GetNativeDisplay() override {
    if (native_display_.Valid())
      return native_display_;

    // Default to null platform
    native_display_ = gl::EGLDisplayPlatform(EGL_DEFAULT_DISPLAY);

    if (gl::g_driver_egl.client_ext.b_EGL_MESA_platform_surfaceless) {
      native_display_ = gl::EGLDisplayPlatform(EGL_DEFAULT_DISPLAY,
                                               EGL_PLATFORM_SURFACELESS_MESA);
    }

    if (!(gl::g_driver_egl.client_ext.b_EGL_EXT_device_query &&
          gl::g_driver_egl.client_ext.b_EGL_EXT_platform_device &&
          gl::g_driver_egl.client_ext.b_EGL_EXT_device_enumeration)) {
      LOG(WARNING) << "Platform device extensions not found.";
      return native_display_;
    }

    const EGLDeviceEXT preferred_device = GetPreferredEGLDevice();
    if (preferred_device != EGL_NO_DEVICE_EXT) {
      native_display_ = gl::EGLDisplayPlatform(
          reinterpret_cast<EGLNativeDisplayType>(preferred_device),
          EGL_PLATFORM_DEVICE_EXT);
    }

    return native_display_;
  }

  bool LoadGLES2Bindings(const gl::GLImplementationParts& impl) override {
    return LoadDefaultEGLGLES2Bindings(impl);
  }

 private:
  raw_ptr<GbmSurfaceFactory, ExperimentalAsh> surface_factory_;
  raw_ptr<DrmThreadProxy, ExperimentalAsh> drm_thread_proxy_;
  gl::EGLDisplayPlatform native_display_;
};

std::vector<gfx::BufferFormat> EnumerateSupportedBufferFormatsForTexturing() {
#if BUILDFLAG(IS_CHROMEOS)
  CHECK(ui::IntelMediaCompressionEnvVarIsSet());
#endif

  std::vector<gfx::BufferFormat> supported_buffer_formats;
  // We cannot use FileEnumerator here because the sandbox is already closed.
  constexpr char kRenderNodeFilePattern[] = "/dev/dri/renderD%d";
  for (int i = 128; /* end on first card# that does not exist */; i++) {
    base::FilePath dev_path(FILE_PATH_LITERAL(
        base::StringPrintf(kRenderNodeFilePattern, i).c_str()));

    ScopedAllowBlockingForGbmSurface scoped_allow_blocking;
    base::File dev_path_file(dev_path,
                             base::File::FLAG_OPEN | base::File::FLAG_READ);
    if (!dev_path_file.IsValid())
      break;

    // Skip the virtual graphics memory manager device.
    ScopedDrmVersionPtr version(drmGetVersion(dev_path_file.GetPlatformFile()));
    if (!version || base::EqualsCaseInsensitiveASCII(version->name, "vgem")) {
      continue;
    }

    ScopedGbmDevice device(gbm_create_device(dev_path_file.GetPlatformFile()));
    if (!device) {
      LOG(ERROR) << "Couldn't create Gbm Device at " << dev_path.MaybeAsASCII();
      continue;
    }
    VLOG(1) << "Found Gbm Device at " << dev_path.MaybeAsASCII();

    for (int j = 0; j <= static_cast<int>(gfx::BufferFormat::LAST); ++j) {
      const gfx::BufferFormat buffer_format = static_cast<gfx::BufferFormat>(j);
      if (base::Contains(supported_buffer_formats, buffer_format))
        continue;
      if (gbm_device_is_format_supported(
              device.get(), GetFourCCFormatFromBufferFormat(buffer_format),
              GBM_BO_USE_TEXTURING)) {
        supported_buffer_formats.push_back(buffer_format);
      }
    }
  }
  return supported_buffer_formats;
}

void OnNativePixmapCreated(GbmSurfaceFactory::NativePixmapCallback callback,
                           base::WeakPtr<GbmSurfaceFactory> weak_ptr,
                           std::unique_ptr<GbmBuffer> buffer,
                           scoped_refptr<DrmFramebuffer> framebuffer) {
  if (!weak_ptr || !buffer) {
    std::move(callback).Run(nullptr);
  } else {
    std::move(callback).Run(base::MakeRefCounted<GbmPixmap>(
        weak_ptr.get(), std::move(buffer), std::move(framebuffer)));
  }
}

}  // namespace

GbmSurfaceFactory::GbmSurfaceFactory(DrmThreadProxy* drm_thread_proxy)
    : egl_implementation_(
          std::make_unique<GLOzoneEGLGbm>(this, drm_thread_proxy)),
      drm_thread_proxy_(drm_thread_proxy) {}

GbmSurfaceFactory::~GbmSurfaceFactory() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void GbmSurfaceFactory::RegisterSurface(gfx::AcceleratedWidget widget,
                                        GbmSurfaceless* surface) {
  DCHECK(thread_checker_.CalledOnValidThread());
  widget_to_surface_map_.emplace(widget, surface);
}

void GbmSurfaceFactory::UnregisterSurface(gfx::AcceleratedWidget widget) {
  DCHECK(thread_checker_.CalledOnValidThread());
  widget_to_surface_map_.erase(widget);
}

GbmSurfaceless* GbmSurfaceFactory::GetSurface(
    gfx::AcceleratedWidget widget) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto it = widget_to_surface_map_.find(widget);
  DCHECK(it != widget_to_surface_map_.end());
  return it->second;
}

std::vector<gl::GLImplementationParts>
GbmSurfaceFactory::GetAllowedGLImplementations() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return std::vector<gl::GLImplementationParts>{
      gl::GLImplementationParts(gl::kGLImplementationEGLGLES2),
      gl::GLImplementationParts(gl::kGLImplementationEGLANGLE),
      gl::GLImplementationParts(gl::ANGLEImplementation::kSwiftShader)};
}

GLOzone* GbmSurfaceFactory::GetGLOzone(
    const gl::GLImplementationParts& implementation) {
  switch (implementation.gl) {
    case gl::kGLImplementationEGLGLES2:
    case gl::kGLImplementationEGLANGLE:
      return egl_implementation_.get();
    default:
      return nullptr;
  }
}

#if BUILDFLAG(ENABLE_VULKAN)
std::unique_ptr<gpu::VulkanImplementation>
GbmSurfaceFactory::CreateVulkanImplementation(bool use_swiftshader,
                                              bool allow_protected_memory) {
  DCHECK(!use_swiftshader)
      << "Vulkan Swiftshader is not supported on this platform.";
  return std::make_unique<VulkanImplementationGbm>();
}

scoped_refptr<gfx::NativePixmap> GbmSurfaceFactory::CreateNativePixmapForVulkan(
    gfx::AcceleratedWidget widget,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    VkDevice vk_device,
    VkDeviceMemory* vk_device_memory,
    VkImage* vk_image) {
  std::unique_ptr<GbmBuffer> buffer;
  scoped_refptr<DrmFramebuffer> framebuffer;

  drm_thread_proxy_->CreateBuffer(widget, size, /*framebuffer_size=*/size,
                                  format, usage, GbmPixmap::kFlagNoModifiers,
                                  &buffer, &framebuffer);
  if (!buffer)
    return nullptr;

  PFN_vkCreateDmaBufImageINTEL create_dma_buf_image_intel =
      reinterpret_cast<PFN_vkCreateDmaBufImageINTEL>(
          vkGetDeviceProcAddr(vk_device, "vkCreateDmaBufImageINTEL"));
  if (!create_dma_buf_image_intel) {
    LOG(ERROR) << "Scanout buffers can only be imported into vulkan when "
                  "vkCreateDmaBufImageINTEL is available.";
    return nullptr;
  }

  DCHECK(buffer->AreFdsValid());
  DCHECK_EQ(buffer->GetNumPlanes(), 1U);

  base::ScopedFD vk_image_fd(dup(buffer->GetPlaneFd(0)));
  DCHECK(vk_image_fd.is_valid());

  // TODO(spang): Fix this for formats other than gfx::BufferFormat::BGRA_8888
  DCHECK_EQ(format, display::DisplaySnapshot::PrimaryFormat());
  VkFormat vk_format = VK_FORMAT_B8G8R8A8_SRGB;

  VkDmaBufImageCreateInfo dma_buf_image_create_info = {
      /* .sType = */ static_cast<VkStructureType>(
          VK_STRUCTURE_TYPE_DMA_BUF_IMAGE_CREATE_INFO_INTEL),
      /* .pNext = */ nullptr,
      /* .fd = */ vk_image_fd.release(),
      /* .format = */ vk_format,
      /* .extent = */
      {
          /* .width = */ static_cast<uint32_t>(size.width()),
          /* .height = */ static_cast<uint32_t>(size.height()),
          /* .depth = */ 1,
      },
      /* .strideInBytes = */ buffer->GetPlaneStride(0),
  };

  VkResult result =
      create_dma_buf_image_intel(vk_device, &dma_buf_image_create_info, nullptr,
                                 vk_device_memory, vk_image);
  if (result != VK_SUCCESS) {
    LOG(ERROR) << "Failed to create a Vulkan image from a dmabuf.";
    return nullptr;
  }

  return base::MakeRefCounted<GbmPixmap>(this, std::move(buffer),
                                         std::move(framebuffer));
}
#endif

std::unique_ptr<OverlaySurface> GbmSurfaceFactory::CreateOverlaySurface(
    gfx::AcceleratedWidget window) {
  return std::make_unique<GbmOverlaySurface>(
      drm_thread_proxy_->CreateDrmWindowProxy(window));
}

std::unique_ptr<SurfaceOzoneCanvas> GbmSurfaceFactory::CreateCanvasForWidget(
    gfx::AcceleratedWidget widget) {
  DCHECK(thread_checker_.CalledOnValidThread());
  LOG(ERROR) << "Software rendering mode is not supported with GBM platform";
  return nullptr;
}

scoped_refptr<gfx::NativePixmap> GbmSurfaceFactory::CreateNativePixmap(
    gfx::AcceleratedWidget widget,
    gpu::VulkanDeviceQueue* device_queue,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    absl::optional<gfx::Size> framebuffer_size) {
  if (framebuffer_size &&
      !gfx::Rect(size).Contains(gfx::Rect(*framebuffer_size))) {
    return nullptr;
  }
  std::unique_ptr<GbmBuffer> buffer;
  scoped_refptr<DrmFramebuffer> framebuffer;
  drm_thread_proxy_->CreateBuffer(
      widget, size, framebuffer_size ? *framebuffer_size : size, format, usage,
      0 /* flags */, &buffer, &framebuffer);
  if (!buffer)
    return nullptr;
  return base::MakeRefCounted<GbmPixmap>(this, std::move(buffer),
                                         std::move(framebuffer));
}

void GbmSurfaceFactory::CreateNativePixmapAsync(
    gfx::AcceleratedWidget widget,
    gpu::VulkanDeviceQueue* device_queue,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    NativePixmapCallback callback) {
  drm_thread_proxy_->CreateBufferAsync(
      widget, size, format, usage, 0 /* flags */,
      base::BindOnce(OnNativePixmapCreated, std::move(callback),
                     weak_factory_.GetWeakPtr()));
}

scoped_refptr<gfx::NativePixmap>
GbmSurfaceFactory::CreateNativePixmapFromHandleInternal(
    gfx::AcceleratedWidget widget,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::NativePixmapHandle handle) {
  if (handle.planes.size() > GBM_MAX_PLANES) {
    return nullptr;
  }

  std::unique_ptr<GbmBuffer> buffer;
  scoped_refptr<DrmFramebuffer> framebuffer;
  drm_thread_proxy_->CreateBufferFromHandle(
      widget, size, format, std::move(handle), &buffer, &framebuffer);
  if (!buffer)
    return nullptr;
  return base::MakeRefCounted<GbmPixmap>(this, std::move(buffer),
                                         std::move(framebuffer));
}

scoped_refptr<gfx::NativePixmap>
GbmSurfaceFactory::CreateNativePixmapFromHandle(
    gfx::AcceleratedWidget widget,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::NativePixmapHandle handle) {
  // Query the external service (if available), whether it recognizes this
  // NativePixmapHandle, and whether it can provide a corresponding NativePixmap
  // backing it. If so, the handle is consumed. Otherwise, the handle remains
  // valid and can be further importer by standard means.
  if (!get_protected_native_pixmap_callback_.is_null()) {
    auto protected_pixmap = get_protected_native_pixmap_callback_.Run(handle);
    if (protected_pixmap)
      return protected_pixmap;
  }

  return CreateNativePixmapFromHandleInternal(widget, size, format,
                                              std::move(handle));
}

scoped_refptr<gfx::NativePixmap>
GbmSurfaceFactory::CreateNativePixmapForProtectedBufferHandle(
    gfx::AcceleratedWidget widget,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::NativePixmapHandle handle) {
  // Create a new NativePixmap without querying the external service for any
  // existing mappings.
  return CreateNativePixmapFromHandleInternal(widget, size, format,
                                              std::move(handle));
}

bool GbmSurfaceFactory::SupportsDrmModifiersFilter() const {
  return true;
}

void GbmSurfaceFactory::SetDrmModifiersFilter(
    std::unique_ptr<DrmModifiersFilter> filter) {
  drm_thread_proxy_->SetDrmModifiersFilter(std::move(filter));
}

void GbmSurfaceFactory::SetGetProtectedNativePixmapDelegate(
    const GetProtectedNativePixmapCallback&
        get_protected_native_pixmap_callback) {
  get_protected_native_pixmap_callback_ = get_protected_native_pixmap_callback;
}

std::vector<gfx::BufferFormat>
GbmSurfaceFactory::GetSupportedFormatsForTexturing() const {
  return EnumerateSupportedBufferFormatsForTexturing();
}

}  // namespace ui
