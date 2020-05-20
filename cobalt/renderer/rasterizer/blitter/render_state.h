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

#ifndef COBALT_RENDERER_RASTERIZER_BLITTER_RENDER_STATE_H_
#define COBALT_RENDERER_RASTERIZER_BLITTER_RENDER_STATE_H_

#include <algorithm>
#include <stack>

#include "base/logging.h"
#include "cobalt/math/matrix3_f.h"
#include "cobalt/math/rect.h"
#include "cobalt/math/rect_f.h"
#include "cobalt/math/vector2d_f.h"
#include "cobalt/render_tree/brush.h"
#include "starboard/blitter.h"

#if SB_API_VERSION < 12 && SB_HAS(BLITTER)

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace blitter {

// Keeps track of the current scale/offset transforms.  Note that while the
// render tree supports general affine transformations, the Starboard Blitter
// API only supports scale and translations, so we maintain only those.  If
// rotations are used, we can fallback to software rendering.
class Transform {
 public:
  Transform() : scale_(1.0f, 1.0f), translate_(0.0f, 0.0f) {}

  explicit Transform(const math::Matrix3F& matrix)
      : scale_(matrix.Get(0, 0), matrix.Get(1, 1)),
        translate_(matrix.Get(0, 2), matrix.Get(1, 2)) {
    // We cannot handle sheers and rotations.
    DCHECK_EQ(0, matrix.Get(0, 1));
    DCHECK_EQ(0, matrix.Get(1, 0));
  }

  Transform(const math::Vector2dF& scale, const math::Vector2dF& translate)
      : scale_(scale), translate_(translate) {}

  const math::Vector2dF& scale() const { return scale_; }
  const math::Vector2dF& translate() const { return translate_; }

  void ApplyOffset(const math::Vector2dF& offset) {
    translate_ +=
        math::Vector2dF(scale_.x() * offset.x(), scale_.y() * offset.y());
  }
  void ApplyScale(const math::Vector2dF& scale_mult) {
    scale_.Scale(scale_mult.x(), scale_mult.y());
  }

  math::RectF TransformRect(const math::RectF& rect) const {
    return math::RectF(rect.x() * scale_.x() + translate_.x(),
                       rect.y() * scale_.y() + translate_.y(),
                       rect.width() * scale_.x(), rect.height() * scale_.y());
  }

  math::Matrix3F ToMatrix() const {
    return math::Matrix3F::FromValues(scale_.x(), 0, translate_.x(), 0,
                                      scale_.y(), translate_.y(), 0, 0, 1);
  }

 private:
  math::Vector2dF scale_;
  math::Vector2dF translate_;
};

// Helper class to manage the pushing and popping of rectangular bounding
// boxes.  This manages intersecting newly pushed bounding boxes with the
// existing stack of bounding boxes.
class BoundsStack {
 public:
  // Helper class to scope a pair of Push()/Pop() calls.
  class ScopedPush {
   public:
    ScopedPush(BoundsStack* stack, const math::Rect& bounds) : stack_(stack) {
      stack_->Push(bounds);
    }
    ~ScopedPush() { stack_->Pop(); }

   private:
    BoundsStack* stack_;
  };

  BoundsStack(SbBlitterContext context, const math::Rect& initial_bounds);
  void Push(const math::Rect& bounds);
  void Pop();

  const math::Rect& Top() const { return bounds_.top(); }

  // Updates the SbBlitterContext object with the current top of the stack.
  void UpdateContext();

 private:
  SbBlitterContext context_;
  std::stack<math::Rect> bounds_;
};

// The full render state used by the Blitter API RenderTreeNodeVisitor.
// Packages together a render target, current transform and clip stack.
struct RenderState {
  RenderState(SbBlitterRenderTarget render_target, const Transform& transform,
              const BoundsStack& bounds_stack)
      : render_target(render_target),
        transform(transform),
        bounds_stack(bounds_stack),
        opacity(1.0f)
#if defined(ENABLE_DEBUGGER)
        ,
        highlight_software_draws(false)
#endif  // defined(ENABLE_DEBUGGER)
  {
  }

  SbBlitterRenderTarget render_target;
  Transform transform;
  BoundsStack bounds_stack;
  float opacity;

#if defined(ENABLE_DEBUGGER)
  // If true, all software rasterization results are replaced with a green
  // fill rectangle.  Thus, if the software cache is used, one will see a flash
  // of green every time something is registered with the cache.
  bool highlight_software_draws;
#endif  // defined(ENABLE_DEBUGGER)
};

}  // namespace blitter
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_API_VERSION < 12 && SB_HAS(BLITTER)

#endif  // COBALT_RENDERER_RASTERIZER_BLITTER_RENDER_STATE_H_
