// Copyright 2014 Google Inc. All Rights Reserved.
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

#include "cobalt/renderer/rasterizer/skia/hardware_image.h"

#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/debug/trace_event.h"
#include "cobalt/renderer/backend/egl/framebuffer_render_target.h"
#include "cobalt/renderer/backend/egl/texture.h"
#include "cobalt/renderer/rasterizer/skia/cobalt_skia_type_conversions.h"
#include "cobalt/renderer/rasterizer/skia/gl_format_conversions.h"
#include "third_party/skia/include/gpu/SkGrPixelRef.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

GrTexture* WrapCobaltTextureWithSkiaTexture(
    GrContext* gr_context, backend::TextureEGL* cobalt_texture) {
  // Setup a Skia texture descriptor to describe the texture we wish to have
  // wrapped within a Skia GrTexture.
  GrBackendTextureDesc desc;
  desc.fFlags = kNone_GrBackendTextureFlag;
  desc.fOrigin = kTopLeft_GrSurfaceOrigin;
  desc.fWidth = cobalt_texture->GetSize().width();
  desc.fHeight = cobalt_texture->GetSize().height();
  desc.fConfig = ConvertGLFormatToGr(cobalt_texture->GetFormat());
  desc.fSampleCnt = 0;

  desc.fTextureHandle =
      static_cast<GrBackendObject>(cobalt_texture->GetPlatformHandle());

  return gr_context->wrapBackendTexture(desc);
}

HardwareImageData::HardwareImageData(
    scoped_ptr<backend::TextureDataEGL> texture_data,
    render_tree::PixelFormat pixel_format,
    render_tree::AlphaFormat alpha_format)
    : texture_data_(texture_data.Pass()),
      descriptor_(texture_data_->GetSize(), pixel_format, alpha_format,
                  texture_data_->GetPitchInBytes()) {}

const render_tree::ImageDataDescriptor& HardwareImageData::GetDescriptor()
    const {
  return descriptor_;
}

uint8_t* HardwareImageData::GetMemory() { return texture_data_->GetMemory(); }

scoped_ptr<backend::TextureDataEGL> HardwareImageData::PassTextureData() {
  return texture_data_.Pass();
}

HardwareRawImageMemory::HardwareRawImageMemory(
    scoped_ptr<backend::RawTextureMemoryEGL> raw_texture_memory)
    : raw_texture_memory_(raw_texture_memory.Pass()) {}

size_t HardwareRawImageMemory::GetSizeInBytes() const {
  return raw_texture_memory_->GetSizeInBytes();
}

uint8_t* HardwareRawImageMemory::GetMemory() {
  return raw_texture_memory_->GetMemory();
}

scoped_ptr<backend::RawTextureMemoryEGL>
HardwareRawImageMemory::PassRawTextureMemory() {
  return raw_texture_memory_.Pass();
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
    thread_checker_.DetachFromThread();
  }

  ~HardwareBackendImage() {
    TRACE_EVENT0("cobalt::renderer",
                 "HardwareBackendImage::~HardwareBackendImage()");
    // This object should always be destroyed from the thread that it was
    // constructed on.
    DCHECK(thread_checker_.CalledOnValidThread());
  }

  // Return true if the object was initialized due to this call, or false if
  // the image was already initialized.
  bool EnsureInitialized() {
    DCHECK(thread_checker_.CalledOnValidThread());
    return ResetAndRunIfNotNull(&initialize_function_, this);
  }

  void InitializeTask() {
    DCHECK(thread_checker_.CalledOnValidThread());
    initialized_task_executed_ = true;
    EnsureInitialized();
  }

  bool TryDestroy() {
    DCHECK(thread_checker_.CalledOnValidThread());
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
  //      However, std::function cannot take ownership of scoped_ptr objects,
  //      so such parameters would have to be manually deleted.

  static void InitializeFromImageData(
      scoped_ptr<HardwareImageData> image_data,
      backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context,
      HardwareBackendImage* backend) {
    TRACE_EVENT0("cobalt::renderer",
                 "HardwareBackendImage::InitializeFromImageData()");
    backend->texture_ = cobalt_context->CreateTexture(
        image_data->PassTextureData());
    backend->CommonInitialize(gr_context);
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
    backend->CommonInitialize(gr_context);
  }

  static void InitializeFromTexture(
      scoped_ptr<backend::TextureEGL> texture, GrContext* gr_context,
      HardwareBackendImage* backend) {
    TRACE_EVENT0("cobalt::renderer",
                 "HardwareBackendImage::InitializeFromTexture()");
    backend->texture_ = texture.Pass();
    backend->CommonInitialize(gr_context);
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

    scoped_ptr<backend::TextureEGL> texture(
        new backend::TextureEGL(cobalt_context, render_target));

    InitializeFromTexture(texture.Pass(), gr_context, backend);

    // Tell Skia that the graphics state is unknown because we issued custom
    // GL commands above.
    gr_context->resetContext(kRenderTarget_GrGLBackendState |
                             kTextureBinding_GrGLBackendState);
  }

  // Initiate all texture initialization code here, which should be executed
  // on the rasterizer thread.
  void CommonInitialize(GrContext* gr_context) {
    DCHECK(thread_checker_.CalledOnValidThread());
    TRACE_EVENT0("cobalt::renderer",
                 "HardwareBackendImage::CommonInitialize()");
    if (!texture_->IsValid()) {
      // The system likely did not have enough GPU memory for the texture.
      LOG(ERROR) << "Invalid texture passed to HardwareBackendImage";
      texture_.reset();
      return;
    }

    if (texture_->GetTarget() == GL_TEXTURE_2D) {
      gr_texture_.reset(
          WrapCobaltTextureWithSkiaTexture(gr_context, texture_.get()));
      DCHECK(gr_texture_);

      // Prepare a member SkBitmap that refers to the newly created GrTexture
      // and will be the object that Skia draw calls will reference when
      // referring to this image.
      bitmap_.setInfo(gr_texture_->info());
      bitmap_.setPixelRef(
                 SkNEW_ARGS(SkGrPixelRef, (bitmap_.info(), gr_texture_)))
          ->unref();
    }
  }

  const SkBitmap* GetBitmap() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    return &bitmap_;
  }

  const backend::TextureEGL* GetTextureEGL() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    return texture_.get();
  }

 private:
  // Keep a reference to the texture alive as long as this backend image
  // exists.
  scoped_ptr<backend::TextureEGL> texture_;

  base::ThreadChecker thread_checker_;
  SkAutoTUnref<GrTexture> gr_texture_;
  SkBitmap bitmap_;

  InitializeFunction initialize_function_;
  bool initialized_task_executed_;
};

