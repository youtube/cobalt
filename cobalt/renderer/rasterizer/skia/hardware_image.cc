// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/configuration.h"
#if SB_API_VERSION >= 12 || SB_HAS(GLES2)

#include "cobalt/renderer/rasterizer/skia/hardware_image.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/renderer/backend/egl/framebuffer_render_target.h"
#include "cobalt/renderer/backend/egl/graphics_context.h"
#include "cobalt/renderer/backend/egl/texture.h"
#include "cobalt/renderer/rasterizer/skia/cobalt_skia_type_conversions.h"
#include "cobalt/renderer/rasterizer/skia/gl_format_conversions.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "third_party/skia/src/gpu/GrResourceProvider.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

HardwareImageData::HardwareImageData(
    std::unique_ptr<backend::TextureDataEGL> texture_data,
    render_tree::PixelFormat pixel_format,
    render_tree::AlphaFormat alpha_format)
    : texture_data_(std::move(texture_data)),
      descriptor_(texture_data_->GetSize(), pixel_format, alpha_format,
                  texture_data_->GetPitchInBytes()) {}

const render_tree::ImageDataDescriptor& HardwareImageData::GetDescriptor()
    const {
  return descriptor_;
}

uint8_t* HardwareImageData::GetMemory() { return texture_data_->GetMemory(); }

std::unique_ptr<backend::TextureDataEGL> HardwareImageData::PassTextureData() {
  return std::move(texture_data_);
}

HardwareRawImageMemory::HardwareRawImageMemory(
    std::unique_ptr<backend::RawTextureMemoryEGL> raw_texture_memory)
    : raw_texture_memory_(std::move(raw_texture_memory)) {}

size_t HardwareRawImageMemory::GetSizeInBytes() const {
  return raw_texture_memory_->GetSizeInBytes();
}

uint8_t* HardwareRawImageMemory::GetMemory() {
  return raw_texture_memory_->GetMemory();
}

std::unique_ptr<backend::RawTextureMemoryEGL>
HardwareRawImageMemory::PassRawTextureMemory() {
  return std::move(raw_texture_memory_);
}

// This will store the given pixel data in a GrTexture and the function
// GetBitmap(), overridden from SkiaImage, will return a reference to a SkBitmap
// object that refers to the image's GrTexture.  This object should only be
// initialized, destructed and used from the same rasterizer thread; it can be
// constructed from any thread though.
class HardwareFrontendImage::HardwareBackendImage {
 public:
  typedef base::Callback<void(HardwareBackendImage*)> InitializeFunction;

  explicit HardwareBackendImage(const InitializeFunction& initialize_function)
      : initialize_function_(initialize_function),
        initialized_task_executed_(false) {
    DETACH_FROM_THREAD(thread_checker_);
  }

  ~HardwareBackendImage() {
    TRACE_EVENT0("cobalt::renderer",
                 "HardwareBackendImage::~HardwareBackendImage()");
    // This object should always be destroyed from the thread that it was
    // constructed on.
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  }

