/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/renderer/rasterizer/blitter/image.h"

#include "cobalt/render_tree/image.h"
#include "cobalt/renderer/rasterizer/blitter/render_tree_blitter_conversions.h"
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
          RenderTreePixelFormatToBlitter(pixel_format),
          RenderTreeAlphaFormatToBlitter(alpha_format))),
      descriptor_(size, pixel_format, alpha_format,
                  SbBlitterGetPixelDataPitchInBytes(pixel_data_)) {}

ImageData::~ImageData() {
  if (SbBlitterIsPixelDataValid(pixel_data_)) {
    SbBlitterDestroyPixelData(pixel_data_);
  }
}

uint8* ImageData::GetMemory() {
  return static_cast<uint8*>(SbBlitterGetPixelDataPointer(pixel_data_));
}

SbBlitterPixelData ImageData::TakePixelData() {
  SbBlitterPixelData pixel_data = pixel_data_;
  pixel_data_ = kSbBlitterInvalidPixelData;
  return pixel_data;
}

Image::Image(scoped_ptr<ImageData> image_data)
    : size_(image_data->GetDescriptor().size) {
  surface_ = SbBlitterCreateSurfaceFromPixelData(image_data->device(),
                                                 image_data->TakePixelData());
}

Image::~Image() { SbBlitterDestroySurface(surface_); }

}  // namespace blitter
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // #if SB_HAS(BLITTER)
