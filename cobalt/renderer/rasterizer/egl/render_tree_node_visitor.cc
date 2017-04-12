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
#include <cmath>

#include "base/debug/trace_event.h"
#include "base/optional.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/base/type_id.h"
#include "cobalt/math/matrix3_f.h"
#include "cobalt/renderer/rasterizer/common/utils.h"
#include "cobalt/renderer/rasterizer/egl/draw_poly_color.h"
#include "cobalt/renderer/rasterizer/egl/draw_rect_color_texture.h"
#include "cobalt/renderer/rasterizer/egl/draw_rect_shadow_blur.h"
#include "cobalt/renderer/rasterizer/egl/draw_rect_shadow_spread.h"
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

bool IsOnlyScaleAndTranslate(const math::Matrix3F& matrix) {
  return matrix(2, 0) == 0 && matrix(2, 1) == 0 && matrix(2, 2) == 1 &&
         matrix(0, 1) == 0 && matrix(1, 0) == 0;
}

}  // namespace

RenderTreeNodeVisitor::RenderTreeNodeVisitor(GraphicsState* graphics_state,
    DrawObjectManager* draw_object_manager,
    const FallbackRasterizeFunction* fallback_rasterize)
    : graphics_state_(graphics_state),
      draw_object_manager_(draw_object_manager),
      fallback_rasterize_(fallback_rasterize) {
  // Let the first draw object render in front of the clear depth.
  draw_state_.depth = GraphicsState::NextClosestDepth(draw_state_.depth);

  draw_state_.scissor.Intersect(graphics_state->GetScissor());
  viewport_size_ = graphics_state->GetViewport().size();
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
    render_tree::MatrixTransform3DNode* transform_3d_node) {
  // TODO: Ignore the 3D transform matrix for now.
  transform_3d_node->data().source->Accept(this);
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
    if (IsOnlyScaleAndTranslate(transform)) {
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

  // Handle opacity-only filter.
  if (data.opacity_filter &&
      !data.viewport_filter &&
      !data.blur_filter &&
      !data.map_to_mesh_filter &&
      common::utils::NodeCanRenderWithOpacity(data.source)) {
    float old_opacity = draw_state_.opacity;
    draw_state_.opacity *= data.opacity_filter->opacity();
    data.source->Accept(this);
    draw_state_.opacity = old_opacity;
    return;
  }

  // Use the fallback rasterizer to handle everything else.
  FallbackRasterize(filter_node, DrawObjectManager::kOffscreenSkiaFilter);
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
  bool clamp_texcoords = false;
  bool is_opaque = skia_image->IsOpaque() && draw_state_.opacity == 1.0f;

  // Ensure any required backend processing is done to create the necessary
  // GPU resource.
  skia_image->EnsureInitialized();

  // Calculate matrix to transform texture coordinates according to the local
  // transform.
  math::Matrix3F texcoord_transform(math::Matrix3F::Identity());
  if (IsOnlyScaleAndTranslate(data.local_transform)) {
    texcoord_transform(0, 0) = data.local_transform(0, 0) != 0 ?
        1.0f / data.local_transform(0, 0) : 0;
    texcoord_transform(1, 1) = data.local_transform(1, 1) != 0 ?
        1.0f / data.local_transform(1, 1) : 0;
    texcoord_transform(0, 2) = -texcoord_transform(0, 0) *
                               data.local_transform(0, 2);
    texcoord_transform(1, 2) = -texcoord_transform(1, 1) *
                               data.local_transform(1, 2);
    if (texcoord_transform(0, 0) < 1.0f || texcoord_transform(1, 1) < 1.0f) {
      // Edges may interpolate with texels outside the designated region.
      // Use a fragment shader that clamps the texture coordinates to prevent
      // that from happening.
      clamp_texcoords = true;
    }
  } else {
    texcoord_transform = data.local_transform.Inverse();
  }

  // Different shaders are used depending on whether the image has a single
  // plane or multiple planes.
  scoped_ptr<DrawObject> draw;
  DrawObjectManager::OnscreenType onscreen_type;

  if (skia_image->GetTypeId() == base::GetTypeId<skia::SinglePlaneImage>()) {
    skia::HardwareFrontendImage* hardware_image =
        base::polymorphic_downcast<skia::HardwareFrontendImage*>(skia_image);
    if (clamp_texcoords || !is_opaque) {
      onscreen_type = DrawObjectManager::kOnscreenRectColorTexture;
      draw.reset(new DrawRectColorTexture(graphics_state_, draw_state_,
          data.destination_rect,
          render_tree::ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
          hardware_image->GetTextureEGL(), texcoord_transform,
          clamp_texcoords));
    } else {
      onscreen_type = DrawObjectManager::kOnscreenRectTexture;
      draw.reset(new DrawRectTexture(graphics_state_, draw_state_,
          data.destination_rect, hardware_image->GetTextureEGL(),
          texcoord_transform));
    }
  } else if (skia_image->GetTypeId() ==
             base::GetTypeId<skia::MultiPlaneImage>()) {
    FallbackRasterize(image_node,
                      DrawObjectManager::kOffscreenSkiaMultiPlaneImage);
    return;
  } else {
    NOTREACHED();
    return;
  }

  if (is_opaque) {
    AddOpaqueDraw(draw.Pass(), onscreen_type,
        DrawObjectManager::kOffscreenNone);
  } else {
    AddTransparentDraw(draw.Pass(), onscreen_type,
        DrawObjectManager::kOffscreenNone, image_node->GetBounds());
  }
}

void RenderTreeNodeVisitor::Visit(
    render_tree::PunchThroughVideoNode* video_node) {
  if (!IsVisible(video_node->GetBounds())) {
    return;
  }

  const render_tree::PunchThroughVideoNode::Builder& data = video_node->data();
  math::RectF mapped_rect = draw_state_.transform.MapRect(data.rect);
  data.set_bounds_cb.Run(
      math::Rect(static_cast<int>(mapped_rect.x()),
                 static_cast<int>(mapped_rect.y()),
                 static_cast<int>(mapped_rect.width()),
                 static_cast<int>(mapped_rect.height())));

  scoped_ptr<DrawObject> draw(new DrawPolyColor(graphics_state_,
      draw_state_, data.rect, render_tree::ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f)));
  AddOpaqueDraw(draw.Pass(), DrawObjectManager::kOnscreenPolyColor,
      DrawObjectManager::kOffscreenNone);
}

