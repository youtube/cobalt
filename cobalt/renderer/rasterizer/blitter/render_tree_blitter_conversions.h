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

#ifndef COBALT_RENDERER_RASTERIZER_BLITTER_RENDER_TREE_BLITTER_CONVERSIONS_H_
#define COBALT_RENDERER_RASTERIZER_BLITTER_RENDER_TREE_BLITTER_CONVERSIONS_H_

#include "cobalt/render_tree/image.h"
#include "starboard/blitter.h"

#if SB_API_VERSION < 12 && SB_HAS(BLITTER)

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace blitter {

// Converts between render_tree pixel formats to Starboard Blitter API pixel
// formats.
SbBlitterPixelDataFormat RenderTreePixelFormatToBlitter(
    render_tree::PixelFormat format);

}  // namespace blitter
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_API_VERSION < 12 && SB_HAS(BLITTER)

#endif  // COBALT_RENDERER_RASTERIZER_BLITTER_RENDER_TREE_BLITTER_CONVERSIONS_H_