  // Return true if the object was initialized due to this call, or false if
  // the image was already initialized.
  bool EnsureInitialized() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return ResetAndRunIfNotNull(&initialize_function_, this);
  }

  void InitializeTask() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    initialized_task_executed_ = true;
    EnsureInitialized();
  }

  bool TryDestroy() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    initialize_function_.Reset();
    return initialized_task_executed_;
  }

  // Initialize functions for the various ways to create a backend image.
  // One of these should be passed to the constructor to tell the object how
  // to create the image from the rasterizer thread. Ideally, these would be
  // non-static member functions, however, the problem is binding "this" to
  // the callback while passing it to the constructor. Options are:
  //   1. Use static functions which accept "this".
  //   2. Use a default constructor so that HardwareBackendImage can be
  //      constructed first, then a separate function to set the callback.
  //   3. Bind std::placeholders::_1 for "this". base::Callback does not accept
  //      placeholders, so the callback needs to be changed to std::function.
  //      However, std::function cannot take ownership of std::unique_ptr
  //      objects, so such parameters would have to be manually deleted.

  static void InitializeFromImageData(
      std::unique_ptr<HardwareImageData> image_data,
      backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context,
      HardwareBackendImage* backend) {
    TRACE_EVENT0("cobalt::renderer",
                 "HardwareBackendImage::InitializeFromImageData()");
    backend->texture_ =
        cobalt_context->CreateTexture(image_data->PassTextureData());
    backend->CommonInitialize(gr_context, cobalt_context);
  }

  static void InitializeFromRawImageData(
      const scoped_refptr<backend::ConstRawTextureMemoryEGL>& texture_memory,
      intptr_t offset, const render_tree::ImageDataDescriptor& descriptor,
      backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context,
      HardwareBackendImage* backend) {
    TRACE_EVENT0("cobalt::renderer",
                 "HardwareBackendImage::InitializeFromRawImageData()");
    backend->texture_ = cobalt_context->CreateTextureFromRawMemory(
        texture_memory, offset, descriptor.size,
        ConvertRenderTreeFormatToGL(descriptor.pixel_format),
        descriptor.pitch_in_bytes);
    backend->CommonInitialize(gr_context, cobalt_context);
  }

  static void InitializeFromTexture(
      std::unique_ptr<backend::TextureEGL> texture, GrContext* gr_context,
      HardwareBackendImage* backend) {
    TRACE_EVENT0("cobalt::renderer",
                 "HardwareBackendImage::InitializeFromTexture()");
    backend->texture_ = std::move(texture);
    backend::GraphicsContextEGL* cobalt_context =
        backend->texture_->graphics_context();
    backend->CommonInitialize(gr_context, cobalt_context);
  }

  static void InitializeFromRenderTree(
      const scoped_refptr<render_tree::Node>& root, const math::Size& size,
      const SubmitOffscreenCallback& submit_offscreen_callback,
      backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context,
      HardwareBackendImage* backend) {
    TRACE_EVENT0("cobalt::renderer",
                 "HardwareBackendImage::InitializeFromRenderTree()");
    backend::GraphicsContextEGL::ScopedMakeCurrent scoped_make_current(
        cobalt_context);

    scoped_refptr<backend::FramebufferRenderTargetEGL> render_target(
        new backend::FramebufferRenderTargetEGL(cobalt_context, size));
    CHECK(!render_target->CreationError());

    // The above call to FramebufferRenderTargetEGL() may have dirtied graphics
    // state, so tell Skia to reset its context.
    gr_context->resetContext(kRenderTarget_GrGLBackendState |
                             kTextureBinding_GrGLBackendState);
    submit_offscreen_callback.Run(root, render_target);

    std::unique_ptr<backend::TextureEGL> texture(
        new backend::TextureEGL(cobalt_context, render_target));

    InitializeFromTexture(std::move(texture), gr_context, backend);

    // Tell Skia that the graphics state is unknown because we issued custom
    // GL commands above.
    gr_context->resetContext(kRenderTarget_GrGLBackendState |
                             kTextureBinding_GrGLBackendState);
  }

  // Initiate all texture initialization code here, which should be executed
  // on the rasterizer thread.
  void CommonInitialize(GrContext* gr_context,
                        backend::GraphicsContextEGL* cobalt_context) {
    SB_DCHECK(gr_context);
    SB_DCHECK(cobalt_context);
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    TRACE_EVENT0("cobalt::renderer",
                 "HardwareBackendImage::CommonInitialize()");
    backend::GraphicsContextEGL::ScopedMakeCurrent scoped(cobalt_context);
    if (!texture_->IsValid()) {
      // The system likely did not have enough GPU memory for the texture.
      LOG(ERROR) << "Invalid texture passed to HardwareBackendImage";
      texture_.reset();
      return;
    }

    if (texture_->GetTarget() == GL_TEXTURE_2D) {
      GLenum internal_format =
          ConvertBaseGLFormatToSizedInternalFormat(texture_->GetFormat());
      GrGLTextureInfo texture_info = {texture_->GetTarget(),
                                      texture_->gl_handle(), internal_format};
      const math::Size& texture_size = texture_->GetSize();
      gr_texture_.reset(new GrBackendTexture(texture_size.width(),
                                             texture_size.height(),
                                             GrMipMapped::kNo, texture_info));

      DCHECK(gr_texture_);

      GrColorType gr_color_type =
          ConvertGLFormatToGrColorType(texture_->GetFormat());
      SkColorType sk_color_type = GrColorTypeToSkColorType(gr_color_type);
      image_ = SkImage::MakeFromTexture(gr_context, *gr_texture_,
                                        kTopLeft_GrSurfaceOrigin, sk_color_type,
                                        kPremul_SkAlphaType, nullptr);
    }
  }

  const sk_sp<SkImage>& GetImage() const {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return image_;
  }

  const backend::TextureEGL* GetTextureEGL() const {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return texture_.get();
  }

 private:
  // Keep a reference to the texture alive as long as this backend image
  // exists.
  std::unique_ptr<backend::TextureEGL> texture_;

  THREAD_CHECKER(thread_checker_);
  std::unique_ptr<GrBackendTexture> gr_texture_;
  sk_sp<SkImage> image_;

  InitializeFunction initialize_function_;
  bool initialized_task_executed_;
};

