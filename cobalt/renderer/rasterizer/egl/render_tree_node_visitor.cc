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
#include <limits>

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/optional.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/base/type_id.h"
#include "cobalt/math/matrix3_f.h"
#include "cobalt/math/transform_2d.h"
#include "cobalt/renderer/rasterizer/common/utils.h"
#include "cobalt/renderer/rasterizer/egl/draw_callback.h"
#include "cobalt/renderer/rasterizer/egl/draw_clear.h"
#include "cobalt/renderer/rasterizer/egl/draw_poly_color.h"
#include "cobalt/renderer/rasterizer/egl/draw_rect_color_texture.h"
#include "cobalt/renderer/rasterizer/egl/draw_rect_linear_gradient.h"
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

const render_tree::ColorRGBA kOpaqueWhite(1.0f, 1.0f, 1.0f, 1.0f);
const render_tree::ColorRGBA kTransparentBlack(0.0f, 0.0f, 0.0f, 0.0f);

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

math::Matrix3F GetTexcoordTransform(
    const OffscreenTargetManager::TargetInfo& target) {
  float scale_x = 1.0f / target.framebuffer->GetSize().width();
  float scale_y = 1.0f / target.framebuffer->GetSize().height();
  return math::Matrix3F::FromValues(
      target.region.width() * scale_x, 0, target.region.x() * scale_x,
      0, target.region.height() * scale_y, target.region.y() * scale_y,
      0, 0, 1);
}

}  // namespace

RenderTreeNodeVisitor::RenderTreeNodeVisitor(GraphicsState* graphics_state,
    DrawObjectManager* draw_object_manager,
    OffscreenTargetManager* offscreen_target_manager,
    const FallbackRasterizeFunction& fallback_rasterize,
    const backend::RenderTarget* render_target,
    const math::Rect& content_rect)
    : graphics_state_(graphics_state),
      draw_object_manager_(draw_object_manager),
      offscreen_target_manager_(offscreen_target_manager),
      fallback_rasterize_(fallback_rasterize),
      render_target_(render_target),
      render_target_is_offscreen_(false),
      scratch_surface_being_used_(false) {
  draw_state_.scissor.Intersect(content_rect);
}

