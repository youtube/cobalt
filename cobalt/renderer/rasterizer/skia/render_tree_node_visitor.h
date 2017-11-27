// Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_RENDER_TREE_NODE_VISITOR_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_RENDER_TREE_NODE_VISITOR_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/optional.h"
#include "cobalt/render_tree/composition_node.h"
#include "cobalt/render_tree/filter_node.h"
#include "cobalt/render_tree/image_node.h"
#include "cobalt/render_tree/matrix_transform_3d_node.h"
#include "cobalt/render_tree/matrix_transform_node.h"
#include "cobalt/render_tree/node_visitor.h"
#include "cobalt/render_tree/punch_through_video_node.h"
#include "cobalt/render_tree/rect_node.h"
#include "cobalt/render_tree/text_node.h"
#include "cobalt/renderer/rasterizer/common/surface_cache.h"
#include "cobalt/renderer/rasterizer/skia/render_tree_node_visitor_draw_state.h"
#include "cobalt/renderer/rasterizer/skia/surface_cache_delegate.h"
#include "third_party/skia/include/core/SkCanvas.h"

namespace cobalt {

namespace render_tree {
namespace animations {
class AnimateNode;
}  // namespace animations
}  // namespace render_tree

namespace renderer {
namespace rasterizer {
namespace skia {

class RenderTreeNodeVisitor : public render_tree::NodeVisitor {
 public:
  // This callback may be called by the visitor in order to obtain a SkSurface
  // from which both a SkCanvas can be obtained (for rendering into) and then
  // a SkImage can be obtained which can be passed in to another SkCanvas'
  // drawImage function.  The surface returned is guaranteed to have been
  // cleared to ARGB(0,0,0,0).
  class ScratchSurface {
   public:
    virtual ~ScratchSurface() {}
    virtual SkSurface* GetSurface() = 0;
  };
  typedef base::Callback<scoped_ptr<ScratchSurface>(const math::Size&)>
      CreateScratchSurfaceFunction;

  typedef base::Callback<void(const render_tree::ImageNode* image_node,
                              RenderTreeNodeVisitorDrawState* draw_state)>
      RenderImageFallbackFunction;

  typedef base::Callback<void(const render_tree::ImageNode* image_node,
                              const render_tree::MapToMeshFilter& mesh_filter,
                              RenderTreeNodeVisitorDrawState* draw_state)>
      RenderImageWithMeshFallbackFunction;

  enum Type {
    kType_Normal,
    kType_SubVisitor,
  };

  // The create_scratch_surface_function functor object will be saved within
  // RenderTreeNodeVisitor, so it must outlive the RenderTreeNodeVisitor
  // object.  If |is_sub_visitor| is set to true, errors will be supported for
  // certain operations such as punch out alpha textures, as it is unfortunately
  // difficult to implement them when rendering to a sub-canvas.  If
  // |render_image_fallback_function| is specified, it will be invoked whenever
  // standard Skia processing of the image is not possible, which usually is
  // when the image is backed by a SbDecodeTarget that requires special
  // consideration.  |render_image_with_mesh| must be specified
  // in order to support the map-to-mesh filter since Skia is unable to draw
  // 3D meshes natively.
  RenderTreeNodeVisitor(
      SkCanvas* render_target,
      const CreateScratchSurfaceFunction* create_scratch_surface_function,
      const base::Closure& reset_skia_context_function,
      const RenderImageFallbackFunction& render_image_fallback_function,
      const RenderImageWithMeshFallbackFunction& render_image_with_mesh,
      SurfaceCacheDelegate* surface_cache_delegate,
      common::SurfaceCache* surface_cache, Type visitor_type = kType_Normal);

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
  // Helper function to render the filter's source to an offscreen surface and
  // then apply the filter to the offscreen surface.
  void RenderFilterViaOffscreenSurface(
      const render_tree::FilterNode::Builder& filter_node);

  RenderTreeNodeVisitorDrawState draw_state_;
  const CreateScratchSurfaceFunction* create_scratch_surface_function_;
  Type visitor_type_;

  SurfaceCacheDelegate* surface_cache_delegate_;
  common::SurfaceCache* surface_cache_;
  base::optional<SurfaceCacheDelegate::ScopedContext>
      surface_cache_scoped_context_;

  base::Closure reset_skia_context_function_;

  RenderImageFallbackFunction render_image_fallback_function_;
  RenderImageWithMeshFallbackFunction render_image_with_mesh_function_;

  DISALLOW_COPY_AND_ASSIGN(RenderTreeNodeVisitor);
};

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_RENDER_TREE_NODE_VISITOR_H_
