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

#include "base/bind.h"
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
// constructed, destructed and used from the same rasterizer thread.
class HardwareFrontendImage::HardwareBackendImage {
 public:
  HardwareBackendImage(scoped_ptr<HardwareImageData> image_data,
                       backend::GraphicsContextEGL* cobalt_context,
                       GrContext* gr_context) {
    TRACE_EVENT0("cobalt::renderer",
                 "HardwareBackendImage::HardwareBackendImage()");
    texture_ = cobalt_context->CreateTexture(image_data->PassTextureData());

    CommonInitialize(gr_context);
  }

  HardwareBackendImage(const scoped_refptr<backend::ConstRawTextureMemoryEGL>&
                           raw_texture_memory,
                       intptr_t offset,
                       const render_tree::ImageDataDescriptor& descriptor,
                       backend::GraphicsContextEGL* cobalt_context,
                       GrContext* gr_context) {
    TRACE_EVENT0("cobalt::renderer",
                 "HardwareBackendImage::HardwareBackendImage()");
    texture_ = cobalt_context->CreateTextureFromRawMemory(
        raw_texture_memory, offset, descriptor.size,
        ConvertRenderTreeFormatToGL(descriptor.pixel_format),
        descriptor.pitch_in_bytes);

    CommonInitialize(gr_context);
  }

  explicit HardwareBackendImage(scoped_ptr<backend::TextureEGL> texture) {
    TRACE_EVENT0("cobalt::renderer",
                 "HardwareBackendImage::HardwareBackendImage()");
    // This constructor can be called from any thread. However,
    // CommonInitialize() must then be called manually from the rasterizer
    // thread.
    texture_ = texture.Pass();
    thread_checker_.DetachFromThread();
  }

  ~HardwareBackendImage() {
    TRACE_EVENT0("cobalt::renderer",
                 "HardwareBackendImage::~HardwareBackendImage()");
    // This object should always be destroyed from the thread that it was
    // constructed on.
    DCHECK(thread_checker_.CalledOnValidThread());
  }

