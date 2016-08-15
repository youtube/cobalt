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
#include "cobalt/renderer/rasterizer/blitter/render_state.h"
#include "cobalt/renderer/rasterizer/blitter/surface_cache_delegate.h"
#include "cobalt/renderer/rasterizer/common/surface_cache.h"
#include "cobalt/renderer/rasterizer/skia/software_rasterizer.h"

#include "starboard/blitter.h"

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
                        skia::SoftwareRasterizer* software_rasterizer,
                        SurfaceCacheDelegate* surface_cache_delegate,
                        common::SurfaceCache* surface_cache);

  void Visit(render_tree::animations::AnimateNode* animate_node) OVERRIDE {
    NOTREACHED();
  }
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
    math::Point position;
    SbBlitterSurface surface;
  };
  OffscreenRender RenderToOffscreenSurface(render_tree::Node* node);

  // We maintain an instance of a software skia rasterizer which is used to
  // render anything that we cannot render via the Blitter API directly.
  skia::SoftwareRasterizer* software_rasterizer_;

  SbBlitterDevice device_;
  SbBlitterContext context_;

  // Keeps track of our current render target, transform and clip stack.
  RenderState render_state_;

  SurfaceCacheDelegate* surface_cache_delegate_;
  common::SurfaceCache* surface_cache_;
  base::optional<SurfaceCacheDelegate::ScopedContext>
      surface_cache_scoped_context_;

  DISALLOW_COPY_AND_ASSIGN(RenderTreeNodeVisitor);
};

}  // namespace blitter
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // #if SB_HAS(BLITTER)

#endif  // COBALT_RENDERER_RASTERIZER_BLITTER_RENDER_TREE_NODE_VISITOR_H_
