// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_RENDERER_RASTERIZER_COMMON_UTILS_H_
#define COBALT_RENDERER_RASTERIZER_COMMON_UTILS_H_

#include "cobalt/render_tree/node.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace common {
namespace utils {

// Returns whether the given node (or its child[ren]) can be rendered directly
// onto the main render target with an opacity value. The alternative is that
// the node should be rendered onto an offscreen render target, then that
// target be rendered onscreen using opacity. The difference can best be seen
// when |node| represents overlapping objects -- rendering them individually
// with opacity will look different than rendering them fully opaque onto an
// offscreen target, then rendering that target with opacity.
bool NodeCanRenderWithOpacity(render_tree::Node* node);

// Returns whether the given opacity [0,1] should be considered fully opaque.
bool IsOpaque(float opacity);

// Returns whether the given opacity [0,1] should be considered fully
// transparent.
bool IsTransparent(float opacity);

}  // namespace utils
}  // namespace common
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_COMMON_UTILS_H_