  // Initiate all texture initialization code here, which should be executed
  // on the rasterizer thread.
  void CommonInitialize(GrContext* gr_context) {
    DCHECK(thread_checker_.CalledOnValidThread());
    TRACE_EVENT0("cobalt::renderer",
                 "HardwareBackendImage::CommonInitialize()");
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
};

HardwareFrontendImage::HardwareFrontendImage(
    scoped_ptr<HardwareImageData> image_data,
    backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context,
    MessageLoop* rasterizer_message_loop)
    : is_opaque_(image_data->GetDescriptor().alpha_format ==
                 render_tree::kAlphaFormatOpaque),
      size_(image_data->GetDescriptor().size),
      rasterizer_message_loop_(rasterizer_message_loop) {
  TRACE_EVENT0("cobalt::renderer",
               "HardwareFrontendImage::HardwareFrontendImage()");

  initialize_backend_image_ =
      base::Bind(&HardwareFrontendImage::InitializeBackendImageFromImageData,
                 base::Unretained(this), base::Passed(&image_data),
                 cobalt_context, gr_context);
}

HardwareFrontendImage::HardwareFrontendImage(
    const scoped_refptr<backend::ConstRawTextureMemoryEGL>& raw_texture_memory,
    intptr_t offset, const render_tree::ImageDataDescriptor& descriptor,
    backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context,
    MessageLoop* rasterizer_message_loop)
    : is_opaque_(descriptor.alpha_format == render_tree::kAlphaFormatOpaque),
      size_(descriptor.size),
      rasterizer_message_loop_(rasterizer_message_loop) {
  TRACE_EVENT0("cobalt::renderer",
               "HardwareFrontendImage::HardwareFrontendImage()");
  initialize_backend_image_ =
      base::Bind(&HardwareFrontendImage::InitializeBackendImageFromRawImageData,
                 base::Unretained(this), raw_texture_memory, offset, descriptor,
                 cobalt_context, gr_context);
}

HardwareFrontendImage::HardwareFrontendImage(
    scoped_ptr<backend::TextureEGL> texture,
    render_tree::AlphaFormat alpha_format,
    backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context,
    scoped_ptr<math::Rect> content_region, MessageLoop* rasterizer_message_loop)
    : is_opaque_(alpha_format == render_tree::kAlphaFormatOpaque),
      content_region_(content_region.Pass()),
      size_(content_region_ ? math::Size(std::abs(content_region_->width()),
                                         std::abs(content_region_->height()))
                            : texture->GetSize()),
      rasterizer_message_loop_(rasterizer_message_loop) {
  TRACE_EVENT0("cobalt::renderer",
               "HardwareFrontendImage::HardwareFrontendImage()");
  backend_image_.reset(new HardwareBackendImage(texture.Pass()));
  initialize_backend_image_ =
      base::Bind(&HardwareFrontendImage::InitializeBackendImageFromTexture,
                 base::Unretained(this), gr_context);
}

HardwareFrontendImage::HardwareFrontendImage(
    const scoped_refptr<render_tree::Node>& root,
    const SubmitOffscreenCallback& submit_offscreen_callback,
    backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context,
    MessageLoop* rasterizer_message_loop)
    : is_opaque_(false),
      size_(static_cast<int>(root->GetBounds().right()),
            static_cast<int>(root->GetBounds().bottom())),
      rasterizer_message_loop_(rasterizer_message_loop) {
  TRACE_EVENT0("cobalt::renderer",
               "HardwareFrontendImage::HardwareFrontendImage()");
  initialize_backend_image_ =
      base::Bind(&HardwareFrontendImage::InitializeBackendImageFromRenderTree,
                 base::Unretained(this), root, submit_offscreen_callback,
                 cobalt_context, gr_context);
}

HardwareFrontendImage::~HardwareFrontendImage() {
  TRACE_EVENT0("cobalt::renderer",
               "HardwareFrontendImage::~HardwareFrontendImage()");
  // If we are destroying this image from a non-rasterizer thread, we still must
  // ensure that the |backend_image_| is destroyed from the rasterizer thread,
  // if |backend_image_| was ever constructed in the first place.
  if (backend_image_ && rasterizer_message_loop_ &&
      rasterizer_message_loop_ != MessageLoop::current()) {
    rasterizer_message_loop_->DeleteSoon(FROM_HERE, backend_image_.release());
  }  // else let the scoped pointer clean it up immediately.
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
         (!GetTextureEGL() || GetTextureEGL()->GetTarget() == GL_TEXTURE_2D);
}

bool HardwareFrontendImage::EnsureInitialized() {
  DCHECK_EQ(rasterizer_message_loop_, MessageLoop::current());
  if (!initialize_backend_image_.is_null()) {
    initialize_backend_image_.Run();
    initialize_backend_image_.Reset();
    return true;
  }
  return false;
}

void HardwareFrontendImage::InitializeBackendImageFromImageData(
    scoped_ptr<HardwareImageData> image_data,
    backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context) {
  DCHECK_EQ(rasterizer_message_loop_, MessageLoop::current());
  backend_image_.reset(
      new HardwareBackendImage(image_data.Pass(), cobalt_context, gr_context));
}

void HardwareFrontendImage::InitializeBackendImageFromRawImageData(
    const scoped_refptr<backend::ConstRawTextureMemoryEGL>& raw_texture_memory,
    intptr_t offset, const render_tree::ImageDataDescriptor& descriptor,
    backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context) {
  DCHECK_EQ(rasterizer_message_loop_, MessageLoop::current());
  backend_image_.reset(new HardwareBackendImage(
      raw_texture_memory, offset, descriptor, cobalt_context, gr_context));
}

void HardwareFrontendImage::InitializeBackendImageFromTexture(
    GrContext* gr_context) {
  DCHECK_EQ(rasterizer_message_loop_, MessageLoop::current());
  backend_image_->CommonInitialize(gr_context);
}

void HardwareFrontendImage::InitializeBackendImageFromRenderTree(
    const scoped_refptr<render_tree::Node>& root,
    const SubmitOffscreenCallback& submit_offscreen_callback,
    backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context) {
  DCHECK_EQ(rasterizer_message_loop_, MessageLoop::current());

  scoped_refptr<backend::FramebufferRenderTargetEGL> render_target(
      new backend::FramebufferRenderTargetEGL(cobalt_context, size_));

  submit_offscreen_callback.Run(root, render_target);

  scoped_ptr<backend::TextureEGL> texture(
      new backend::TextureEGL(cobalt_context, render_target));

  backend_image_.reset(new HardwareBackendImage(texture.Pass()));
  backend_image_->CommonInitialize(gr_context);
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
