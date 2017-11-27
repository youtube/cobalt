// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDERER_RASTERIZER_EGL_RENDER_TREE_NODE_VISITOR_H_
#define COBALT_RENDERER_RASTERIZER_EGL_RENDER_TREE_NODE_VISITOR_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "cobalt/math/matrix3_f.h"
#include "cobalt/math/rect_f.h"
#include "cobalt/render_tree/animations/animate_node.h"
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
#include "cobalt/renderer/backend/egl/texture.h"
#include "cobalt/renderer/backend/render_target.h"
#include "cobalt/renderer/rasterizer/egl/draw_object.h"
#include "cobalt/renderer/rasterizer/egl/draw_object_manager.h"
#include "cobalt/renderer/rasterizer/egl/graphics_state.h"
#include "cobalt/renderer/rasterizer/egl/offscreen_target_manager.h"
#include "third_party/skia/include/core/SkCanvas.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

// A render tree node visitor that translates a given render tree into
// DrawObjects which must then be processed using calls to ExecuteDraw.
class RenderTreeNodeVisitor : public render_tree::NodeVisitor {
 public:
  enum FallbackRasterizeFlags {
    kFallbackShouldClear = 1 << 0,
    kFallbackShouldFlush = 1 << 1,
  };
  typedef base::Callback<void(
      const scoped_refptr<render_tree::Node>& render_tree,
      SkCanvas* fallback_render_target, const math::Matrix3F& transform,
      const math::RectF& scissor, float opacity, uint32_t rasterize_flags)>
      FallbackRasterizeFunction;

  RenderTreeNodeVisitor(GraphicsState* graphics_state,
                        DrawObjectManager* draw_object_manager,
                        OffscreenTargetManager* offscreen_target_manager,
                        const FallbackRasterizeFunction& fallback_rasterize,
                        SkCanvas* fallback_render_target,
                        backend::RenderTarget* render_target,
                        const math::Rect& content_rect);

  void Visit(render_tree::animations::AnimateNode* /* animate */) override {
    NOTREACHED();
  }
  void Visit(render_tree::CompositionNode* composition_node) override;
  void Visit(render_tree::MatrixTransform3DNode* transform_3d_node) override;
  void Visit(render_tree::MatrixTransformNode* transform_node) override;
  void Visit(render_tree::FilterNode* filter_node) override;
  void Visit(render_tree::ImageNode* image_node) override;
  void Visit(render_tree::PunchThroughVideoNode* video_node) override;
  void Visit(render_tree::RectNode* rect_node) override;
  void Visit(render_tree::RectShadowNode* shadow_node) override;
  void Visit(render_tree::TextNode* text_node) override;

 private:
  void GetScratchTexture(scoped_refptr<render_tree::Node> node, float size,
                         DrawObject::TextureInfo* out_texture_info);
  void GetCachedTarget(scoped_refptr<render_tree::Node> node,
                       bool* out_content_cached,
                       OffscreenTargetManager::TargetInfo* out_target_info,
                       math::RectF* out_content_rect);

  void FallbackRasterize(scoped_refptr<render_tree::Node> node);
  void FallbackRasterize(scoped_refptr<render_tree::Node> node,
                         const OffscreenTargetManager::TargetInfo& target_info,
                         const math::RectF& content_rect);

  void OffscreenRasterize(scoped_refptr<render_tree::Node> node,
                          const backend::TextureEGL** out_texture,
                          math::Matrix3F* out_texcoord_transform,
                          math::RectF* out_content_rect);

  bool IsVisible(const math::RectF& bounds);
  void AddDraw(scoped_ptr<DrawObject> object, const math::RectF& local_bounds,
               DrawObjectManager::BlendType blend_type);
  void AddExternalDraw(scoped_ptr<DrawObject> object,
                       const math::RectF& world_bounds, base::TypeId draw_type);

  GraphicsState* graphics_state_;
  DrawObjectManager* draw_object_manager_;
  OffscreenTargetManager* offscreen_target_manager_;
  FallbackRasterizeFunction fallback_rasterize_;

  DrawObject::BaseState draw_state_;
  SkCanvas* fallback_render_target_;
  backend::RenderTarget* render_target_;
  backend::RenderTarget* onscreen_render_target_;

  uint32_t last_draw_id_;
};

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_EGL_RENDER_TREE_NODE_VISITOR_H_