namespace {
// Given a ImageDataDescriptor, returns a AlternateRgbaFormat value for it,
// which for most formats will be base::nullopt, but for those that piggy-back
// on RGBA but assign different meanings to each of the 4 pixels, this will
// return a special formatting option.
base::Optional<AlternateRgbaFormat> AlternateRgbaFormatFromImageDataDescriptor(
    const render_tree::ImageDataDescriptor& descriptor) {
  if (descriptor.pixel_format == render_tree::kPixelFormatUYVY) {
    return AlternateRgbaFormat_UYVY;
  } else {
    return base::nullopt;
  }
}

// Depending on the alternate RGBA format, possibly adjust the content rect
// size.  For example, UYVY needs the content rect's width to be multiplied
// by two since each "pixel" actually represents two pixels side-by-side.  This
// allows render_tree::ImageNode objects that are constructed without an
// explicit size assigned to them to take on the size of the Y-width for UYVY,
// which is more natural.
math::Size AdjustSizeForFormat(
    const math::Size& size,
    const base::Optional<AlternateRgbaFormat>& alternate_rgba_format) {
  if (!alternate_rgba_format) {
    return size;
  }

  switch (*alternate_rgba_format) {
    case AlternateRgbaFormat_UYVY: {
      return math::Size(size.width() * 2, size.height());
    }
    default: {
      NOTREACHED();
      return size;
    }
  }
}
}  // namespace

HardwareFrontendImage::HardwareFrontendImage(
    std::unique_ptr<HardwareImageData> image_data,
    backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context,
    scoped_refptr<base::SingleThreadTaskRunner> rasterizer_task_runner)
    : is_opaque_(image_data->GetDescriptor().alpha_format ==
                 render_tree::kAlphaFormatOpaque),
      alternate_rgba_format_(AlternateRgbaFormatFromImageDataDescriptor(
          image_data->GetDescriptor())),
      size_(AdjustSizeForFormat(image_data->GetDescriptor().size,
                                alternate_rgba_format_)),
      rasterizer_task_runner_(rasterizer_task_runner) {
  backend_image_.reset(new HardwareBackendImage(
      base::Bind(&HardwareBackendImage::InitializeFromImageData,
                 base::Passed(&image_data), cobalt_context, gr_context)));
  InitializeBackend();
}