void RenderTreeNodeVisitor::Visit(
    render_tree::CompositionNode* composition_node) {
  const render_tree::CompositionNode::Builder& data = composition_node->data();
  math::Matrix3F old_transform = draw_state_.transform;
  draw_state_.transform = draw_state_.transform *
      math::TranslateMatrix(data.offset().x(), data.offset().y());
  const render_tree::CompositionNode::Children& children =
      data.children();
  for (render_tree::CompositionNode::Children::const_iterator iter =
       children.begin(); iter != children.end(); ++iter) {
    (*iter)->Accept(this);
  }
  draw_state_.transform = old_transform;
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
      !data.map_to_mesh_filter) {
    const float filter_opacity = data.opacity_filter->opacity();
    if (filter_opacity <= 0.0f) {
      // Totally transparent. Ignore the source.
      return;
    } else if (filter_opacity >= 1.0f) {
      // Totally opaque. Render like normal.
      data.source->Accept(this);
      return;
    } else if (common::utils::NodeCanRenderWithOpacity(data.source)) {
      // Simple opacity that does not require an offscreen target.
      float old_opacity = draw_state_.opacity;
      draw_state_.opacity *= filter_opacity;
      data.source->Accept(this);
      draw_state_.opacity = old_opacity;
      return;
    } else {
      // Complex opacity that requires an offscreen target.
      math::Matrix3F texcoord_transform(math::Matrix3F::Identity());
      math::RectF content_rect;
      const backend::TextureEGL* texture = nullptr;

      // Render source at 100% opacity to an offscreen target, then render
      // that result with the specified filter opacity.
      OffscreenRasterize(data.source, &texture, &texcoord_transform,
                         &content_rect);
      if (content_rect.IsEmpty()) {
        return;
      }

      // The content rect is already in screen space, so reset the transform.
      math::Matrix3F old_transform = draw_state_.transform;
      draw_state_.transform = math::Matrix3F::Identity();
      scoped_ptr<DrawObject> draw(new DrawRectColorTexture(graphics_state_,
          draw_state_, content_rect, kOpaqueWhite * filter_opacity, texture,
          texcoord_transform, false /* clamp_texcoords */));
      AddTransparentDraw(draw.Pass(), content_rect);
      draw_state_.transform = old_transform;
      return;
    }
  }

  // Handle blur-only filter.
  if (data.blur_filter &&
      !data.viewport_filter &&
      !data.opacity_filter &&
      !data.map_to_mesh_filter) {
    if (data.blur_filter->blur_sigma() == 0.0f) {
      // Ignorable blur request. Render normally.
      data.source->Accept(this);
      return;
    }
  }

  // No filter.
  if (!data.opacity_filter &&
      !data.viewport_filter &&
      !data.blur_filter &&
      !data.map_to_mesh_filter) {
    data.source->Accept(this);
    return;
  }

  // Use the fallback rasterizer to handle everything else.
  FallbackRasterize(filter_node);
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

  if (skia_image->GetTypeId() == base::GetTypeId<skia::SinglePlaneImage>()) {
    skia::HardwareFrontendImage* hardware_image =
        base::polymorphic_downcast<skia::HardwareFrontendImage*>(skia_image);
    if (clamp_texcoords || !is_opaque) {
      draw.reset(new DrawRectColorTexture(graphics_state_, draw_state_,
          data.destination_rect, kOpaqueWhite,
          hardware_image->GetTextureEGL(), texcoord_transform,
          clamp_texcoords));
    } else {
      draw.reset(new DrawRectTexture(graphics_state_, draw_state_,
          data.destination_rect, hardware_image->GetTextureEGL(),
          texcoord_transform));
    }
  } else if (skia_image->GetTypeId() ==
             base::GetTypeId<skia::MultiPlaneImage>()) {
    FallbackRasterize(image_node);
    return;
  } else {
    NOTREACHED();
    return;
  }

  if (is_opaque) {
    AddOpaqueDraw(draw.Pass(), image_node->GetBounds());
  } else {
    AddTransparentDraw(draw.Pass(), image_node->GetBounds());
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
      draw_state_, data.rect, kTransparentBlack));
  AddOpaqueDraw(draw.Pass(), video_node->GetBounds());
}

void RenderTreeNodeVisitor::Visit(render_tree::RectNode* rect_node) {
  if (!IsVisible(rect_node->GetBounds())) {
    return;
  }

  const render_tree::RectNode::Builder& data = rect_node->data();
  const scoped_ptr<render_tree::Brush>& brush = data.background_brush;

  // Only solid color brushes are supported at this time.
  const bool brush_supported = !brush ||
      brush->GetTypeId() == base::GetTypeId<render_tree::SolidColorBrush>() ||
      brush->GetTypeId() == base::GetTypeId<render_tree::LinearGradientBrush>();

  // Borders are not supported natively by this rasterizer at this time. The
  // difficulty lies in getting anti-aliased borders and minimizing state
  // switches (due to anti-aliased borders requiring transparency). However,
  // by using the fallback rasterizer, both can be accomplished -- sort to
  // minimize state switches while rendering anti-aliased borders to the
  // offscreen target, then use a single shader to render those.
  const bool border_supported = !data.border;

  if (data.rounded_corners) {
    FallbackRasterize(rect_node);
  } else if (!brush_supported) {
    FallbackRasterize(rect_node);
  } else if (!border_supported) {
    FallbackRasterize(rect_node);
  } else {
    DCHECK(!data.border);
    const math::RectF& content_rect(data.rect);

    // Handle drawing the content.
    if (brush) {
      base::TypeId brush_type = brush->GetTypeId();
      if (brush_type == base::GetTypeId<render_tree::SolidColorBrush>()) {
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
          AddOpaqueDraw(draw.Pass(), rect_node->GetBounds());
        } else {
          AddTransparentDraw(draw.Pass(), rect_node->GetBounds());
        }
      } else {
        const render_tree::LinearGradientBrush* linear_brush =
            base::polymorphic_downcast<const render_tree::LinearGradientBrush*>
                (brush.get());
        scoped_ptr<DrawObject> draw(new DrawRectLinearGradient(graphics_state_,
            draw_state_, content_rect, *linear_brush));
        // The draw may use transparent pixels to ensure only pixels in the
        // content_rect are modified.
        AddTransparentDraw(draw.Pass(), rect_node->GetBounds());
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
    FallbackRasterize(shadow_node);
    return;
  }

  scoped_ptr<DrawObject> draw;
  render_tree::ColorRGBA shadow_color(
      data.shadow.color.r() * data.shadow.color.a(),
      data.shadow.color.g() * data.shadow.color.a(),
      data.shadow.color.b() * data.shadow.color.a(),
      data.shadow.color.a());

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
          blur_rect, data.rect, spread_rect, shadow_color,
          data.shadow.blur_sigma, data.inset));
    } else {
      draw.reset(new DrawRectShadowSpread(graphics_state_, draw_state_,
          spread_rect, data.rect, shadow_color, data.rect));
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
          data.rect, blur_rect, spread_rect, shadow_color,
          data.shadow.blur_sigma, data.inset));
    } else {
      draw.reset(new DrawRectShadowSpread(graphics_state_, draw_state_,
          data.rect, spread_rect, shadow_color, spread_rect));
    }
    node_bounds.Union(blur_rect);
  }

  // Transparency is used to skip pixels that are not shadowed.
  AddTransparentDraw(draw.Pass(), node_bounds);
}

