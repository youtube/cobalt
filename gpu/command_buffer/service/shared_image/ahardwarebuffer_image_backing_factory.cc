// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image/ahardwarebuffer_image_backing_factory.h"

#include <dawn/native/DawnNative.h>
#include <dawn/webgpu.h>
#include <sync/sync.h>
#include <unistd.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/android/android_hardware_buffer_compat.h"
#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "base/android/scoped_hardware_buffer_handle.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/ahardwarebuffer_utils.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image/android_image_backing.h"
#include "gpu/command_buffer/service/shared_image/dawn_ahardwarebuffer_image_representation.h"
#include "gpu/command_buffer/service/shared_image/gl_texture_android_image_representation.h"
#include "gpu/command_buffer/service/shared_image/gl_texture_passthrough_android_image_representation.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_format_utils.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image/skia_gl_image_representation.h"
#include "gpu/command_buffer/service/shared_image/skia_vk_android_image_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/vulkan/vulkan_image.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "ui/gfx/android/android_surface_control_compat.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence_android_native_fence_sync.h"
#include "ui/gl/gl_gl_api_implementation.h"
#include "ui/gl/gl_utils.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/scoped_binders.h"

namespace gpu {
namespace {

class OverlayImage final : public base::RefCounted<OverlayImage> {
 public:
  explicit OverlayImage(AHardwareBuffer* buffer)
      : handle_(base::android::ScopedHardwareBufferHandle::Create(buffer)) {}

  base::ScopedFD TakeEndFence() {
    previous_end_read_fence_ =
        base::ScopedFD(HANDLE_EINTR(dup(end_read_fence_.get())));
    return std::move(end_read_fence_);
  }

  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
  GetAHardwareBuffer() {
    return std::make_unique<ScopedHardwareBufferFenceSyncImpl>(
        this, base::android::ScopedHardwareBufferHandle::Create(handle_.get()),
        std::move(previous_end_read_fence_));
  }

 private:
  friend class base::RefCounted<OverlayImage>;

  class ScopedHardwareBufferFenceSyncImpl
      : public base::android::ScopedHardwareBufferFenceSync {
   public:
    ScopedHardwareBufferFenceSyncImpl(
        scoped_refptr<OverlayImage> image,
        base::android::ScopedHardwareBufferHandle handle,
        base::ScopedFD available_fence_fd)
        : ScopedHardwareBufferFenceSync(std::move(handle),
                                        base::ScopedFD(),
                                        std::move(available_fence_fd),
                                        /*is_video=*/false),
          image_(std::move(image)) {}
    ~ScopedHardwareBufferFenceSyncImpl() override = default;

    void SetReadFence(base::ScopedFD fence_fd) override {
      DCHECK(!image_->end_read_fence_.is_valid());
      DCHECK(!image_->previous_end_read_fence_.is_valid());

      image_->end_read_fence_ = std::move(fence_fd);
    }

   private:
    scoped_refptr<OverlayImage> image_;
  };

  ~OverlayImage() = default;

  base::android::ScopedHardwareBufferHandle handle_;

  // The fence for overlay controller to set to indicate scanning out
  // completion. The image content should not be modified before passing this
  // fence.
  base::ScopedFD end_read_fence_;

