// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/renderer/rasterizer/blitter/render_tree_blitter_conversions.h"

#include "cobalt/render_tree/image.h"
#include "starboard/blitter.h"

#if SB_API_VERSION < 12 && SB_HAS(BLITTER)

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace blitter {

SbBlitterPixelDataFormat RenderTreePixelFormatToBlitter(
    render_tree::PixelFormat format) {
  switch (format) {
    case render_tree::kPixelFormatRGBA8:
      return kSbBlitterPixelDataFormatRGBA8;
    case render_tree::kPixelFormatBGRA8:
      return kSbBlitterPixelDataFormatBGRA8;
    // The backend does not need to distinguish between Y/U/V images, all it
    // needs to know is that they are 8-bit greyscale images, which is what
    // we are telling it when we return kSbBlitterPixelDataFormatA8.
    case render_tree::kPixelFormatY8:
      return kSbBlitterPixelDataFormatA8;
    case render_tree::kPixelFormatU8:
      return kSbBlitterPixelDataFormatA8;
    case render_tree::kPixelFormatV8:
      return kSbBlitterPixelDataFormatA8;
    default: {
      NOTREACHED() << "Unknown render tree pixel format.";
      return kSbBlitterPixelDataFormatRGBA8;
    }
  }
}

}  // namespace blitter
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_API_VERSION < 12 && SB_HAS(BLITTER)