void RenderTreeNodeVisitor::Visit(render_tree::TextNode* text_node) {
  if (!IsVisible(text_node->GetBounds())) {
    return;
  }

  FallbackRasterize(text_node);
}

// Get an offscreen target to render |node|.
// |out_content_cached| is true if the node's contents are already cached in
//   the returned offscreen target.
// |out_target_info| describes the offscreen surface into which |node| should
//   be rendered.
// |out_content_rect| is the onscreen rect (already in screen space) where the
//   offscreen contents should be rendered.
void RenderTreeNodeVisitor::GetOffscreenTarget(
    scoped_refptr<render_tree::Node> node,
    bool* out_content_cached,
    OffscreenTargetManager::TargetInfo* out_target_info,
    math::RectF* out_content_rect) {
  // Default to telling the caller that nothing should be rendered.
  *out_content_cached = true;
  out_content_rect->SetRect(0, 0, 0, 0);

  math::RectF node_bounds(node->GetBounds());
  math::RectF mapped_bounds(draw_state_.transform.MapRect(node_bounds));
  if (mapped_bounds.IsEmpty()) {
    return;
  }

  // Request a slightly larger render target than the calculated bounds. The
  // rasterizer may use an extra pixel along the edge of anti-aliased objects.
  const float kBorderWidth = 1.0f;

  // The render target cache keys off the render_tree Node and target size.
  // Since the size is rounded, it is possible to be a fraction of a pixel
  // off. However, this in turn allows for a cache hit for "close-enough"
  // renderings. To minimize offset errors, use the nearest full pixel offset.
  const float kFractionPad = 1.0f;
  float offset_x = std::floor(mapped_bounds.x() + 0.5f);
  float offset_y = std::floor(mapped_bounds.y() + 0.5f);
  math::PointF content_offset(
      offset_x - kBorderWidth - kFractionPad,
      offset_y - kBorderWidth - kFractionPad);
  math::SizeF content_size(
      std::ceil(mapped_bounds.width() + 2.0f * kBorderWidth + kFractionPad),
      std::ceil(mapped_bounds.height() + 2.0f * kBorderWidth + kFractionPad));
  *out_content_cached = offscreen_target_manager_->GetCachedOffscreenTarget(
      node, content_size, out_target_info);
  if (!(*out_content_cached)) {
    offscreen_target_manager_->AllocateOffscreenTarget(node,
        content_size, out_target_info);
  }

  // If the render target is the scratch surface, then just render what fits
  // onscreen for better performance.
  if (out_target_info->is_scratch_surface) {
    // The scratch surface can only be used for one scene at a time.
    if (scratch_surface_being_used_) {
      DLOG(ERROR) << "Too many offscreen surfaces have been requested. Some"
                     " elements will not be rendered. Increase the surface"
                     " cache size to fix any visual artifacts.";
      return;
    }

    mapped_bounds.Outset(kBorderWidth, kBorderWidth);
    mapped_bounds.Intersect(draw_state_.scissor);
    if (mapped_bounds.IsEmpty()) {
      return;
    }
    float left = std::floor(mapped_bounds.x());
    float right = std::ceil(mapped_bounds.right());
    float top = std::floor(mapped_bounds.y());
    float bottom = std::ceil(mapped_bounds.bottom());
    content_offset.SetPoint(left, top);
    content_size.SetSize(right - left, bottom - top);
  }

  // The returned target may be larger than actually needed. Clamp to just the
  // needed size.
  DCHECK_LE(content_size.width(), out_target_info->region.width());
  DCHECK_LE(content_size.height(), out_target_info->region.height());
  out_target_info->region.set_size(content_size);

  out_content_rect->set_origin(content_offset);
  out_content_rect->set_size(content_size);
}

