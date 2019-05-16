// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/renderer/rasterizer/skia/image.h"

#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

void Image::DCheckForPremultipliedAlpha(const math::Size& dimensions,
                                        int source_pitch_in_bytes,
                                        render_tree::PixelFormat pixel_format,
                                        const uint8_t* source_pixels) {
  TRACE_EVENT0("cobalt::renderer", "Image::DCheckForPremultipliedAlpha()");
  if (pixel_format == render_tree::kPixelFormatUYVY ||
      pixel_format == render_tree::kPixelFormatY8 ||
      pixel_format == render_tree::kPixelFormatU8 ||
      pixel_format == render_tree::kPixelFormatV8 ||
      pixel_format == render_tree::kPixelFormatUV8) {
    // These formats don't have alpha, so they are trivially good to go.
    return;
  }

  DCHECK(pixel_format == render_tree::kPixelFormatRGBA8 ||
         pixel_format == render_tree::kPixelFormatBGRA8);
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

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
