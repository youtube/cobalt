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

#include "cobalt/renderer/rasterizer/egl/render_tree_node_visitor.h"

#include <algorithm>

#include "base/debug/trace_event.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/base/type_id.h"
#include "cobalt/math/matrix3_f.h"
#include "cobalt/renderer/rasterizer/egl/draw_poly_color.h"
#include "cobalt/renderer/rasterizer/egl/draw_rect_texture.h"
#include "cobalt/renderer/rasterizer/skia/hardware_image.h"
#include "cobalt/renderer/rasterizer/skia/image.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

namespace {

math::Rect RoundRectFToInt(const math::RectF& input) {
  int left = static_cast<int>(input.x() + 0.5f);
  int right = static_cast<int>(input.right() + 0.5f);
  int top = static_cast<int>(input.y() + 0.5f);
  int bottom = static_cast<int>(input.bottom() + 0.5f);
  return math::Rect(left, top, right - left, bottom - top);
}

}  // namespace

RenderTreeNodeVisitor::RenderTreeNodeVisitor(GraphicsState* graphics_state,
    ShaderProgramManager* shader_program_manager,
    const FallbackRasterizeFunction* fallback_rasterize)
    : graphics_state_(graphics_state),
      shader_program_manager_(shader_program_manager),
      fallback_rasterize_(fallback_rasterize) {
  // Let the first draw object render in front of the clear depth.
  draw_state_.depth = graphics_state_->NextClosestDepth(draw_state_.depth);

  draw_state_.scissor.Intersect(graphics_state->GetScissor());
}

void RenderTreeNodeVisitor::Visit(
    render_tree::CompositionNode* composition_node) {
  const render_tree::CompositionNode::Builder& data = composition_node->data();
  draw_state_.transform(0, 2) += data.offset().x();
  draw_state_.transform(1, 2) += data.offset().y();
  const render_tree::CompositionNode::Children& children =
      data.children();
  for (render_tree::CompositionNode::Children::const_iterator iter =
       children.begin(); iter != children.end(); ++iter) {
    (*iter)->Accept(this);
  }
  draw_state_.transform(0, 2) -= data.offset().x();
  draw_state_.transform(1, 2) -= data.offset().y();
}

void RenderTreeNodeVisitor::Visit(
    render_tree::MatrixTransformNode* transform_node) {
  const render_tree::MatrixTransformNode::Builder& data =
      transform_node->data();
  math::Matrix3F old_transform = draw_state_.transform;
  draw_state_.transform = draw_state_.transform *
      data.transform;
  data.source->Accept(this);
  draw_state_.transform = old_transform;
}

void RenderTreeNodeVisitor::Visit(render_tree::FilterNode* filter_node) {
  const render_tree::FilterNode::Builder& data = filter_node->data();

  // If this is only a viewport filter w/o rounded edges, and the current
  // transform matrix keeps the filter as an orthogonal rect, then collapse
  // the node.
  if (data.viewport_filter &&
      !data.viewport_filter->has_rounded_corners() &&
      !data.opacity_filter &&
      !data.blur_filter &&
      !data.map_to_mesh_filter) {
    const math::Matrix3F& transform = draw_state_.transform;
    if (transform(2, 0) == 0 && transform(2, 1) == 0 && transform(2, 2) == 1 &&
        transform(0, 1) == 0 && transform(1, 0) == 0) {
      // Transform local viewport to world viewport.
      const math::RectF& filter_viewport = data.viewport_filter->viewport();
      math::RectF transformed_viewport(
          filter_viewport.x() * transform(0, 0) + transform(0, 2),
          filter_viewport.y() * transform(1, 1) + transform(1, 2),
          filter_viewport.width() * transform(0, 0),
          filter_viewport.height() * transform(1, 1));
      // Ensure transformed viewport data is sane (in case global transform
      // flipped any axis).
      if (transformed_viewport.width() < 0) {
        transformed_viewport.set_x(transformed_viewport.right());
        transformed_viewport.set_width(-transformed_viewport.width());
      }
      if (transformed_viewport.height() < 0) {
        transformed_viewport.set_y(transformed_viewport.bottom());
        transformed_viewport.set_height(-transformed_viewport.height());
      }
      // Combine the new viewport filter with existing viewport filter.
      math::Rect old_scissor = draw_state_.scissor;
      draw_state_.scissor.Intersect(RoundRectFToInt(transformed_viewport));
      if (!draw_state_.scissor.IsEmpty()) {
        data.source->Accept(this);
      }
      draw_state_.scissor = old_scissor;
      return;
    }
  }

  NOTIMPLEMENTED();
}