void RenderTreeNodeVisitor::FallbackRasterize(
    scoped_refptr<render_tree::Node> node) {
  OffscreenTargetManager::TargetInfo target_info;
  math::RectF content_rect;
  bool content_is_cached = false;
  GetOffscreenTarget(node, &content_is_cached, &target_info, &content_rect);

  if (content_rect.IsEmpty()) {
    return;
  }

  // Setup draw for the contents as needed.
  if (!content_is_cached) {
    FallbackRasterize(node, target_info, content_rect);
  }

  // Sub-pixel offsets are passed to the fallback rasterizer to preserve
  // sharpness. The results should be drawn to |content_rect| which is already
  // in screen space.
  math::Matrix3F old_transform = draw_state_.transform;
  draw_state_.transform = math::Matrix3F::Identity();

  // Create the appropriate draw object to call the draw callback, then render
  // its results onscreen. A transparent draw must be used even if the current
  // opacity is 100% because the contents may have transparency.
  backend::TextureEGL* texture = target_info.framebuffer->GetColorTexture();
  math::Matrix3F texcoord_transform = GetTexcoordTransform(target_info);
  if (draw_state_.opacity == 1.0f) {
    scoped_ptr<DrawObject> draw(new DrawRectTexture(graphics_state_,
        draw_state_, content_rect, texture, texcoord_transform));
    AddTransparentDraw(draw.Pass(), content_rect);
  } else {
    scoped_ptr<DrawObject> draw(new DrawRectColorTexture(graphics_state_,
        draw_state_, content_rect, kOpaqueWhite, texture, texcoord_transform,
        false /* clamp_texcoords */));
    AddTransparentDraw(draw.Pass(), content_rect);
  }

  draw_state_.transform = old_transform;
}

void RenderTreeNodeVisitor::FallbackRasterize(
    scoped_refptr<render_tree::Node> node,
    const OffscreenTargetManager::TargetInfo& target_info,
    const math::RectF& content_rect) {
  // Pre-translate the contents so it starts near the origin. The fallback
  // rasterizer will then translate this to start drawing in target_info's
  // region.
  math::Matrix3F content_transform =
      math::TranslateMatrix(-content_rect.x(), -content_rect.y()) *
      draw_state_.transform;
  base::Closure rasterize_callback = base::Bind(fallback_rasterize_,
      scoped_refptr<render_tree::Node>(node), content_transform,
      target_info);
  scoped_ptr<DrawObject> draw(new DrawCallback(rasterize_callback));

  if (target_info.is_scratch_surface) {
    // The scratch surface must be updated just before the subsequent draw
    // uses it. So make sure it goes into the same draw category as the next
    // draw.
    if (render_target_is_offscreen_) {
      // TODO: Sorting needs to be addressed for native offscren draws that use
      // the fallback rasterizer with the scratch surface.
      NOTREACHED();
    } else {
      draw_object_manager_->AddOnscreenDraw(draw.Pass(),
          DrawObjectManager::kBlendExternal, node->GetTypeId(),
          target_info.framebuffer, target_info.region);
    }
  } else {
    draw_object_manager_->AddOffscreenDraw(draw.Pass(),
        DrawObjectManager::kBlendExternal, node->GetTypeId(),
        target_info.framebuffer, target_info.region);
  }
}

