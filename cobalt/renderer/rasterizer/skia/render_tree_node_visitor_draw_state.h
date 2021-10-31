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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_RENDER_TREE_NODE_VISITOR_DRAW_STATE_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_RENDER_TREE_NODE_VISITOR_DRAW_STATE_H_

#include "third_party/glm/glm/mat4x4.hpp"
#include "third_party/skia/include/core/SkCanvas.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

struct RenderTreeNodeVisitorDrawState {
  explicit RenderTreeNodeVisitorDrawState(SkCanvas* render_target)
      : render_target(render_target),
        opacity(1.0f),
        clip_is_rect(true),
        transform_3d(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1) {}

  SkCanvas* render_target;
  float opacity;

  // True if the current clip is a rectangle or not.  If it is not, we need
  // to enable blending when rendering clipped rectangles.
  bool clip_is_rect;

  glm::mat4 transform_3d;
};

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_RENDER_TREE_NODE_VISITOR_DRAW_STATE_H_