void RenderTreeNodeVisitor::Visit(render_tree::RectNode* rect_node) {
  if (!IsVisible(rect_node->GetBounds())) {
    return;
  }

  const render_tree::RectNode::Builder& data = rect_node->data();
  const scoped_ptr<render_tree::Brush>& brush = data.background_brush;

  // Only solid color brushes are supported at this time.
  const bool brush_supported = !brush || brush->GetTypeId() ==
      base::GetTypeId<render_tree::SolidColorBrush>();

  // Borders are not supported natively by this rasterizer at this time. The
  // difficulty lies in getting anti-aliased borders and minimizing state
  // switches (due to anti-aliased borders requiring transparency). However,
  // by using the fallback rasterizer, both can be accomplished -- sort to
  // minimize state switches while rendering anti-aliased borders to the
  // offscreen target, then use a single shader to render those.
  const bool border_supported = !data.border;

  if (data.rounded_corners) {
    FallbackRasterize(rect_node, DrawObjectManager::kOffscreenSkiaRectRounded);
  } else if (!brush_supported) {
    FallbackRasterize(rect_node, DrawObjectManager::kOffscreenSkiaRectBrush);
  } else if (!border_supported) {
    FallbackRasterize(rect_node, DrawObjectManager::kOffscreenSkiaRectBorder);
  } else {
    DCHECK(!data.border);
    const math::RectF& content_rect(data.rect);

    // Handle drawing the content.
    if (brush) {
      const render_tree::SolidColorBrush* solid_brush =
          base::polymorphic_downcast<const render_tree::SolidColorBrush*>
              (brush.get());
      const render_tree::ColorRGBA& brush_color(solid_brush->color());
      render_tree::ColorRGBA content_color(
          brush_color.r() * brush_color.a(),
          brush_color.g() * brush_color.a(),
          brush_color.b() * brush_color.a(),
          brush_color.a());
      scoped_ptr<DrawObject> draw(new DrawPolyColor(graphics_state_,
          draw_state_, content_rect, content_color));
      if (draw_state_.opacity * content_color.a() == 1.0f) {
        AddOpaqueDraw(draw.Pass(), DrawObjectManager::kOnscreenPolyColor,
            DrawObjectManager::kOffscreenNone);
      } else {
        AddTransparentDraw(draw.Pass(), DrawObjectManager::kOnscreenPolyColor,
            DrawObjectManager::kOffscreenNone, rect_node->GetBounds());
      }
    }
  }
}