HardwareFrontendImage::HardwareFrontendImage(
    const scoped_refptr<backend::ConstRawTextureMemoryEGL>& raw_texture_memory,
    intptr_t offset, const render_tree::ImageDataDescriptor& descriptor,
    backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context,
    scoped_refptr<base::SingleThreadTaskRunner> rasterizer_task_runner)
    : is_opaque_(descriptor.alpha_format == render_tree::kAlphaFormatOpaque),
      alternate_rgba_format_(
          AlternateRgbaFormatFromImageDataDescriptor(descriptor)),
      size_(AdjustSizeForFormat(descriptor.size, alternate_rgba_format_)),
      rasterizer_task_runner_(rasterizer_task_runner) {
  TRACE_EVENT0("cobalt::renderer",
               "HardwareFrontendImage::HardwareFrontendImage()");
  backend_image_.reset(new HardwareBackendImage(base::Bind(
      &HardwareBackendImage::InitializeFromRawImageData, raw_texture_memory,
      offset, descriptor, cobalt_context, gr_context)));
  InitializeBackend();
}

HardwareFrontendImage::HardwareFrontendImage(
    std::unique_ptr<backend::TextureEGL> texture,
    render_tree::AlphaFormat alpha_format,
    backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context,
    std::unique_ptr<math::RectF> content_region,
    scoped_refptr<base::SingleThreadTaskRunner> rasterizer_task_runner,
    base::Optional<AlternateRgbaFormat> alternate_rgba_format)
    : is_opaque_(alpha_format == render_tree::kAlphaFormatOpaque),
      content_region_(std::move(content_region)),
      alternate_rgba_format_(alternate_rgba_format),
      size_(AdjustSizeForFormat(
          content_region_ ? math::Size(std::abs(content_region_->width()),
                                       std::abs(content_region_->height()))
                          : texture->GetSize(),
          alternate_rgba_format_)),
      rasterizer_task_runner_(rasterizer_task_runner) {
  TRACE_EVENT0("cobalt::renderer",
               "HardwareFrontendImage::HardwareFrontendImage()");
  backend_image_.reset(new HardwareBackendImage(
      base::Bind(&HardwareBackendImage::InitializeFromTexture,
                 base::Passed(&texture), gr_context)));
  InitializeBackend();
}

HardwareFrontendImage::HardwareFrontendImage(
    const scoped_refptr<render_tree::Node>& root,
    const SubmitOffscreenCallback& submit_offscreen_callback,
    backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context,
    scoped_refptr<base::SingleThreadTaskRunner> rasterizer_task_runner)
    : is_opaque_(false),
      size_(AdjustSizeForFormat(
          math::Size(static_cast<int>(root->GetBounds().right()),
                     static_cast<int>(root->GetBounds().bottom())),
          alternate_rgba_format_)),
      rasterizer_task_runner_(rasterizer_task_runner) {
  TRACE_EVENT0("cobalt::renderer",
               "HardwareFrontendImage::HardwareFrontendImage()");
  backend_image_.reset(new HardwareBackendImage(
      base::Bind(&HardwareBackendImage::InitializeFromRenderTree, root, size_,
                 submit_offscreen_callback, cobalt_context, gr_context)));
  InitializeBackend();
}

HardwareFrontendImage::~HardwareFrontendImage() {
  TRACE_EVENT0("cobalt::renderer",
               "HardwareFrontendImage::~HardwareFrontendImage()");
  // InitializeBackend() posted a task to call backend_image_'s
  // InitializeTask(). Make sure that task has finished before
  // destroying backend_image_.
  if (rasterizer_task_runner_) {
    if (!rasterizer_task_runner_->BelongsToCurrentThread() ||
        !backend_image_->TryDestroy()) {
      rasterizer_task_runner_->DeleteSoon(
          FROM_HERE, backend_image_.release());
    }
  }  // else let the scoped pointer clean it up immediately.
}

void HardwareFrontendImage::InitializeBackend() {
  // Initialize the image as soon as possible, rather than waiting for the
  // rasterizer to initialize it when needed. The image initialization process
  // may take some time, so doing a lazy initialize can cause a big spike in
  // frame time if multiple images are initialized in one frame.
  if (rasterizer_task_runner_) {
    rasterizer_task_runner_->PostTask(
        FROM_HERE, base::Bind(&HardwareBackendImage::InitializeTask,
                              base::Unretained(backend_image_.get())));
  }
}