namespace {
// Given a ImageDataDescriptor, returns a AlternateRgbaFormat value for it,
// which for most formats will be base::nullopt, but for those that piggy-back
// on RGBA but assign different meanings to each of the 4 pixels, this will
// return a special formatting option.
base::optional<AlternateRgbaFormat> AlternateRgbaFormatFromImageDataDescriptor(
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
    const base::optional<AlternateRgbaFormat>& alternate_rgba_format) {
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
    scoped_ptr<HardwareImageData> image_data,
    backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context,
    MessageLoop* rasterizer_message_loop)
    : is_opaque_(image_data->GetDescriptor().alpha_format ==
                 render_tree::kAlphaFormatOpaque),
      alternate_rgba_format_(AlternateRgbaFormatFromImageDataDescriptor(
          image_data->GetDescriptor())),
      size_(AdjustSizeForFormat(image_data->GetDescriptor().size,
                                alternate_rgba_format_)),
      rasterizer_message_loop_(rasterizer_message_loop) {
  backend_image_.reset(new HardwareBackendImage(base::Bind(
      &HardwareBackendImage::InitializeFromImageData,
      base::Passed(&image_data), cobalt_context, gr_context)));
  InitializeBackend();
}

HardwareFrontendImage::HardwareFrontendImage(
    const scoped_refptr<backend::ConstRawTextureMemoryEGL>& raw_texture_memory,
    intptr_t offset, const render_tree::ImageDataDescriptor& descriptor,
    backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context,
    MessageLoop* rasterizer_message_loop)
    : is_opaque_(descriptor.alpha_format == render_tree::kAlphaFormatOpaque),
      alternate_rgba_format_(
          AlternateRgbaFormatFromImageDataDescriptor(descriptor)),
      size_(AdjustSizeForFormat(descriptor.size, alternate_rgba_format_)),
      rasterizer_message_loop_(rasterizer_message_loop) {
  TRACE_EVENT0("cobalt::renderer",
               "HardwareFrontendImage::HardwareFrontendImage()");
  backend_image_.reset(new HardwareBackendImage(base::Bind(
      &HardwareBackendImage::InitializeFromRawImageData,
      raw_texture_memory, offset, descriptor, cobalt_context, gr_context)));
  InitializeBackend();
}

HardwareFrontendImage::HardwareFrontendImage(
    scoped_ptr<backend::TextureEGL> texture,
    render_tree::AlphaFormat alpha_format,
    backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context,
    scoped_ptr<math::Rect> content_region, MessageLoop* rasterizer_message_loop,
    base::optional<AlternateRgbaFormat> alternate_rgba_format)
    : is_opaque_(alpha_format == render_tree::kAlphaFormatOpaque),
      content_region_(content_region.Pass()),
      alternate_rgba_format_(alternate_rgba_format),
      size_(AdjustSizeForFormat(
          content_region_ ? math::Size(std::abs(content_region_->width()),
                                       std::abs(content_region_->height()))
                          : texture->GetSize(),
          alternate_rgba_format_)),
      rasterizer_message_loop_(rasterizer_message_loop) {
  TRACE_EVENT0("cobalt::renderer",
               "HardwareFrontendImage::HardwareFrontendImage()");
  backend_image_.reset(new HardwareBackendImage(base::Bind(
      &HardwareBackendImage::InitializeFromTexture, base::Passed(&texture),
      gr_context)));
  InitializeBackend();
}

HardwareFrontendImage::HardwareFrontendImage(
    const scoped_refptr<render_tree::Node>& root,
    const SubmitOffscreenCallback& submit_offscreen_callback,
    backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context,
    MessageLoop* rasterizer_message_loop)
    : is_opaque_(false),
      size_(AdjustSizeForFormat(
          math::Size(static_cast<int>(root->GetBounds().right()),
                     static_cast<int>(root->GetBounds().bottom())),
          alternate_rgba_format_)),
      rasterizer_message_loop_(rasterizer_message_loop) {
  TRACE_EVENT0("cobalt::renderer",
               "HardwareFrontendImage::HardwareFrontendImage()");
  backend_image_.reset(new HardwareBackendImage(base::Bind(
      &HardwareBackendImage::InitializeFromRenderTree, root, size_,
      submit_offscreen_callback, cobalt_context, gr_context)));
  InitializeBackend();
}

HardwareFrontendImage::~HardwareFrontendImage() {
  TRACE_EVENT0("cobalt::renderer",
               "HardwareFrontendImage::~HardwareFrontendImage()");
  // InitializeBackend() posted a task to call backend_image_'s
  // InitializeTask(). Make sure that task has finished before
  // destroying backend_image_.
  if (rasterizer_message_loop_) {
    if (rasterizer_message_loop_ != MessageLoop::current() ||
        !backend_image_->TryDestroy()) {
      rasterizer_message_loop_->DeleteSoon(FROM_HERE, backend_image_.release());
    }
  }  // else let the scoped pointer clean it up immediately.
}

void HardwareFrontendImage::InitializeBackend() {
  // Initialize the image as soon as possible, rather than waiting for the
  // rasterizer to initialize it when needed. The image initialization process
  // may take some time, so doing a lazy initialize can cause a big spike in
  // frame time if multiple images are initialized in one frame.
  if (rasterizer_message_loop_) {
    rasterizer_message_loop_->PostTask(FROM_HERE, base::Bind(
        &HardwareBackendImage::InitializeTask,
        base::Unretained(backend_image_.get())));
  }
}

const SkBitmap* HardwareFrontendImage::GetBitmap() const {
  DCHECK_EQ(rasterizer_message_loop_, MessageLoop::current());
  // Forward this call to the backend image.  This method must be called from
  // the rasterizer thread (e.g. during a render tree visitation).  The backend
  // image will check that this is being called from the correct thread.
  return backend_image_->GetBitmap();
}

const backend::TextureEGL* HardwareFrontendImage::GetTextureEGL() const {
  DCHECK_EQ(rasterizer_message_loop_, MessageLoop::current());
  return backend_image_->GetTextureEGL();
}

bool HardwareFrontendImage::CanRenderInSkia() const {
  DCHECK_EQ(rasterizer_message_loop_, MessageLoop::current());
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
  DCHECK_EQ(rasterizer_message_loop_, MessageLoop::current());
  return backend_image_->EnsureInitialized();
}

HardwareMultiPlaneImage::HardwareMultiPlaneImage(
    scoped_ptr<HardwareRawImageMemory> raw_image_memory,
    const render_tree::MultiPlaneImageDataDescriptor& descriptor,
    backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context,
    MessageLoop* rasterizer_message_loop)
    : size_(descriptor.GetPlaneDescriptor(0).size),
      format_(descriptor.image_format()) {
  scoped_refptr<backend::ConstRawTextureMemoryEGL> const_raw_texture_memory(
      new backend::ConstRawTextureMemoryEGL(
          raw_image_memory->PassRawTextureMemory()));

  // Construct a single plane image for each plane of this multi plane image.
  for (int i = 0; i < descriptor.num_planes(); ++i) {
    planes_[i] = new HardwareFrontendImage(
        const_raw_texture_memory, descriptor.GetPlaneOffset(i),
        descriptor.GetPlaneDescriptor(i), cobalt_context, gr_context,
        rasterizer_message_loop);
  }
}

HardwareMultiPlaneImage::HardwareMultiPlaneImage(
    render_tree::MultiPlaneImageFormat format,
    const std::vector<scoped_refptr<HardwareFrontendImage> >& planes)
    : size_(planes[0]->GetSize()), format_(format) {
  DCHECK(planes.size() <=
         render_tree::MultiPlaneImageDataDescriptor::kMaxPlanes);
  for (unsigned int i = 0; i < planes.size(); ++i) {
    planes_[i] = planes[i];
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