void RenderTreeNodeVisitor::Visit(render_tree::RectShadowNode* shadow_node) {
  math::RectF node_bounds(shadow_node->GetBounds());
  if (!IsVisible(node_bounds)) {
    return;
  }

  const render_tree::RectShadowNode::Builder& data = shadow_node->data();
  if (data.rounded_corners) {
    FallbackRasterize(shadow_node, DrawObjectManager::kOffscreenSkiaShadow);
    return;
  }

  scoped_ptr<DrawObject> draw;
  render_tree::ColorRGBA shadow_color(
      data.shadow.color.r() * data.shadow.color.a(),
      data.shadow.color.g() * data.shadow.color.a(),
      data.shadow.color.b() * data.shadow.color.a(),
      data.shadow.color.a());
  DrawObjectManager::OnscreenType onscreen_type =
      DrawObjectManager::kOnscreenRectShadow;

  math::RectF spread_rect(data.rect);
  spread_rect.Offset(data.shadow.offset);
  if (data.inset) {
    // data.rect is outermost.
    // spread_rect is in the middle.
    // blur_rect is innermost.
    spread_rect.Inset(data.spread, data.spread);
    spread_rect.Intersect(data.rect);
    if (spread_rect.IsEmpty()) {
      // Spread covers the whole data.rect.
      spread_rect.set_origin(data.rect.CenterPoint());
      spread_rect.set_size(math::SizeF());
    }
    if (!spread_rect.IsEmpty() && data.shadow.blur_sigma > 0.0f) {
      math::RectF blur_rect(spread_rect);
      math::Vector2dF blur_extent(data.shadow.BlurExtent());
      blur_rect.Inset(blur_extent.x(), blur_extent.y());
      blur_rect.Intersect(spread_rect);
      if (blur_rect.IsEmpty()) {
        // Blur covers all of spread.
        blur_rect.set_origin(spread_rect.CenterPoint());
        blur_rect.set_size(math::SizeF());
      }
      draw.reset(new DrawRectShadowBlur(graphics_state_, draw_state_,
          blur_rect, data.rect, spread_rect, shadow_color, math::RectF(),
          data.shadow.blur_sigma, data.inset));
      onscreen_type = DrawObjectManager::kOnscreenRectShadowBlur;
    } else {
      draw.reset(new DrawRectShadowSpread(graphics_state_, draw_state_,
          spread_rect, data.rect, shadow_color, data.rect, math::RectF()));
    }
  } else {
    // blur_rect is outermost.
    // spread_rect is in the middle (barring negative |spread| values).
    // data.rect is innermost (though it may not overlap due to offset).
    spread_rect.Outset(data.spread, data.spread);
    if (spread_rect.IsEmpty()) {
      // Negative spread shenanigans! Nothing to draw.
      return;
    }
    math::RectF blur_rect(spread_rect);
    if (data.shadow.blur_sigma > 0.0f) {
      math::Vector2dF blur_extent(data.shadow.BlurExtent());
      blur_rect.Outset(blur_extent.x(), blur_extent.y());
      draw.reset(new DrawRectShadowBlur(graphics_state_, draw_state_,
          data.rect, blur_rect, spread_rect, shadow_color, data.rect,
          data.shadow.blur_sigma, data.inset));
      onscreen_type = DrawObjectManager::kOnscreenRectShadowBlur;
    } else {
      draw.reset(new DrawRectShadowSpread(graphics_state_, draw_state_,
          data.rect, spread_rect, shadow_color, spread_rect, data.rect));
    }
    node_bounds.Union(blur_rect);
  }

  // Include or exclude scissor will touch these pixels.
  node_bounds.Union(data.rect);

  // Since the depth buffer is polluted to create a stencil for pixels to be
  // modified by the shadow, this draw must occur during the transparency
  // pass. During this pass, all subsequent draws are guaranteed to be closer
  // (i.e. pass the depth test) than pixels modified by previous transparency
  // draws.
  AddTransparentDraw(draw.Pass(), onscreen_type,
      DrawObjectManager::kOffscreenNone, node_bounds);

  // Since the box shadow draw objects use the depth stencil object, two depth
  // values were used. So skip an additional depth value.
  draw_state_.depth = GraphicsState::NextClosestDepth(draw_state_.depth);
}

void RenderTreeNodeVisitor::Visit(render_tree::TextNode* text_node) {
  if (!IsVisible(text_node->GetBounds())) {
    return;
  }

  FallbackRasterize(text_node, DrawObjectManager::kOffscreenSkiaText);
}

