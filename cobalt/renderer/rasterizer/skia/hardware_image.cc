/*
 * Copyright 2014 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/renderer/rasterizer/skia/hardware_image.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
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

SkiaHardwareImageData::SkiaHardwareImageData(
    scoped_ptr<backend::TextureDataEGL> texture_data,
    render_tree::PixelFormat pixel_format,
    render_tree::AlphaFormat alpha_format)
    : texture_data_(texture_data.Pass()),
      descriptor_(texture_data_->GetSize(), pixel_format, alpha_format,
                  texture_data_->GetPitchInBytes()) {}

const render_tree::ImageDataDescriptor& SkiaHardwareImageData::GetDescriptor()
    const {
  return descriptor_;
}

uint8_t* SkiaHardwareImageData::GetMemory() {
  return texture_data_->GetMemory();
}

scoped_ptr<backend::TextureDataEGL> SkiaHardwareImageData::PassTextureData() {
  return texture_data_.Pass();
}

SkiaHardwareRawImageMemory::SkiaHardwareRawImageMemory(
    scoped_ptr<backend::RawTextureMemoryEGL> raw_texture_memory)
    : raw_texture_memory_(raw_texture_memory.Pass()) {}

size_t SkiaHardwareRawImageMemory::GetSizeInBytes() const {
  return raw_texture_memory_->GetSizeInBytes();
}

uint8_t* SkiaHardwareRawImageMemory::GetMemory() {
  return raw_texture_memory_->GetMemory();
}

scoped_ptr<backend::RawTextureMemoryEGL>
SkiaHardwareRawImageMemory::PassRawTextureMemory() {
  return raw_texture_memory_.Pass();
}

// This will store the given pixel data in a GrTexture and the function
// GetBitmap(), overridden from SkiaImage, will return a reference to a SkBitmap
// object that refers to the image's GrTexture.  This object should only be
// constructed, destructed and used from the same rasterizer thread.
class SkiaHardwareFrontendImage::SkiaHardwareBackendImage {
 public:
  SkiaHardwareBackendImage(scoped_ptr<SkiaHardwareImageData> image_data,
                           backend::GraphicsContextEGL* cobalt_context,
                           GrContext* gr_context) {
    TRACE_EVENT0("cobalt::renderer",
                 "SkiaHardwareBackendImage::SkiaHardwareBackendImage()");
    scoped_ptr<backend::TextureEGL> texture =
        cobalt_context->CreateTexture(image_data->PassTextureData());

    CommonInitialize(texture.Pass(), gr_context);
  }

  SkiaHardwareBackendImage(
      const scoped_refptr<backend::ConstRawTextureMemoryEGL>&
          raw_texture_memory,
      intptr_t offset, const render_tree::ImageDataDescriptor& descriptor,
      backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context) {
    TRACE_EVENT0("cobalt::renderer",
                 "SkiaHardwareBackendImage::SkiaHardwareBackendImage()");
    scoped_ptr<backend::TextureEGL> texture =
        cobalt_context->CreateTextureFromRawMemory(
            raw_texture_memory, offset, descriptor.size,
            ConvertRenderTreeFormatToGL(descriptor.pixel_format),
            descriptor.pitch_in_bytes);

    CommonInitialize(texture.Pass(), gr_context);
  }

  ~SkiaHardwareBackendImage() {
    TRACE_EVENT0("cobalt::renderer",
                 "SkiaHardwareBackendImage::~SkiaHardwareBackendImage()");
    // This object should always be destroyed from the thread that it was
    // constructed on.
    DCHECK(thread_checker_.CalledOnValidThread());
  }

  // Initiate all texture initialization code here, which should be executed
  // on the rasterizer thread.
  void CommonInitialize(scoped_ptr<backend::TextureEGL> texture,
                        GrContext* gr_context) {
    DCHECK(thread_checker_.CalledOnValidThread());
    TRACE_EVENT0("cobalt::renderer",
                 "SkiaHardwareBackendImage::CommonInitialize()");

    texture_ = texture.Pass();
    gr_texture_.reset(
        WrapCobaltTextureWithSkiaTexture(gr_context, texture_.get()));
    DCHECK(gr_texture_);

    // Prepare a member SkBitmap that refers to the newly created GrTexture and
    // will be the object that Skia draw calls will reference when referring
    // to this image.
    bitmap_.setInfo(gr_texture_->info());
    bitmap_.setPixelRef(SkNEW_ARGS(SkGrPixelRef, (bitmap_.info(), gr_texture_)))
        ->unref();
  }

  const SkBitmap& GetBitmap() const {
    DCHECK(thread_checker_.CalledOnValidThread());
    return bitmap_;
  }

 private:
  // Keep a reference to the texture alive as long as this backend image
  // exists.
  scoped_ptr<backend::TextureEGL> texture_;

  base::ThreadChecker thread_checker_;
  SkAutoTUnref<GrTexture> gr_texture_;
  SkBitmap bitmap_;
};

SkiaHardwareFrontendImage::SkiaHardwareFrontendImage(
    scoped_ptr<SkiaHardwareImageData> image_data,
    backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context,
    MessageLoop* rasterizer_message_loop)
    : size_(image_data->GetDescriptor().size),
      rasterizer_message_loop_(rasterizer_message_loop) {
  TRACE_EVENT0("cobalt::renderer",
               "SkiaHardwareFrontendImage::SkiaHardwareFrontendImage()");

  initialize_backend_image_ = base::Bind(
      &SkiaHardwareFrontendImage::InitializeBackendImageFromImageData,
      base::Unretained(this), base::Passed(&image_data), cobalt_context,
      gr_context);
}

SkiaHardwareFrontendImage::SkiaHardwareFrontendImage(
    const scoped_refptr<backend::ConstRawTextureMemoryEGL>& raw_texture_memory,
    intptr_t offset, const render_tree::ImageDataDescriptor& descriptor,
    backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context,
    MessageLoop* rasterizer_message_loop)
    : size_(descriptor.size),
      rasterizer_message_loop_(rasterizer_message_loop) {
  TRACE_EVENT0("cobalt::renderer",
               "SkiaHardwareFrontendImage::SkiaHardwareFrontendImage()");
  initialize_backend_image_ = base::Bind(
      &SkiaHardwareFrontendImage::InitializeBackendImageFromRawImageData,
      base::Unretained(this), raw_texture_memory, offset, descriptor,
      cobalt_context, gr_context);
}

SkiaHardwareFrontendImage::~SkiaHardwareFrontendImage() {
  TRACE_EVENT0("cobalt::renderer",
               "SkiaHardwareFrontendImage::~SkiaHardwareFrontendImage()");
  // If we are destroying this image from a non-rasterizer thread, we still must
  // ensure that the |backend_image_| is destroyed from the rasterizer thread,
  // if |backend_image_| was ever constructed in the first place.
  if (backend_image_ && rasterizer_message_loop_ &&
      rasterizer_message_loop_ != MessageLoop::current()) {
    rasterizer_message_loop_->DeleteSoon(FROM_HERE, backend_image_.release());
  }  // else let the scoped pointer clean it up immediately.
}

const SkBitmap& SkiaHardwareFrontendImage::GetBitmap() const {
  DCHECK_EQ(rasterizer_message_loop_, MessageLoop::current());
  // Forward this call to the backend image.  This method must be called from
  // the rasterizer thread (e.g. during a render tree visitation).  The backend
  // image will check that this is being called from the correct thread.
  return backend_image_->GetBitmap();
}

void SkiaHardwareFrontendImage::EnsureInitialized() {
  DCHECK_EQ(rasterizer_message_loop_, MessageLoop::current());
  if (!initialize_backend_image_.is_null()) {
    initialize_backend_image_.Run();
    initialize_backend_image_.Reset();
  }
}

void SkiaHardwareFrontendImage::InitializeBackendImageFromImageData(
    scoped_ptr<SkiaHardwareImageData> image_data,
    backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context) {
  DCHECK_EQ(rasterizer_message_loop_, MessageLoop::current());
  backend_image_.reset(new SkiaHardwareBackendImage(
      image_data.Pass(), cobalt_context, gr_context));
}

void SkiaHardwareFrontendImage::InitializeBackendImageFromRawImageData(
    const scoped_refptr<backend::ConstRawTextureMemoryEGL>& raw_texture_memory,
    intptr_t offset, const render_tree::ImageDataDescriptor& descriptor,
    backend::GraphicsContextEGL* cobalt_context, GrContext* gr_context) {
  DCHECK_EQ(rasterizer_message_loop_, MessageLoop::current());
  backend_image_.reset(new SkiaHardwareBackendImage(
      raw_texture_memory, offset, descriptor, cobalt_context, gr_context));
}

SkiaHardwareMultiPlaneImage::SkiaHardwareMultiPlaneImage(
    scoped_ptr<SkiaHardwareRawImageMemory> raw_image_memory,
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
    planes_[i] = new SkiaHardwareFrontendImage(
        const_raw_texture_memory, descriptor.GetPlaneOffset(i),
        descriptor.GetPlaneDescriptor(i), cobalt_context, gr_context,
        rasterizer_message_loop);
  }
}

SkiaHardwareMultiPlaneImage::~SkiaHardwareMultiPlaneImage() {}

void SkiaHardwareMultiPlaneImage::EnsureInitialized() {
  // A multi-plane image is not considered backend-initialized until all its
  // single-plane images are backend-initialized, thus we ensure that all
  // the component images are backend-initialized.
  for (int i = 0; i < render_tree::MultiPlaneImageDataDescriptor::kMaxPlanes;
       ++i) {
    if (planes_[i]) {
      planes_[i]->EnsureInitialized();
    }
  }
}

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
