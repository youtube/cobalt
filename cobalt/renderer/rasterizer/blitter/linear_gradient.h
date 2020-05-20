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

#ifndef COBALT_RENDERER_RASTERIZER_BLITTER_LINEAR_GRADIENT_H_
#define COBALT_RENDERER_RASTERIZER_BLITTER_LINEAR_GRADIENT_H_

#include "cobalt/math/rect_f.h"
#include "cobalt/render_tree/rect_node.h"
#include "cobalt/renderer/rasterizer/blitter/linear_gradient_cache.h"
#include "cobalt/renderer/rasterizer/blitter/render_state.h"
#include "starboard/blitter.h"

#if SB_API_VERSION < 12 && SB_HAS(BLITTER)

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace blitter {

bool RenderLinearGradient(SbBlitterDevice device, SbBlitterContext context,
                          const RenderState& render_state,
                          const render_tree::RectNode& rect_node,
                          LinearGradientCache* linear_gradient_cache);

}  // namespace blitter
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_API_VERSION < 12 && SB_HAS(BLITTER)

#endif  // COBALT_RENDERER_RASTERIZER_BLITTER_LINEAR_GRADIENT_H_