  // The fence for overlay controller from the last frame where this buffer was
  // presented.
  base::ScopedFD previous_end_read_fence_;
};

GLuint CreateAndBindTexture(EGLImage image, GLenum target) {
  gl::GLApi* api = gl::g_current_gl_context;
  GLuint service_id = 0;
  api->glGenTexturesFn(1, &service_id);
  gl::ScopedTextureBinder texture_binder(target, service_id);

  api->glTexParameteriFn(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  api->glTexParameteriFn(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  api->glTexParameteriFn(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  api->glTexParameteriFn(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glEGLImageTargetTexture2DOES(target, image);

  return service_id;
}

constexpr viz::SharedImageFormat kSupportedFormats[6]{
    viz::SinglePlaneFormat::kRGBA_8888, viz::SinglePlaneFormat::kRGB_565,
    viz::SinglePlaneFormat::kBGR_565,   viz::SinglePlaneFormat::kRGBA_F16,
    viz::SinglePlaneFormat::kRGBX_8888, viz::SinglePlaneFormat::kRGBA_1010102};

// Returns whether the format is supported by AHardwareBuffer.
// TODO(vikassoni): In future we will need to expose the set of formats and
// constraints (e.g. max size) to the clients somehow that are available for
// certain combinations of SharedImageUsage flags (e.g. when Vulkan is on,
// SHARED_IMAGE_USAGE_GLES2 + SHARED_IMAGE_USAGE_DISPLAY_READ implies AHB, so
// those restrictions apply, but that's decided on the service side). For now
// getting supported format is a static mechanism like this. We probably need
// something like gpu::Capabilities.texture_target_exception_list.
bool AHardwareBufferSupportedFormat(viz::SharedImageFormat format) {
  return base::Contains(kSupportedFormats, format);
}

// Returns the corresponding AHardwareBuffer format.
unsigned int AHardwareBufferFormat(viz::SharedImageFormat format) {
  DCHECK(AHardwareBufferSupportedFormat(format));

  if (format == viz::SinglePlaneFormat::kRGBA_8888) {
    return AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
  } else if (format == viz::SinglePlaneFormat::kRGB_565) {
    return AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM;
  } else if (format == viz::SinglePlaneFormat::kBGR_565) {
    return AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM;
  } else if (format == viz::SinglePlaneFormat::kRGBA_F16) {
    return AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT;
  } else if (format == viz::SinglePlaneFormat::kRGBX_8888) {
    return AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM;
  } else if (format == viz::SinglePlaneFormat::kRGBA_1010102) {
    return AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM;
  }

  NOTREACHED();
  return AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM;
}

constexpr uint32_t kSupportedUsage =
    SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT |
    SHARED_IMAGE_USAGE_DISPLAY_WRITE | SHARED_IMAGE_USAGE_DISPLAY_READ |
    SHARED_IMAGE_USAGE_RASTER | SHARED_IMAGE_USAGE_OOP_RASTERIZATION |
    SHARED_IMAGE_USAGE_SCANOUT | SHARED_IMAGE_USAGE_WEBGPU |
    SHARED_IMAGE_USAGE_VIDEO_DECODE |
    SHARED_IMAGE_USAGE_WEBGPU_SWAP_CHAIN_TEXTURE |
    SHARED_IMAGE_USAGE_HIGH_PERFORMANCE_GPU |
    SHARED_IMAGE_USAGE_WEBGPU_STORAGE_TEXTURE;

}  // namespace

// Implementation of SharedImageBacking that holds an AHardwareBuffer. This
// can be used to create a GL texture or a VK Image from the AHardwareBuffer
// backing.
class AHardwareBufferImageBacking : public AndroidImageBacking {
 public:
  AHardwareBufferImageBacking(
      const Mailbox& mailbox,
      viz::SharedImageFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      GrSurfaceOrigin surface_origin,
      SkAlphaType alpha_type,
      uint32_t usage,
      base::android::ScopedHardwareBufferHandle handle,
      size_t estimated_size,
      bool is_thread_safe,
      base::ScopedFD initial_upload_fd,
      scoped_refptr<base::RefCountedData<DawnProcTable>> dawn_procs,
      bool use_passthrough);

  AHardwareBufferImageBacking(const AHardwareBufferImageBacking&) = delete;
  AHardwareBufferImageBacking& operator=(const AHardwareBufferImageBacking&) =
      delete;

  ~AHardwareBufferImageBacking() override;

  // SharedImageBacking implementation.
  SharedImageBackingType GetType() const override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;
  gfx::Rect ClearedRect() const override;
  void SetClearedRect(const gfx::Rect& cleared_rect) override;
  base::android::ScopedHardwareBufferHandle GetAhbHandle() const;
  OverlayImage* BeginOverlayAccess(gfx::GpuFenceHandle&);
  void EndOverlayAccess();

 protected:
  std::unique_ptr<GLTextureImageRepresentation> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;

  std::unique_ptr<GLTexturePassthroughImageRepresentation>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) override;

  std::unique_ptr<SkiaGaneshImageRepresentation> ProduceSkiaGanesh(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;

  std::unique_ptr<OverlayImageRepresentation> ProduceOverlay(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) override;

  std::unique_ptr<DawnImageRepresentation> ProduceDawn(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      WGPUDevice device,
      WGPUBackendType backend_type,
      std::vector<WGPUTextureFormat> view_formats) override;

 private:
  const base::android::ScopedHardwareBufferHandle hardware_buffer_handle_;

  scoped_refptr<OverlayImage> overlay_image_ GUARDED_BY(lock_);
  scoped_refptr<base::RefCountedData<DawnProcTable>> dawn_procs_;
  const bool use_passthrough_;
};

// Vk backed Skia representation of AHardwareBufferImageBacking.
class SkiaVkAHBImageRepresentation : public SkiaVkAndroidImageRepresentation {
 public:
  SkiaVkAHBImageRepresentation(SharedImageManager* manager,
                               AndroidImageBacking* backing,
                               scoped_refptr<SharedContextState> context_state,
                               std::unique_ptr<VulkanImage> vulkan_image,
                               MemoryTypeTracker* tracker)
      : SkiaVkAndroidImageRepresentation(manager,
                                         backing,
                                         std::move(context_state),
                                         tracker) {
    DCHECK(vulkan_image);

    vulkan_image_ = std::move(vulkan_image);
    // TODO(bsalomon): Determine whether it makes sense to attempt to reuse this
    // if the vk_info stays the same on subsequent calls.
    promise_texture_ = SkPromiseImageTexture::Make(
        GrBackendTexture(size().width(), size().height(),
                         CreateGrVkImageInfo(vulkan_image_.get())));
    DCHECK(promise_texture_);
  }
};

class OverlayAHBImageRepresentation : public OverlayImageRepresentation {
 public:
  OverlayAHBImageRepresentation(SharedImageManager* manager,
                                SharedImageBacking* backing,
                                MemoryTypeTracker* tracker)
      : OverlayImageRepresentation(manager, backing, tracker) {}

  ~OverlayAHBImageRepresentation() override { EndReadAccess({}); }

 private:
  AHardwareBufferImageBacking* ahb_backing() {
    return static_cast<AHardwareBufferImageBacking*>(backing());
  }

  bool BeginReadAccess(gfx::GpuFenceHandle& acquire_fence) override {
    gfx::GpuFenceHandle fence_handle;
    gl_image_ = ahb_backing()->BeginOverlayAccess(fence_handle);

    if (!fence_handle.is_null())
      acquire_fence = std::move(fence_handle);

    return !!gl_image_;
  }

  void EndReadAccess(gfx::GpuFenceHandle release_fence) override {
    DCHECK(release_fence.is_null());
    if (gl_image_) {
      ahb_backing()->EndOverlayAccess();
      gl_image_ = nullptr;
    }
  }

  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
  GetAHardwareBufferFenceSync() override {
    return gl_image_->GetAHardwareBuffer();
  }

  raw_ptr<OverlayImage> gl_image_ = nullptr;
};

AHardwareBufferImageBacking::AHardwareBufferImageBacking(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    base::android::ScopedHardwareBufferHandle handle,
    size_t estimated_size,
    bool is_thread_safe,
    base::ScopedFD initial_upload_fd,
    scoped_refptr<base::RefCountedData<DawnProcTable>> dawn_procs,
    bool use_passthrough)
    : AndroidImageBacking(mailbox,
                          format,
                          size,
                          color_space,
                          surface_origin,
                          alpha_type,
                          usage,
                          estimated_size,
                          is_thread_safe,
                          std::move(initial_upload_fd)),
      hardware_buffer_handle_(std::move(handle)),
      dawn_procs_(std::move(dawn_procs)),
      use_passthrough_(use_passthrough) {
  DCHECK(hardware_buffer_handle_.is_valid());
}

AHardwareBufferImageBacking::~AHardwareBufferImageBacking() {
  // Locking here in destructor since we are accessing member variable
  // |have_context_| via have_context().
  AutoLock auto_lock(this);
  DCHECK(hardware_buffer_handle_.is_valid());
}

SharedImageBackingType AHardwareBufferImageBacking::GetType() const {
  return SharedImageBackingType::kAHardwareBuffer;
}

gfx::Rect AHardwareBufferImageBacking::ClearedRect() const {
  AutoLock auto_lock(this);
  return ClearedRectInternal();
}

void AHardwareBufferImageBacking::SetClearedRect(
    const gfx::Rect& cleared_rect) {
  AutoLock auto_lock(this);
  SetClearedRectInternal(cleared_rect);
}

void AHardwareBufferImageBacking::Update(
    std::unique_ptr<gfx::GpuFence> in_fence) {
  DCHECK(!in_fence);
}

base::android::ScopedHardwareBufferHandle
AHardwareBufferImageBacking::GetAhbHandle() const {
  return hardware_buffer_handle_.Clone();
}

std::unique_ptr<GLTextureImageRepresentation>
AHardwareBufferImageBacking::ProduceGLTexture(SharedImageManager* manager,
                                              MemoryTypeTracker* tracker) {
  // Use same texture for all the texture representations generated from same
  // backing.
  DCHECK(hardware_buffer_handle_.is_valid());

  auto egl_image =
      CreateEGLImageFromAHardwareBuffer(hardware_buffer_handle_.get());

  if (!egl_image.is_valid()) {
    return nullptr;
  }

  // Android documentation states that right GL format for RGBX AHardwareBuffer
  // is GL_RGB8, so we don't use angle rgbx.
  auto gl_format_desc = ToGLFormatDesc(format(), /*plane_index=*/0,
                                       /*use_angle_rgbx_format=*/false);
  GLuint service_id =
      CreateAndBindTexture(egl_image.get(), gl_format_desc.target);

  auto* texture =
      gles2::CreateGLES2TextureWithLightRef(service_id, gl_format_desc.target);
  texture->SetLevelInfo(gl_format_desc.target, 0,
                        gl_format_desc.image_internal_format, size().width(),
                        size().height(), 1, 0, gl_format_desc.data_format,
                        gl_format_desc.data_type, ClearedRect());
  texture->SetImmutable(true, false);

  return std::make_unique<GLTextureAndroidImageRepresentation>(
      manager, this, tracker, std::move(egl_image), std::move(texture));
}

std::unique_ptr<GLTexturePassthroughImageRepresentation>
AHardwareBufferImageBacking::ProduceGLTexturePassthrough(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  // Use same texture for all the texture representations generated from same
  // backing.
  DCHECK(hardware_buffer_handle_.is_valid());

  auto egl_image =
      CreateEGLImageFromAHardwareBuffer(hardware_buffer_handle_.get());
  if (!egl_image.is_valid()) {
    return nullptr;
  }

  // Android documentation states that right GL format for RGBX AHardwareBuffer
  // is GL_RGB8, so we don't use angle rgbx.
  auto gl_format_desc = ToGLFormatDesc(format(), /*plane_index=*/0,
                                       /*use_angle_rgbx_format=*/false);
  GLuint service_id =
      CreateAndBindTexture(egl_image.get(), gl_format_desc.target);

  auto texture = base::MakeRefCounted<gles2::TexturePassthrough>(
      service_id, gl_format_desc.target);
  texture->SetEstimatedSize(GetEstimatedSize());

  return std::make_unique<GLTexturePassthroughAndroidImageRepresentation>(
      manager, this, tracker, std::move(egl_image), std::move(texture));
}

std::unique_ptr<SkiaGaneshImageRepresentation>
AHardwareBufferImageBacking::ProduceSkiaGanesh(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  DCHECK(context_state);

  // Check whether we are in Vulkan mode OR GL mode and accordingly create
  // Skia representation.
  if (context_state->GrContextIsVulkan()) {
    uint32_t queue_family = VK_QUEUE_FAMILY_EXTERNAL;
    if (usage() & SHARED_IMAGE_USAGE_SCANOUT) {
      // Any Android API that consume or produce buffers (e.g SurfaceControl)
      // requires a foreign queue.
      queue_family = VK_QUEUE_FAMILY_FOREIGN_EXT;
    }
    auto vulkan_image = CreateVkImageFromAhbHandle(
        GetAhbHandle(), context_state.get(), size(), format(), queue_family);

    if (!vulkan_image)
      return nullptr;

    return std::make_unique<SkiaVkAHBImageRepresentation>(
        manager, this, std::move(context_state), std::move(vulkan_image),
        tracker);
  }
  DCHECK(context_state->GrContextIsGL());
  DCHECK(hardware_buffer_handle_.is_valid());

  std::unique_ptr<GLTextureImageRepresentationBase> gl_representation;
  if (use_passthrough_) {
    gl_representation = ProduceGLTexturePassthrough(manager, tracker);
  } else {
    gl_representation = ProduceGLTexture(manager, tracker);
  }

  if (!gl_representation) {
    LOG(ERROR) << "Unable produce gl texture!";
    return nullptr;
  }

  return SkiaGLImageRepresentation::Create(std::move(gl_representation),
                                           std::move(context_state), manager,
                                           this, tracker);
}

std::unique_ptr<OverlayImageRepresentation>
AHardwareBufferImageBacking::ProduceOverlay(SharedImageManager* manager,
                                            MemoryTypeTracker* tracker) {
  return std::make_unique<OverlayAHBImageRepresentation>(manager, this,
                                                         tracker);
}

std::unique_ptr<DawnImageRepresentation>
AHardwareBufferImageBacking::ProduceDawn(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    WGPUDevice device,
    WGPUBackendType backend_type,
    std::vector<WGPUTextureFormat> view_formats) {
#if BUILDFLAG(USE_DAWN)
  // Use same texture for all the texture representations generated from same
  // backing.
  DCHECK(hardware_buffer_handle_.is_valid());
  DCHECK(dawn_procs_);

  // Only Vulkan is supported on Android currently
  DCHECK_EQ(backend_type, WGPUBackendType_Vulkan);
  WGPUTextureFormat webgpu_format = ToWGPUFormat(format());
  if (webgpu_format == WGPUTextureFormat_Undefined) {
    LOG(ERROR) << "Unable to fine a suitable WebGPU format.";
    return nullptr;
  }
  return std::make_unique<DawnAHardwareBufferImageRepresentation>(
      manager, this, tracker, device, webgpu_format, std::move(view_formats),
      hardware_buffer_handle_.get(), dawn_procs_);
#else
  return nullptr;
#endif  // BUILDFLAG(USE_DAWN)
}

OverlayImage* AHardwareBufferImageBacking::BeginOverlayAccess(
    gfx::GpuFenceHandle& begin_read_fence) {
  AutoLock auto_lock(this);

  DCHECK(!is_overlay_accessing_);

  if (is_writing_) {
    LOG(ERROR)
        << "BeginOverlayAccess should only be called when there are no writers";
    return nullptr;
  }

  if (!overlay_image_) {
    overlay_image_ =
        base::MakeRefCounted<OverlayImage>(hardware_buffer_handle_.get());
  }

  if (write_sync_fd_.is_valid()) {
    gfx::GpuFenceHandle fence_handle;
    fence_handle.owned_fd =
        base::ScopedFD(HANDLE_EINTR(dup(write_sync_fd_.get())));
    begin_read_fence = std::move(fence_handle);
  }

  is_overlay_accessing_ = true;
  return overlay_image_.get();
}

void AHardwareBufferImageBacking::EndOverlayAccess() {
  AutoLock auto_lock(this);

  DCHECK(is_overlay_accessing_);
  is_overlay_accessing_ = false;

  auto fence_fd = overlay_image_->TakeEndFence();
  read_sync_fd_ = gl::MergeFDs(std::move(read_sync_fd_), std::move(fence_fd));
}

// static
AHardwareBufferImageBackingFactory::FormatInfo
AHardwareBufferImageBackingFactory::FormatInfoForSupportedFormat(
    viz::SharedImageFormat format,
    const gles2::Validators* validators) {
  CHECK(AHardwareBufferSupportedFormat(format));

  FormatInfo info;
  info.ahb_format = AHardwareBufferFormat(format);

  // TODO(vikassoni): In future when we use GL_TEXTURE_EXTERNAL_OES target
  // with AHB, we need to check if oes_egl_image_external is supported or
  // not.
  const bool is_egl_image_supported =
      gl::g_current_gl_driver->ext.b_GL_OES_EGL_image;
  if (!is_egl_image_supported) {
    return info;
  }

  // Check if AHB backed GL texture can be created using this format and
  // gather GL related format info.
  // TODO(vikassoni): Add vulkan related information in future.
  GLuint internal_format = GLInternalFormat(format);
  GLenum gl_format = GLDataFormat(format);
  GLenum gl_type = GLDataType(format);

  //  GLImageAHardwareBuffer supports internal format GL_RGBA and GL_RGB.
  if (internal_format != GL_RGBA && internal_format != GL_RGB) {
    return info;
  }

  // Validate if GL format, type and internal format is supported.
  if (validators->texture_internal_format.IsValid(internal_format) &&
      validators->texture_format.IsValid(gl_format) &&
      validators->pixel_type.IsValid(gl_type)) {
    info.gl_supported = true;
    info.gl_format = gl_format;
    info.gl_type = gl_type;
    info.internal_format = internal_format;
  }
  return info;
}

AHardwareBufferImageBackingFactory::AHardwareBufferImageBackingFactory(
    const gles2::FeatureInfo* feature_info,
    const GpuPreferences& gpu_preferences)
    : SharedImageBackingFactory(kSupportedUsage),
      use_passthrough_(gpu_preferences.use_passthrough_cmd_decoder &&
                       gl::PassthroughCommandDecoderSupported()) {
  DCHECK(base::AndroidHardwareBufferCompat::IsSupportAvailable());

  // Build the feature info for all the supported formats.
  for (auto format : kSupportedFormats) {
    format_infos_[format] =
        FormatInfoForSupportedFormat(format, feature_info->validators());
  }

  // TODO(vikassoni): We are using below GL api calls for now as Vulkan mode
  // doesn't exist. Once we have vulkan support, we shouldn't query GL in this
  // code until we are asked to make a GL representation (or allocate a
  // backing for import into GL)? We may use an AHardwareBuffer exclusively
  // with Vulkan, where there is no need to require that a GL context is
  // current. Maybe we can lazy init this if someone tries to create an
  // AHardwareBuffer with SHARED_IMAGE_USAGE_GLES2 ||
  // !gpu_preferences.enable_vulkan. When in Vulkan mode, we should only need
  // this with GLES2.
  gl::GLApi* api = gl::g_current_gl_context;
  api->glGetIntegervFn(GL_MAX_TEXTURE_SIZE, &max_gl_texture_size_);

  // Ensure max_texture_size_ is less than INT_MAX so that gfx::Rect and friends
  // can be used to accurately represent all valid sub-rects, with overflow
  // cases, clamped to INT_MAX, always invalid.
  max_gl_texture_size_ = std::min(max_gl_texture_size_, INT_MAX - 1);

#if BUILDFLAG(USE_DAWN)
  // If building with Dawn support enabled, ensure that we have access to the
  // Dawn procs.
  dawn_procs_ = base::MakeRefCounted<base::RefCountedData<DawnProcTable>>(
      dawn::native::GetProcs());
#endif  // BUILDFLAG(USE_DAWN)
}

AHardwareBufferImageBackingFactory::~AHardwareBufferImageBackingFactory() =
    default;

bool AHardwareBufferImageBackingFactory::ValidateUsage(
    uint32_t usage,
    const gfx::Size& size,
    viz::SharedImageFormat format) const {
  if (!AHardwareBufferSupportedFormat(format)) {
    LOG(ERROR) << "viz::SharedImageFormat " << format.ToString()
               << " not supported by AHardwareBuffer";
    return false;
  }

  const FormatInfo& format_info = GetFormatInfo(format);

  // SHARED_IMAGE_USAGE_RASTER is set when we want to write on Skia
  // representation and SHARED_IMAGE_USAGE_DISPLAY_READ is used for cases we
  // want to read from skia representation.
  // TODO(vikassoni): Also check gpu_preferences.enable_vulkan to figure out
  // if skia is using vulkan backing or GL backing.
  const bool use_gles2 =
      (usage &
       (SHARED_IMAGE_USAGE_GLES2 | SHARED_IMAGE_USAGE_RASTER |
        SHARED_IMAGE_USAGE_DISPLAY_READ | SHARED_IMAGE_USAGE_DISPLAY_WRITE));

  // If usage flags indicated this backing can be used as a GL texture, then
  // do below gl related checks.
  if (use_gles2) {
    // Check if the GL texture can be created from AHB with this format.
    if (!format_info.gl_supported) {
      LOG(ERROR)
          << "viz::SharedImageFormat " << format.ToString()
          << " can not be used to create a GL texture from AHardwareBuffer.";
      return false;
    }
  }

  // Check if AHB can be created with the current size restrictions.
  // TODO(vikassoni): Check for VK size restrictions for VK import, GL size
  // restrictions for GL import OR both if this backing is needed to be used
  // with both GL and VK.
  if (size.width() < 1 || size.height() < 1 ||
      size.width() > max_gl_texture_size_ ||
      size.height() > max_gl_texture_size_) {
    LOG(ERROR) << "CreateSharedImage: invalid size=" << size.ToString()
               << " max_gl_texture_size=" << max_gl_texture_size_;
    return false;
  }

  return true;
}

std::unique_ptr<SharedImageBacking>
AHardwareBufferImageBackingFactory::MakeBacking(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    bool is_thread_safe,
    base::span<const uint8_t> pixel_data) {
  DCHECK(base::AndroidHardwareBufferCompat::IsSupportAvailable());
  DCHECK(!format.IsCompressed());

  if (!ValidateUsage(usage, size, format)) {
    return nullptr;
  }

  // Calculate SharedImage size in bytes.
  auto estimated_size = format.MaybeEstimatedSizeInBytes(size);
  if (!estimated_size) {
    LOG(ERROR) << "Failed to calculate SharedImage size";
    return nullptr;
  }

  const FormatInfo& format_info = GetFormatInfo(format);

  // Setup AHardwareBuffer.
  AHardwareBuffer* buffer = nullptr;
  AHardwareBuffer_Desc hwb_desc;
  hwb_desc.width = size.width();
  hwb_desc.height = size.height();
  hwb_desc.format = format_info.ahb_format;

  // Set usage so that gpu can both read as a texture/write as a framebuffer
  // attachment. TODO(vikassoni): Find out if we need to set some more usage
  // flags based on the usage params in the current function call.
  hwb_desc.usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE |
                   AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT;
  if (usage & SHARED_IMAGE_USAGE_SCANOUT)
    hwb_desc.usage |= gfx::SurfaceControl::RequiredUsage();

  // Add WRITE usage as we'll it need to upload data
  if (!pixel_data.empty())
    hwb_desc.usage |= AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY;

  // Number of images in an image array.
  hwb_desc.layers = 1;

  // The following three are not used here.
  hwb_desc.stride = 0;
  hwb_desc.rfu0 = 0;
  hwb_desc.rfu1 = 0;

  // Allocate an AHardwareBuffer.
  base::AndroidHardwareBufferCompat::GetInstance().Allocate(&hwb_desc, &buffer);
  if (!buffer) {
    LOG(ERROR) << "Failed to allocate AHardwareBuffer";
    return nullptr;
  }

  auto handle = base::android::ScopedHardwareBufferHandle::Adopt(buffer);

  base::ScopedFD initial_upload_fd;
  // Upload data if necessary
  if (!pixel_data.empty()) {
    // Get description about buffer to obtain stride
    AHardwareBuffer_Desc hwb_info;
    base::AndroidHardwareBufferCompat::GetInstance().Describe(buffer,
                                                              &hwb_info);
    void* address = nullptr;
    if (int error = base::AndroidHardwareBufferCompat::GetInstance().Lock(
            buffer, AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY, -1, 0, &address)) {
      LOG(ERROR) << "Failed to lock AHardwareBuffer: " << error;
      return nullptr;
    }

    int bytes_per_pixel = BitsPerPixel(format) / 8;

    // NOTE: hwb_info.stride is in pixels
    int dst_stride = bytes_per_pixel * hwb_info.stride;
    int src_stride = bytes_per_pixel * size.width();

    for (int y = 0; y < size.height(); y++) {
      void* dst = reinterpret_cast<uint8_t*>(address) + dst_stride * y;
      const void* src = pixel_data.data() + src_stride * y;

      memcpy(dst, src, src_stride);
    }

    int32_t fence = -1;
    base::AndroidHardwareBufferCompat::GetInstance().Unlock(buffer, &fence);
    initial_upload_fd = base::ScopedFD(fence);
  }

  auto backing = std::make_unique<AHardwareBufferImageBacking>(
      mailbox, format, size, color_space, surface_origin, alpha_type, usage,
      std::move(handle), estimated_size.value(), is_thread_safe,
      std::move(initial_upload_fd), dawn_procs_, use_passthrough_);

  // If we uploaded initial data, set the backing as cleared.
  if (!pixel_data.empty())
    backing->SetCleared();

  return backing;
}

std::unique_ptr<SharedImageBacking>
AHardwareBufferImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    SurfaceHandle surface_handle,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    std::string debug_label,
    bool is_thread_safe) {
  return MakeBacking(mailbox, format, size, color_space, surface_origin,
                     alpha_type, usage, is_thread_safe, base::span<uint8_t>());
}

std::unique_ptr<SharedImageBacking>
AHardwareBufferImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    std::string debug_label,
    base::span<const uint8_t> pixel_data) {
  return MakeBacking(mailbox, format, size, color_space, surface_origin,
                     alpha_type, usage, false, pixel_data);
}

bool AHardwareBufferImageBackingFactory::CanImportGpuMemoryBuffer(
    gfx::GpuMemoryBufferType memory_buffer_type) {
  return memory_buffer_type == gfx::ANDROID_HARDWARE_BUFFER;
}

bool AHardwareBufferImageBackingFactory::IsSupported(
    uint32_t usage,
    viz::SharedImageFormat format,
    const gfx::Size& size,
    bool thread_safe,
    gfx::GpuMemoryBufferType gmb_type,
    GrContextType gr_context_type,
    base::span<const uint8_t> pixel_data) {
  if (format.is_multi_plane()) {
    return false;
  }

  if (gmb_type != gfx::EMPTY_BUFFER && !CanImportGpuMemoryBuffer(gmb_type)) {
    return false;
  }

  if (!AHardwareBufferSupportedFormat(format)) {
    return false;
  }

  return true;
}

AHardwareBufferImageBackingFactory::FormatInfo::FormatInfo() = default;

AHardwareBufferImageBackingFactory::FormatInfo::~FormatInfo() = default;

std::unique_ptr<SharedImageBacking>
AHardwareBufferImageBackingFactory::CreateSharedImage(
    const Mailbox& mailbox,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat buffer_format,
    gfx::BufferPlane plane,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    GrSurfaceOrigin surface_origin,
    SkAlphaType alpha_type,
    uint32_t usage,
    std::string debug_label) {
  // TODO(vasilyt): support SHARED_MEMORY_BUFFER?
  if (handle.type != gfx::ANDROID_HARDWARE_BUFFER) {
    NOTIMPLEMENTED();
    return nullptr;
  }
  if (plane != gfx::BufferPlane::DEFAULT) {
    LOG(ERROR) << "Invalid plane " << gfx::BufferPlaneToString(plane);
    return nullptr;
  }

  auto si_format = viz::SharedImageFormat::SinglePlane(
      viz::GetResourceFormat(buffer_format));
  if (!ValidateUsage(usage, size, si_format)) {
    return nullptr;
  }

  auto estimated_size = si_format.MaybeEstimatedSizeInBytes(size);
  if (!estimated_size) {
    LOG(ERROR) << "Failed to calculate SharedImage size";
    return nullptr;
  }

  auto backing = std::make_unique<AHardwareBufferImageBacking>(
      mailbox, si_format, size, color_space, surface_origin, alpha_type, usage,
      std::move(handle.android_hardware_buffer), estimated_size.value(), false,
      base::ScopedFD(), dawn_procs_, use_passthrough_);

  backing->SetCleared();
  return backing;
}

}  // namespace gpu