void RenderTreeNodeVisitor::FallbackRasterize(
    scoped_refptr<render_tree::Node> node,
    DrawObjectManager::OffscreenType offscreen_type) {
  DCHECK_NE(offscreen_type, DrawObjectManager::kOffscreenNone);
  base::optional<math::Matrix3F> original_transform;

  // Use fallback_rasterize_ to render to an offscreen target. Add a small
  // buffer to allow anti-aliased edges (e.g. rendered text).
  const float kBorderWidth = 1.0f;
  math::RectF node_bounds(node->GetBounds());
  math::RectF viewport(
      // Viewport position is used to translate rendering within the offscreen
      // render target. Use as small a target as possible.
      kBorderWidth - node_bounds.x(),
      kBorderWidth - node_bounds.y(),
      node_bounds.width() + 2 * kBorderWidth,
      node_bounds.height() + 2 * kBorderWidth);

  // Determine if the contents need to be scaled to preserve fidelity when
  // rasterized onscreen. If the scaled size is larger than the main viewport's
  // size, then just clamp to that.
  const float kScaleThreshold = 1.0f;
  math::RectF mapped_bounds(draw_state_.transform.MapRect(node_bounds));
  math::SizeF viewport_scale(1.0f, 1.0f);
  if (mapped_bounds.width() > viewport_size_.width() ||
      mapped_bounds.height() > viewport_size_.height()) {
    // Just render what will fit onscreen.
    mapped_bounds.Intersect(draw_state_.scissor);
    if (mapped_bounds.IsEmpty()) {
      return;
    }
    viewport.SetRect(-mapped_bounds.x(), -mapped_bounds.y(),
                     mapped_bounds.width(), mapped_bounds.height());
    // Treat the offscreen render target as a window that maps onto the main
    // render target.
    original_transform = draw_state_.transform;
    draw_state_.transform = math::Matrix3F::Identity();
    node = new render_tree::MatrixTransformNode(node, *original_transform);
  } else if (mapped_bounds.width() > node_bounds.width() + kScaleThreshold ||
             mapped_bounds.height() > node_bounds.height() + kScaleThreshold) {
    // Scale up the offscreen contents to avoid pixelation.
    viewport_scale.set_width(mapped_bounds.width() / node_bounds.width());
    viewport_scale.set_height(mapped_bounds.height() / node_bounds.height());
  }

  // Adjust the draw rect to accomodate the extra border and align texels with
  // pixels. Perform the smallest fractional pixel shift for alignment.
  float trans_x = std::floor(draw_state_.transform(0, 2) + 0.5f) -
      draw_state_.transform(0, 2);
  float trans_y = std::floor(draw_state_.transform(1, 2) + 0.5f) -
      draw_state_.transform(1, 2);
  math::RectF draw_rect(trans_x - viewport.x(), trans_y - viewport.y(),
      viewport.width(), viewport.height());

  if (draw_state_.opacity == 1.0f) {
    scoped_ptr<DrawObject> draw(new DrawRectTexture(graphics_state_,
        draw_state_, draw_rect, base::Bind(*fallback_rasterize_,
            scoped_refptr<render_tree::Node>(node), viewport, viewport_scale)));
    AddTransparentDraw(draw.Pass(), DrawObjectManager::kOnscreenRectTexture,
        offscreen_type, draw_rect);
  } else {
    scoped_ptr<DrawObject> draw(new DrawRectColorTexture(graphics_state_,
        draw_state_, draw_rect, render_tree::ColorRGBA(1.0f, 1.0f, 1.0f, 1.0f),
        base::Bind(*fallback_rasterize_,
            scoped_refptr<render_tree::Node>(node), viewport, viewport_scale)));
    AddTransparentDraw(draw.Pass(),
        DrawObjectManager::kOnscreenRectColorTexture, offscreen_type,
        draw_rect);
  }

  if (original_transform) {
    draw_state_.transform = *original_transform;
  }
}

bool RenderTreeNodeVisitor::IsVisible(const math::RectF& bounds) {
  math::RectF intersection = IntersectRects(
      draw_state_.transform.MapRect(bounds), draw_state_.scissor);
  return !intersection.IsEmpty();
}

void RenderTreeNodeVisitor::AddOpaqueDraw(scoped_ptr<DrawObject> object,
    DrawObjectManager::OnscreenType onscreen_type,
    DrawObjectManager::OffscreenType offscreen_type) {
  draw_object_manager_->AddOpaqueDraw(object.Pass(), onscreen_type,
      offscreen_type);
  draw_state_.depth = GraphicsState::NextClosestDepth(draw_state_.depth);
}

void RenderTreeNodeVisitor::AddTransparentDraw(scoped_ptr<DrawObject> object,
    DrawObjectManager::OnscreenType onscreen_type,
    DrawObjectManager::OffscreenType offscreen_type,
    const math::RectF& local_bounds) {
  draw_object_manager_->AddTransparentDraw(object.Pass(), onscreen_type,
      offscreen_type, draw_state_.transform.MapRect(local_bounds));
  draw_state_.depth = GraphicsState::NextClosestDepth(draw_state_.depth);
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
