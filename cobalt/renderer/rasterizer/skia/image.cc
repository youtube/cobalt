/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/renderer/rasterizer/skia/image.h"

#include "base/debug/trace_event.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

void SkiaImage::SkiaConvertImageData(
    const math::Size& dimensions,
    int source_pitch_in_bytes,
    SkColorType source_color_type, SkAlphaType source_alpha_type,
    const uint8_t* source_pixels,
    int destination_pitch_in_bytes,
    uint8_t* destination_pixels,
    SkColorType dest_color_type, SkAlphaType dest_alpha_type) {
  TRACE_EVENT0("cobalt::renderer", "SkiaImage::SkiaConvertImageData()");

  // Create a temporary SkBitmap wrapping the incoming source image data with
  // RGB components unpremultiplied by the alpha component, as we expect them
  // to be.
  SkImageInfo image_info = SkImageInfo::Make(
      dimensions.width(), dimensions.height(),
      source_color_type, source_alpha_type);
  SkBitmap unpremul_bitmap;
  unpremul_bitmap.installPixels(
      image_info, const_cast<uint8_t*>(source_pixels), source_pitch_in_bytes);

  // Now convert these out into our stored SkBitmap with alpha premultiplied
  // values, as Skia expects them to be formatted.
  SkImageInfo premul_image_info = SkImageInfo::Make(
      dimensions.width(), dimensions.height(),
      dest_color_type, dest_alpha_type);
  unpremul_bitmap.readPixels(
      premul_image_info, destination_pixels, destination_pitch_in_bytes, 0, 0);
}

void SkiaImage::DCheckForPremultipliedAlpha(
    const math::Size& dimensions, int source_pitch_in_bytes,
    render_tree::PixelFormat pixel_format, const uint8_t* source_pixels) {
  TRACE_EVENT0("cobalt::renderer", "SkiaImage::DCheckForPremultipliedAlpha()");
  DCHECK_EQ(render_tree::kPixelFormatRGBA8, pixel_format);
  for (int row = 0; row < dimensions.height(); ++row) {
    const uint8_t* current_pixel = source_pixels + row * source_pitch_in_bytes;
    for (int column = 0; column < dimensions.width(); ++column) {
      uint8_t alpha_value = current_pixel[3];
      DCHECK_LE(current_pixel[0], alpha_value);
      DCHECK_LE(current_pixel[1], alpha_value);
      DCHECK_LE(current_pixel[2], alpha_value);
      current_pixel += 4;
    }
  }
}

void SkiaSinglePlaneImage::Accept(SkiaImageVisitor* visitor) {
  visitor->VisitSinglePlaneImage(this);
}

void SkiaMultiPlaneImage::Accept(SkiaImageVisitor* visitor) {
  visitor->VisitMultiPlaneImage(this);
}

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
