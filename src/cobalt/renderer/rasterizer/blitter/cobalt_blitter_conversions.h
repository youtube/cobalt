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

#ifndef COBALT_RENDERER_RASTERIZER_BLITTER_COBALT_BLITTER_CONVERSIONS_H_
#define COBALT_RENDERER_RASTERIZER_BLITTER_COBALT_BLITTER_CONVERSIONS_H_

#include "cobalt/math/rect.h"
#include "cobalt/math/rect_f.h"
#include "starboard/blitter.h"

#if SB_HAS(BLITTER)

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace blitter {

inline SbBlitterRect RectToBlitterRect(const math::Rect& rect) {
  return SbBlitterMakeRect(rect.x(), rect.y(), rect.width(), rect.height());
}

inline SbBlitterRect RectFToBlitterRect(const math::RectF& rectf) {
  return RectToBlitterRect(cobalt::math::Rect::RoundFromRectF(rectf));
}

}  // namespace blitter
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // #if SB_HAS(BLITTER)

#endif  // COBALT_RENDERER_RASTERIZER_BLITTER_COBALT_BLITTER_CONVERSIONS_H_
