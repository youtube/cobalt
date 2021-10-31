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

#include "cobalt/renderer/rasterizer/blitter/render_state.h"

#include "cobalt/renderer/rasterizer/blitter/cobalt_blitter_conversions.h"

#if SB_API_VERSION < 12 && SB_HAS(BLITTER)

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace blitter {

BoundsStack::BoundsStack(SbBlitterContext context,
                         const math::Rect& initial_bounds)
    : context_(context) {
  bounds_.push(initial_bounds);
  UpdateContext();
}

void BoundsStack::Push(const math::Rect& bounds) {
  DCHECK_LE(1, bounds_.size())
      << "There must always be at least an initial bounds on the stack.";

  // Push onto the stack the rectangle that is the intersection with the current
  // top of the stack and the rectangle being pushed.
  bounds_.push(math::IntersectRects(bounds, bounds_.top()));
  UpdateContext();
}

void BoundsStack::Pop() {
  DCHECK_LT(1, bounds_.size()) << "Cannot pop the initial bounds.";
  bounds_.pop();
  UpdateContext();
}

void BoundsStack::UpdateContext() {
  SbBlitterSetScissor(context_, RectToBlitterRect(bounds_.top()));
}

}  // namespace blitter
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_API_VERSION < 12 && SB_HAS(BLITTER)
