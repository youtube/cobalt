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
#include "cobalt/math/rect_f.h"
#include "cobalt/math/size.h"
#include "cobalt/render_tree/animations/animate_node.h"
#include "cobalt/render_tree/composition_node.h"
#include "cobalt/render_tree/filter_node.h"
#include "cobalt/render_tree/image_node.h"
#include "cobalt/render_tree/matrix_transform_node.h"
#include "cobalt/render_tree/node_visitor.h"
#include "cobalt/render_tree/punch_through_video_node.h"
#include "cobalt/render_tree/rect_node.h"
#include "cobalt/render_tree/rect_shadow_node.h"
#include "cobalt/render_tree/text_node.h"
#include "cobalt/renderer/backend/egl/texture.h"
#include "cobalt/renderer/rasterizer/egl/draw_object.h"
#include "cobalt/renderer/rasterizer/egl/graphics_state.h"
#include "cobalt/renderer/rasterizer/egl/shader_program_manager.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

// A render tree node visitor that translates a given render tree into
// DrawObjects which must then be processed using calls to ExecuteDraw.
class RenderTreeNodeVisitor : public render_tree::NodeVisitor {
 public:
  typedef base::Callback<backend::TextureEGL*(
      const scoped_refptr<render_tree::Node>& render_tree,
      const math::Size& viewport_size)>
      FallbackRasterizeFunction;

  RenderTreeNodeVisitor(GraphicsState* graphics_state,
                        ShaderProgramManager* shader_program_manager,
                        const FallbackRasterizeFunction* fallback_rasterize);

  void Visit(render_tree::animations::AnimateNode* /* animate */) OVERRIDE {
    NOTREACHED();
  }
  void Visit(render_tree::CompositionNode* composition_node) OVERRIDE;
  void Visit(render_tree::MatrixTransformNode* transform_node) OVERRIDE;
  void Visit(render_tree::FilterNode* filter_node) OVERRIDE;
  void Visit(render_tree::ImageNode* image_node) OVERRIDE;
  void Visit(render_tree::PunchThroughVideoNode* video_node) OVERRIDE;
  void Visit(render_tree::RectNode* rect_node) OVERRIDE;
  void Visit(render_tree::RectShadowNode* shadow_node) OVERRIDE;
  void Visit(render_tree::TextNode* text_node) OVERRIDE;

  void ExecuteDraw(DrawObject::ExecutionStage stage);

 private:
  enum DrawType {
    kDrawRectTexture = 0,
    kDrawPolyColor,
    kDrawTransparent,
    kDrawCount,
  };

  bool IsVisible(const math::RectF& bounds);
  void AddDrawObject(scoped_ptr<DrawObject> object, DrawType type);

  GraphicsState* graphics_state_;
  ShaderProgramManager* shader_program_manager_;
  const FallbackRasterizeFunction* fallback_rasterize_;

  DrawObject::BaseState draw_state_;
  ScopedVector<DrawObject> draw_objects_[kDrawCount];
};

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_EGL_RENDER_TREE_NODE_VISITOR_H_