// Add draw objects to render |node| to an offscreen render target.
// |out_texture| and |out_texcoord_transform| describe the texture subregion
//   that will contain the result of rendering |node|.
// |out_content_rect| describes the onscreen rect (in screen space) which
//   should be used to render node's contents.
void RenderTreeNodeVisitor::OffscreenRasterize(
    scoped_refptr<render_tree::Node> node,
    const backend::TextureEGL** out_texture,
    math::Matrix3F* out_texcoord_transform,
    math::RectF* out_content_rect) {
  OffscreenTargetManager::TargetInfo target_info;
  bool content_is_cached = false;
  GetOffscreenTarget(node, &content_is_cached, &target_info, out_content_rect);

  if (out_content_rect->IsEmpty()) {
    return;
  }

  *out_texture = target_info.framebuffer->GetColorTexture();
  *out_texcoord_transform = GetTexcoordTransform(target_info);

  // TODO: Resolve the coordinate system mismatch between the fallback
  // rasterizer and native rasterizer so they both render to the expected
  // region of the render target.
  //
  // Also resolve using an offscreen render for portions of |node| while
  // rendering to offscreen already. Drivers may think this can result in a
  // feedback loop if both the sampler and render target point to the same
  // surface. However, they are guaranteed to be different regions.
  bool use_native_rasterizer = false;

  if (!content_is_cached) {
    if (use_native_rasterizer) {
      // Push a new render state to rasterize to the offscreen render target.
      DrawObject::BaseState old_draw_state = draw_state_;
      const backend::RenderTarget* old_render_target = render_target_;
      bool old_render_target_is_offscreen = render_target_is_offscreen_;
      bool old_scratch_surface_being_used = scratch_surface_being_used_;

      draw_state_.transform =
          math::TranslateMatrix(target_info.region.x() - out_content_rect->x(),
                                target_info.region.y() - out_content_rect->y())
          * draw_state_.transform;
      draw_state_.scissor = RoundRectFToInt(target_info.region);
      draw_state_.opacity = 1.0f;
      render_target_ = target_info.framebuffer;
      render_target_is_offscreen_ = !target_info.is_scratch_surface;
      scratch_surface_being_used_ = scratch_surface_being_used_ ||
                                    target_info.is_scratch_surface;

      // The scratch surface must be manually cleared before being used.
      if (target_info.is_scratch_surface) {
        scoped_ptr<DrawObject> draw(new DrawClear(graphics_state_, draw_state_,
            kTransparentBlack));
        AddOpaqueDraw(draw.Pass(), node->GetBounds());
      }
      node->Accept(this);

      draw_state_ = old_draw_state;
      render_target_ = old_render_target;
      render_target_is_offscreen_ = old_render_target_is_offscreen;
      scratch_surface_being_used_ = old_scratch_surface_being_used;
    } else {
      FallbackRasterize(node, target_info, *out_content_rect);
    }
  }
}

bool RenderTreeNodeVisitor::IsVisible(const math::RectF& bounds) {
  math::RectF intersection = IntersectRects(
      draw_state_.transform.MapRect(bounds), draw_state_.scissor);
  return !intersection.IsEmpty();
}

void RenderTreeNodeVisitor::AddOpaqueDraw(scoped_ptr<DrawObject> object,
    const math::RectF& local_bounds) {
  base::TypeId draw_type = object->GetTypeId();
  if (render_target_is_offscreen_) {
    draw_object_manager_->AddOffscreenDraw(object.Pass(),
        DrawObjectManager::kBlendNone, draw_type, render_target_,
        draw_state_.transform.MapRect(local_bounds));
  } else {
    draw_object_manager_->AddOnscreenDraw(object.Pass(),
        DrawObjectManager::kBlendNone, draw_type, render_target_,
        draw_state_.transform.MapRect(local_bounds));
  }
}

void RenderTreeNodeVisitor::AddTransparentDraw(scoped_ptr<DrawObject> object,
    const math::RectF& local_bounds) {
  base::TypeId draw_type = object->GetTypeId();
  if (render_target_is_offscreen_) {
    draw_object_manager_->AddOffscreenDraw(object.Pass(),
        DrawObjectManager::kBlendSrcAlpha, draw_type, render_target_,
        draw_state_.transform.MapRect(local_bounds));
  } else {
    draw_object_manager_->AddOnscreenDraw(object.Pass(),
        DrawObjectManager::kBlendSrcAlpha, draw_type, render_target_,
        draw_state_.transform.MapRect(local_bounds));
  }
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
