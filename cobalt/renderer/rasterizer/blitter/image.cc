// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/renderer/rasterizer/blitter/image.h"

#include "base/bind.h"
#include "cobalt/render_tree/image.h"
#include "cobalt/renderer/backend/blitter/surface_render_target.h"
#include "cobalt/renderer/rasterizer/blitter/render_tree_blitter_conversions.h"
#include "cobalt/renderer/rasterizer/blitter/skia_blitter_conversions.h"
#include "cobalt/renderer/rasterizer/skia/image.h"
#include "starboard/blitter.h"

#if SB_HAS(BLITTER)

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace blitter {

ImageData::ImageData(SbBlitterDevice device, const math::Size& size,
                     render_tree::PixelFormat pixel_format,
                     render_tree::AlphaFormat alpha_format)
    : device_(device),
      pixel_data_(SbBlitterCreatePixelData(
          device_, size.width(), size.height(),
          RenderTreePixelFormatToBlitter(pixel_format))) {
  CHECK(alpha_format == render_tree::kAlphaFormatPremultiplied ||
        alpha_format == render_tree::kAlphaFormatOpaque);

  if (SbBlitterIsPixelDataValid(pixel_data_)) {
    descriptor_.emplace(size, pixel_format, alpha_format,
                        SbBlitterGetPixelDataPitchInBytes(pixel_data_));
  } else {
    LOG(ERROR) << "Failed to allocate pixel data for image.";
  }
}

ImageData::~ImageData() {
  if (SbBlitterIsPixelDataValid(pixel_data_)) {
    SbBlitterDestroyPixelData(pixel_data_);
  }
}

uint8* ImageData::GetMemory() {
  if (!SbBlitterIsPixelDataValid(pixel_data_)) {
    return NULL;
  } else {
    return static_cast<uint8*>(SbBlitterGetPixelDataPointer(pixel_data_));
  }
}

SbBlitterPixelData ImageData::TakePixelData() {
  SbBlitterPixelData pixel_data = pixel_data_;
  pixel_data_ = kSbBlitterInvalidPixelData;
  return pixel_data;
}

SinglePlaneImage::SinglePlaneImage(scoped_ptr<ImageData> image_data)
    : size_(image_data->GetDescriptor().size) {
  surface_ = SbBlitterCreateSurfaceFromPixelData(image_data->device(),
                                                 image_data->TakePixelData());
  CHECK(SbBlitterIsSurfaceValid(surface_));

  is_opaque_ = image_data->GetDescriptor().alpha_format ==
               render_tree::kAlphaFormatOpaque;
}

SinglePlaneImage::SinglePlaneImage(SbBlitterSurface surface, bool is_opaque,
                                   const base::Closure& delete_function)
    : surface_(surface),
      is_opaque_(is_opaque),
      delete_function_(delete_function) {
  CHECK(SbBlitterIsSurfaceValid(surface_));
  SbBlitterSurfaceInfo info;
  if (!SbBlitterGetSurfaceInfo(surface_, &info)) {
    NOTREACHED();
  }
  size_ = math::Size(info.width, info.height);
}

SinglePlaneImage::SinglePlaneImage(
    const scoped_refptr<render_tree::Node>& root,
    SubmitOffscreenCallback submit_offscreen_callback, SbBlitterDevice device)
    : size_(static_cast<int>(root->GetBounds().right()),
            static_cast<int>(root->GetBounds().bottom())),
      is_opaque_(false),
      surface_(kSbBlitterInvalidSurface) {
  initialize_image_ = base::Bind(
      &SinglePlaneImage::InitializeImageFromRenderTree, base::Unretained(this),
      root, submit_offscreen_callback, device);
}

bool SinglePlaneImage::EnsureInitialized() {
  if (!initialize_image_.is_null()) {
    initialize_image_.Run();
    initialize_image_.Reset();
    return true;
  }
  return false;
}

const sk_sp<SkImage>& SinglePlaneImage::GetImage() const {
  // This function will only ever get called if the Skia software renderer needs
  // to reference the image, and so should be called rarely.  In that case, the
  // first time it is called on this image, we will download the image data from
  // the Blitter API surface into a SkBitmap object where the pixel data lives
  // in CPU memory.  It will then create an SkImage from that SkBitmap object,
  // and the pointer to the new SkImage object will be returned.
  if (!image_) {
    SkImageInfo image_info = SkImageInfo::Make(
        size_.width(), size_.height(), kN32_SkColorType, kPremul_SkAlphaType);

    SkBitmap bitmap;
    bitmap.allocPixels(image_info);
    bool result = SbBlitterDownloadSurfacePixels(
        surface_, SkiaToBlitterPixelFormat(image_info.colorType()),
        bitmap.rowBytes(), bitmap.getPixels());
    if (!result) {
      LOG(WARNING) << "Failed to download surface pixel data so that it could "
                      "be accessed by software skia.";
      NOTREACHED();
    } else {
      image_ = SkImage::MakeFromBitmap(bitmap);
    }
  }

  return image_;
}

SinglePlaneImage::~SinglePlaneImage() {
  if (!delete_function_.is_null()) {
    delete_function_.Run();
  } else {
    SbBlitterDestroySurface(surface_);
  }
}

void SinglePlaneImage::InitializeImageFromRenderTree(
    const scoped_refptr<render_tree::Node>& root,
    const SubmitOffscreenCallback& submit_offscreen_callback,
    SbBlitterDevice device) {
  scoped_refptr<backend::SurfaceRenderTargetBlitter> render_target(
      new backend::SurfaceRenderTargetBlitter(device, size_));

  submit_offscreen_callback.Run(root, render_target);
  surface_ = render_target->TakeSbSurface();
  CHECK(SbBlitterIsSurfaceValid(surface_));
}

}  // namespace blitter
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // #if SB_HAS(BLITTER)
