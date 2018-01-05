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
#include "cobalt/render_tree/matrix_transform_3d_node.h"
#include "cobalt/render_tree/matrix_transform_node.h"
#include "cobalt/render_tree/node_visitor.h"
#include "cobalt/render_tree/punch_through_video_node.h"
#include "cobalt/render_tree/rect_node.h"
#include "cobalt/render_tree/rect_shadow_node.h"
#include "cobalt/render_tree/text_node.h"
#include "cobalt/renderer/rasterizer/blitter/cached_software_rasterizer.h"
#include "cobalt/renderer/rasterizer/blitter/linear_gradient_cache.h"
#include "cobalt/renderer/rasterizer/blitter/render_state.h"
#include "cobalt/renderer/rasterizer/blitter/scratch_surface_cache.h"

#include "starboard/blitter.h"
#include "third_party/skia/include/core/SkImageInfo.h"

#if SB_HAS(BLITTER)

namespace cobalt {

namespace render_tree {
namespace animations {
class AnimateNode;
}  // namespace animations
}  // namespace render_tree

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
                        const RenderState& render_state,
                        ScratchSurfaceCache* scratch_surface_cache,
                        CachedSoftwareRasterizer* software_surface_cache,
                        LinearGradientCache* linear_gradient_cache);

  void Visit(render_tree::animations::AnimateNode* animate_node) override {
    NOTREACHED();
  }
  void Visit(render_tree::CompositionNode* composition_node) override;
  void Visit(render_tree::FilterNode* filter_node) override;
  void Visit(render_tree::ImageNode* image_node) override;
  void Visit(
      render_tree::MatrixTransform3DNode* matrix_transform_3d_node) override;
  void Visit(render_tree::MatrixTransformNode* matrix_transform_node) override;
  void Visit(
      render_tree::PunchThroughVideoNode* punch_through_video_node) override;
  void Visit(render_tree::RectNode* rect_node) override;
  void Visit(render_tree::RectShadowNode* rect_shadow_node) override;
  void Visit(render_tree::TextNode* text_node) override;

 private:
  // Can be called with any render tree node in order to invoke the Skia
  // software renderer to render to an offscreen surface which is then applied
  // to the render target via a Blitter API blit.
  void RenderWithSoftwareRenderer(render_tree::Node* node);

  RenderState* SetRenderState(const RenderState& render_state) {
    render_state_ = render_state;
    return &render_state_;
  }

  // Uses a Blitter API sub-visitor to render the provided render tree to a
  // offscreen SbBlitterSurface which is then returned.
  struct OffscreenRender {
    math::RectF destination_rect;
    scoped_ptr<CachedScratchSurface> scratch_surface;
  };
  scoped_ptr<OffscreenRender> RenderToOffscreenSurface(render_tree::Node* node);

  SbBlitterDevice device_;
  SbBlitterContext context_;

  // Keeps track of our current render target, transform and clip stack.
  RenderState render_state_;

  // Manager for scratch surfaces used for intermediate rendering during render
  // tree traversal.
  ScratchSurfaceCache* scratch_surface_cache_;

  // We fallback to software rasterization in order to render anything that we
  // cannot render via the Blitter API directly.  We cache the results.
  CachedSoftwareRasterizer* software_surface_cache_;
  LinearGradientCache* linear_gradient_cache_;

  DISALLOW_COPY_AND_ASSIGN(RenderTreeNodeVisitor);
};

}  // namespace blitter
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // #if SB_HAS(BLITTER)

#endif  // COBALT_RENDERER_RASTERIZER_BLITTER_RENDER_TREE_NODE_VISITOR_H_