void RenderTreeNodeVisitor::Visit(render_tree::ImageNode* image_node) {
  const render_tree::ImageNode::Builder& data = image_node->data();

  // The image node may contain nothing. For example, when it represents a video
  // element before any frame is decoded.
  if (!data.source) {
    return;
  }

  if (!IsVisible(image_node->GetBounds())) {
    return;
  }

  skia::Image* skia_image =
      base::polymorphic_downcast<skia::Image*>(data.source.get());

  // Ensure any required backend processing is done to create the necessary
  // GPU resource.
  skia_image->EnsureInitialized();

  // Different shaders are used depending on whether the image has a single
  // plane or multiple planes.
  if (skia_image->GetTypeId() == base::GetTypeId<skia::SinglePlaneImage>()) {
    math::Matrix3F texcoord_transform =
        data.local_transform.IsIdentity() ?
            math::Matrix3F::Identity() :
            data.local_transform.Inverse();
    skia::HardwareFrontendImage* hardware_image =
        base::polymorphic_downcast<skia::HardwareFrontendImage*>(skia_image);

    if (hardware_image->IsOpaque() && draw_state_.opacity == 1.0f) {
      scoped_ptr<DrawObject> draw(new DrawRectTexture(graphics_state_,
          draw_state_, data.destination_rect,
          hardware_image->GetTextureEGL(), texcoord_transform));
      AddDrawObject(draw.Pass(), kDrawRectTexture);
    } else {
      NOTIMPLEMENTED();
    }
  } else if (skia_image->GetTypeId() ==
             base::GetTypeId<skia::MultiPlaneImage>()) {
    NOTIMPLEMENTED();
  } else {
    NOTREACHED();
  }
}

void RenderTreeNodeVisitor::Visit(
    render_tree::PunchThroughVideoNode* video_node) {
  if (!IsVisible(video_node->GetBounds())) {
    return;
  }

  NOTIMPLEMENTED();
}

void RenderTreeNodeVisitor::Visit(render_tree::RectNode* rect_node) {
  if (!IsVisible(rect_node->GetBounds())) {
    return;
  }

  const render_tree::RectNode::Builder& data = rect_node->data();

  const scoped_ptr<render_tree::Brush>& brush = data.background_brush;
  math::RectF content_rect(data.rect);
  if (data.border) {
    content_rect.Inset(data.border->left.width,
                       data.border->top.width,
                       data.border->right.width,
                       data.border->bottom.width);
  }

  if (data.rounded_corners ||
      (brush && brush->GetTypeId() !=
          base::GetTypeId<render_tree::SolidColorBrush>())) {
    NOTIMPLEMENTED();
  } else {
    // Handle drawing the content.
    if (brush) {
      render_tree::SolidColorBrush* solid_brush =
          base::polymorphic_downcast<render_tree::SolidColorBrush*>
              (brush.get());
      scoped_ptr<DrawObject> draw(new DrawPolyColor(graphics_state_,
          draw_state_, content_rect, solid_brush->color()));
      if (draw_state_.opacity * solid_brush->color().a() == 1.0f) {
        AddDrawObject(draw.Pass(), kDrawPolyColor);
      } else {
        AddDrawObject(draw.Pass(), kDrawTransparent);
      }
    }
  }
}

void RenderTreeNodeVisitor::Visit(render_tree::RectShadowNode* shadow_node) {
  if (!IsVisible(shadow_node->GetBounds())) {
    return;
  }

  NOTIMPLEMENTED();
}

void RenderTreeNodeVisitor::Visit(render_tree::TextNode* text_node) {
  if (!IsVisible(text_node->GetBounds())) {
    return;
  }

  NOTIMPLEMENTED();
}

void RenderTreeNodeVisitor::ExecuteDraw(DrawObject::ExecutionStage stage) {
  for (int type = 0; type < kDrawCount; ++type) {
    if (type == kDrawTransparent) {
      graphics_state_->EnableBlend();
      graphics_state_->DisableDepthWrite();
    } else {
      graphics_state_->DisableBlend();
      graphics_state_->EnableDepthWrite();
    }

    for (size_t index = 0; index < draw_objects_[type].size(); ++index) {
      draw_objects_[type][index]->Execute(graphics_state_,
                                          shader_program_manager_, stage);
    }
  }
}

bool RenderTreeNodeVisitor::IsVisible(const math::RectF& bounds) {
  math::RectF intersection = IntersectRects(
      draw_state_.transform.MapRect(bounds), draw_state_.scissor);
  return !intersection.IsEmpty();
}

void RenderTreeNodeVisitor::AddDrawObject(scoped_ptr<DrawObject> object,
                                          DrawType type) {
  draw_objects_[type].push_back(object.release());
  draw_state_.depth = graphics_state_->NextClosestDepth(draw_state_.depth);
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
