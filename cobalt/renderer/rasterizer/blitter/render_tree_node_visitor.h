/*
 * Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDERER_RASTERIZER_BLITTER_RENDER_TREE_NODE_VISITOR_H_
#define COBALT_RENDERER_RASTERIZER_BLITTER_RENDER_TREE_NODE_VISITOR_H_

#include <stack>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/optional.h"
#include "cobalt/math/rect.h"
#include "cobalt/render_tree/composition_node.h"
#include "cobalt/render_tree/filter_node.h"
#include "cobalt/render_tree/image_node.h"
#include "cobalt/render_tree/matrix_transform_node.h"
#include "cobalt/render_tree/node_visitor.h"
#include "cobalt/render_tree/punch_through_video_node.h"
#include "cobalt/render_tree/rect_node.h"
#include "cobalt/render_tree/rect_shadow_node.h"
#include "cobalt/render_tree/text_node.h"
#include "cobalt/renderer/rasterizer/skia/software_rasterizer.h"
#include "starboard/blitter.h"

#if SB_HAS(BLITTER)

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace blitter {

// A Starboard Blitter API render tree node visitor.  Will use the Blitter API
// to rasterize render tree nodes.  If a render tree node expresses a visual
// element that the Blitter API is not capable of rendering, we will fallback
// to software rendering.
class RenderTreeNodeVisitor : public render_tree::NodeVisitor {
 public:
  RenderTreeNodeVisitor(SbBlitterDevice device, SbBlitterContext context,
                        SbBlitterRenderTarget render_target,
                        const math::Size& bounds,
                        skia::SkiaSoftwareRasterizer* software_rasterizer);

  void Visit(render_tree::CompositionNode* composition_node) OVERRIDE;
  void Visit(render_tree::FilterNode* filter_node) OVERRIDE;
  void Visit(render_tree::ImageNode* image_node) OVERRIDE;
  void Visit(render_tree::MatrixTransformNode* matrix_transform_node) OVERRIDE;
  void Visit(
      render_tree::PunchThroughVideoNode* punch_through_video_node) OVERRIDE;
  void Visit(render_tree::RectNode* rect_node) OVERRIDE;
  void Visit(render_tree::RectShadowNode* rect_shadow_node) OVERRIDE;
  void Visit(render_tree::TextNode* text_node) OVERRIDE;

 private:
  // Keeps track of the current scale/offset transforms.  Note that while the
  // render tree supports general affine transformations, the Starboard Blitter
  // API only supports scale and translations, so we maintain only those.  If
  // rotations are used, we will fallback to software rendering.
  struct Transform {
    Transform() : scale(1.0f, 1.0f), translate(0.0f, 0.0f) {}

    Transform(const math::Vector2dF& scale, const math::Vector2dF& translate)
        : scale(scale), translate(translate) {}

    void ApplyOffset(const math::Vector2dF& offset) {
      translate +=
          math::Vector2dF(scale.x() * offset.x(), scale.y() * offset.y());
    }
    void ApplyScale(const math::Vector2dF& scale_mult) {
      scale.Scale(scale_mult.x(), scale_mult.y());
    }

    math::RectF TransformRect(const math::RectF& rect) const;

    math::Vector2dF scale;
    math::Vector2dF translate;
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

  // The following helper function returns bounding box/transform information
  // used to setup a sub-render, whether that sub render is happening through
  // a Skia software renderer, or the Blitter API.
  struct SubRenderBounds {
    // Dictates where on the screen the final subrendered rectangle should be
    // blitted, in absolute coordinates (i.e. |transform_| has already been
    // taken into account).
    math::Rect output_bounds;
    // Specifies the transform which should be applied while performing the sub
    // render.
    Transform sub_transform;
  };
  SubRenderBounds GetSubRenderBounds(render_tree::Node* node);

  // Can be called with any render tree node in order to invoke the Skia
  // software renderer to render to an offscreen surface which is then applied
  // to the render target via a Blitter API blit.
  void RenderWithSoftwareRenderer(render_tree::Node* node);

  // Uses a Blitter API sub-visitor to render the provided render tree to a
  // offscreen SbBlitterSurface which is then returned.
  SbBlitterSurface RenderToOffscreenSurface(render_tree::Node* node);

  // We maintain an instance of a software skia rasterizer which is used to
  // render anything that we cannot render via the Blitter API directly.
  skia::SkiaSoftwareRasterizer* software_rasterizer_;

  SbBlitterDevice device_;
  SbBlitterContext context_;
  SbBlitterRenderTarget render_target_;

  BoundsStack bounds_stack_;

  // The current transform state.
  Transform transform_;

  DISALLOW_COPY_AND_ASSIGN(RenderTreeNodeVisitor);
};

}  // namespace blitter
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // #if SB_HAS(BLITTER)

#endif  // COBALT_RENDERER_RASTERIZER_BLITTER_RENDER_TREE_NODE_VISITOR_H_