const sk_sp<SkImage>& HardwareFrontendImage::GetImage() const {
  DCHECK(!rasterizer_task_runner_ ||
         rasterizer_task_runner_->BelongsToCurrentThread());
  // Forward this call to the backend image.  This method must be called from
  // the rasterizer thread (e.g. during a render tree visitation).  The backend
  // image will check that this is being called from the correct thread.
  return backend_image_->GetImage();
}

const backend::TextureEGL* HardwareFrontendImage::GetTextureEGL() const {
  DCHECK(!rasterizer_task_runner_ ||
         rasterizer_task_runner_->BelongsToCurrentThread());
  return backend_image_->GetTextureEGL();
}

bool HardwareFrontendImage::CanRenderInSkia() const {
  DCHECK(!rasterizer_task_runner_ ||
         rasterizer_task_runner_->BelongsToCurrentThread());
  // In some cases, especially when dealing with SbDecodeTargets, we may end
  // up with a GLES2 texture whose target is not GL_TEXTURE_2D, in which case
  // we cannot use our typical Skia flow to render it, and we delegate to
  // a rasterizer-provided callback for performing custom rendering (e.g.
  // via direct GL calls).
  // We also fallback if a content region is specified on the image, since we
  // don't support handling that in the normal flow.
  return !GetContentRegion() &&
         (!GetTextureEGL() || GetTextureEGL()->GetTarget() == GL_TEXTURE_2D) &&
         !alternate_rgba_format_;
}

bool HardwareFrontendImage::EnsureInitialized() {
  DCHECK(!rasterizer_task_runner_ ||
         rasterizer_task_runner_->BelongsToCurrentThread());
  return backend_image_->EnsureInitialized();
}

HardwareMultiPlaneImage::HardwareMultiPlaneImage(
    std::unique_ptr<HardwareRawImageMemory> raw_image_memory,
    const render_tree::MultiPlaneImageDataDescriptor& descriptor,
    backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context,
    scoped_refptr<base::SingleThreadTaskRunner> rasterizer_task_runner)
    : size_(descriptor.GetPlaneDescriptor(0).size),
      estimated_size_in_bytes_(raw_image_memory->GetSizeInBytes()),
      format_(descriptor.image_format()) {
  scoped_refptr<backend::ConstRawTextureMemoryEGL> const_raw_texture_memory(
      new backend::ConstRawTextureMemoryEGL(
          raw_image_memory->PassRawTextureMemory()));

  // Construct a single plane image for each plane of this multi plane image.
  for (int i = 0; i < descriptor.num_planes(); ++i) {
    planes_[i] = new HardwareFrontendImage(
        const_raw_texture_memory, descriptor.GetPlaneOffset(i),
        descriptor.GetPlaneDescriptor(i), cobalt_context, gr_context,
        rasterizer_task_runner);
  }
}

HardwareMultiPlaneImage::HardwareMultiPlaneImage(
    render_tree::MultiPlaneImageFormat format,
    const std::vector<scoped_refptr<HardwareFrontendImage> >& planes)
    : size_(planes[0]->GetSize()), format_(format) {
  DCHECK(planes.size() <=
         render_tree::MultiPlaneImageDataDescriptor::kMaxPlanes);
  estimated_size_in_bytes_ = 0;
  for (unsigned int i = 0; i < planes.size(); ++i) {
    planes_[i] = planes[i];
    estimated_size_in_bytes_ += planes_[i]->GetEstimatedSizeInBytes();
  }
}

HardwareMultiPlaneImage::~HardwareMultiPlaneImage() {}

bool HardwareMultiPlaneImage::EnsureInitialized() {
  // A multi-plane image is not considered backend-initialized until all its
  // single-plane images are backend-initialized, thus we ensure that all
  // the component images are backend-initialized.
  bool initialized = false;
  for (int i = 0; i < render_tree::MultiPlaneImageDataDescriptor::kMaxPlanes;
       ++i) {
    if (planes_[i]) {
      initialized |= planes_[i]->EnsureInitialized();
    }
  }
  return initialized;
}

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_API_VERSION >= 12 || SB_HAS(GLES2)
