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
#include "cobalt/renderer/rasterizer/egl/draw_rrect_color.h"
#include "cobalt/renderer/rasterizer/egl/draw_rrect_color_texture.h"
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
  // Flip the texture vertically to accommodate OpenGL's bottom-left origin.
  float scale_x = 1.0f / target.framebuffer->GetSize().width();
  float scale_y = -1.0f / target.framebuffer->GetSize().height();
  return math::Matrix3F::FromValues(
      target.region.width() * scale_x, 0, target.region.x() * scale_x,
      0, target.region.height() * scale_y, 1.0f + target.region.y() * scale_y,
      0, 0, 1);
}

bool ImageNodeSupportedNatively(render_tree::ImageNode* image_node) {
  skia::Image* skia_image = base::polymorphic_downcast<skia::Image*>(
      image_node->data().source.get());
  if (skia_image->GetTypeId() == base::GetTypeId<skia::SinglePlaneImage>() &&
      skia_image->CanRenderInSkia()) {
    return true;
  }
  return false;
}

bool RoundedViewportSupportedForSource(render_tree::Node* source) {
  base::TypeId source_type = source->GetTypeId();
  if (source_type == base::GetTypeId<render_tree::ImageNode>()) {
    render_tree::ImageNode* image_node =
        base::polymorphic_downcast<render_tree::ImageNode*>(source);
    return ImageNodeSupportedNatively(image_node);
  } else if (source_type == base::GetTypeId<render_tree::CompositionNode>()) {
    // If this is a composition of valid sources, then rendering with a rounded
    // viewport is also supported.
    render_tree::CompositionNode* composition_node =
        base::polymorphic_downcast<render_tree::CompositionNode*>(source);
    typedef render_tree::CompositionNode::Children Children;
    const Children& children = composition_node->data().children();

    for (Children::const_iterator iter = children.begin();
         iter != children.end(); ++iter) {
      if (!RoundedViewportSupportedForSource(iter->get())) {
        return false;
      }
    }
    return true;
  }

  return false;
}

// Return the error value of a given offscreen taget cache entry.
// |desired_bounds| specifies the world-space bounds to which an offscreen
//   target will be rendered.
// |cached_bounds| specifies the world-space bounds used when the offscreen
//   target was generated.
// Lower return values indicate better fits. Only cache entries with an error
//   less than 1 will be considered suitable.
float OffscreenTargetErrorFunction(const math::RectF& desired_bounds,
                                   const math::RectF& cached_bounds) {
  // The cached contents must be within 0.5 pixels of the desired size to avoid
  // scaling artifacts.
  if (std::abs(desired_bounds.width() - cached_bounds.width()) >= 0.5f ||
      std::abs(desired_bounds.height() - cached_bounds.height()) >= 0.5f) {
    return 1.0f;
  }

  // The cached contents' sub-pixel offset must be within 0.5 pixels to ensure
  // appropriate positioning.
  math::PointF desired_offset(
      desired_bounds.x() - std::floor(desired_bounds.x()),
      desired_bounds.y() - std::floor(desired_bounds.y()));
  math::PointF cached_offset(
      cached_bounds.x() - std::floor(cached_bounds.x()),
      cached_bounds.y() - std::floor(cached_bounds.y()));
  float error_x = std::abs(desired_offset.x() - cached_offset.x());
  float error_y = std::abs(desired_offset.y() - cached_offset.y());
  if (error_x >= 0.5f || error_y >= 0.5f) {
    return 1.0f;
  }

  return error_x + error_y;
}

}  // namespace

RenderTreeNodeVisitor::RenderTreeNodeVisitor(GraphicsState* graphics_state,
    DrawObjectManager* draw_object_manager,
    OffscreenTargetManager* offscreen_target_manager,
    const FallbackRasterizeFunction& fallback_rasterize,
    SkCanvas* fallback_render_target,
    backend::RenderTarget* render_target,
    const math::Rect& content_rect)
    : graphics_state_(graphics_state),
      draw_object_manager_(draw_object_manager),
      offscreen_target_manager_(offscreen_target_manager),
      fallback_rasterize_(fallback_rasterize),
      fallback_render_target_(fallback_render_target),
      render_target_(render_target),
      render_target_is_offscreen_(false),
      allow_offscreen_targets_(true),
      failed_offscreen_target_request_(false),
      last_draw_id_(0) {
  draw_state_.scissor.Intersect(content_rect);
}

void RenderTreeNodeVisitor::Visit(
    render_tree::CompositionNode* composition_node) {
  const render_tree::CompositionNode::Builder& data = composition_node->data();
  math::Matrix3F old_transform = draw_state_.transform;
  draw_state_.transform = draw_state_.transform *
      math::TranslateMatrix(data.offset().x(), data.offset().y());
  draw_state_.rounded_scissor_rect.Offset(-data.offset());
  const render_tree::CompositionNode::Children& children =
      data.children();
  for (render_tree::CompositionNode::Children::const_iterator iter =
       children.begin(); iter != children.end(); ++iter) {
    (*iter)->Accept(this);
  }
  draw_state_.rounded_scissor_rect.Offset(data.offset());
  draw_state_.transform = old_transform;
}

void RenderTreeNodeVisitor::Visit(
    render_tree::MatrixTransform3DNode* transform_3d_node) {
  // This is used in conjunction with a map-to-mesh filter. If that filter is
  // implemented natively, then this transform 3D must be handled natively
  // as well. Otherwise, use the fallback rasterizer for both.
  FallbackRasterize(transform_3d_node);
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

  // Handle viewport-only filter.
  if (data.viewport_filter &&
      !data.opacity_filter &&
      !data.blur_filter &&
      !data.map_to_mesh_filter) {
    const math::Matrix3F& transform = draw_state_.transform;
    if (data.viewport_filter->has_rounded_corners()) {
      // Certain source nodes have an optimized path for rendering inside
      // rounded viewports.
      if (RoundedViewportSupportedForSource(data.source)) {
        DCHECK(!draw_state_.rounded_scissor_corners);
        draw_state_.rounded_scissor_rect = data.viewport_filter->viewport();
        draw_state_.rounded_scissor_corners =
            data.viewport_filter->rounded_corners();
        draw_state_.rounded_scissor_corners->Normalize(
            draw_state_.rounded_scissor_rect);
        data.source->Accept(this);
        draw_state_.rounded_scissor_corners = base::nullopt;
        return;
      }
    } else if (IsOnlyScaleAndTranslate(transform)) {
      // Orthogonal viewport filters without rounded corners can be collapsed
      // into the world-space scissor.

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
      if (texture != nullptr) {
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

  if (!ImageNodeSupportedNatively(image_node)) {
    FallbackRasterize(image_node);
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
    if (hardware_image->alternate_rgba_format()) {
      // We don't yet handle alternative formats that piggyback on a GL_RGBA
      // texture.  This comes up, for example, with UYVY (YUV 422) textures.
      FallbackRasterize(image_node);
      return;
    }

    if (draw_state_.rounded_scissor_corners) {
      // Transparency is used to anti-alias the rounded rect.
      is_opaque = false;
      draw.reset(new DrawRRectColorTexture(graphics_state_, draw_state_,
          data.destination_rect, kOpaqueWhite,
          hardware_image->GetTextureEGL(), texcoord_transform,
          clamp_texcoords));
    } else if (clamp_texcoords || !is_opaque) {
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

  if (data.rounded_corners && brush &&
      brush->GetTypeId() != base::GetTypeId<render_tree::SolidColorBrush>()) {
    FallbackRasterize(rect_node);
  } else if (!brush_supported) {
    FallbackRasterize(rect_node);
  } else if (!border_supported) {
    FallbackRasterize(rect_node);
  } else {
    DCHECK(!data.border);

    // Handle drawing the content.
    if (brush) {
      base::TypeId brush_type = brush->GetTypeId();
      if (brush_type == base::GetTypeId<render_tree::SolidColorBrush>()) {
        const render_tree::SolidColorBrush* solid_brush =
            base::polymorphic_downcast<const render_tree::SolidColorBrush*>
                (brush.get());
        const render_tree::ColorRGBA& brush_color(solid_brush->color());
        render_tree::ColorRGBA color(
            brush_color.r() * brush_color.a(),
            brush_color.g() * brush_color.a(),
            brush_color.b() * brush_color.a(),
            brush_color.a());
        if (data.rounded_corners) {
          scoped_ptr<DrawObject> draw(new DrawRRectColor(graphics_state_,
              draw_state_, data.rect, *data.rounded_corners, color));
          // Transparency is used for anti-aliasing.
          AddTransparentDraw(draw.Pass(), rect_node->GetBounds());
        } else {
          scoped_ptr<DrawObject> draw(new DrawPolyColor(graphics_state_,
              draw_state_, data.rect, color));
          if (draw_state_.opacity * color.a() == 1.0f) {
            AddOpaqueDraw(draw.Pass(), rect_node->GetBounds());
          } else {
            AddTransparentDraw(draw.Pass(), rect_node->GetBounds());
          }
        }
      } else {
        const render_tree::LinearGradientBrush* linear_brush =
            base::polymorphic_downcast<const render_tree::LinearGradientBrush*>
                (brush.get());
        scoped_ptr<DrawObject> draw(new DrawRectLinearGradient(graphics_state_,
            draw_state_, data.rect, *linear_brush));
        // The draw may use transparent pixels to ensure only pixels in the
        // specified area are modified.
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
  base::optional<render_tree::RoundedCorners> spread_corners =
      data.rounded_corners;

  scoped_ptr<DrawObject> draw;
  render_tree::ColorRGBA shadow_color(
      data.shadow.color.r() * data.shadow.color.a(),
      data.shadow.color.g() * data.shadow.color.a(),
      data.shadow.color.b() * data.shadow.color.a(),
      data.shadow.color.a());

  math::RectF spread_rect(data.rect);
  spread_rect.Offset(data.shadow.offset);
  if (data.inset) {
    if (spread_corners) {
      spread_corners = spread_corners->Inset(
          data.spread, data.spread, data.spread, data.spread);
    }
    spread_rect.Inset(data.spread, data.spread);
    if (!spread_rect.IsEmpty() && data.shadow.blur_sigma > 0.0f) {
      draw.reset(new DrawRectShadowBlur(graphics_state_, draw_state_,
          data.rect, data.rounded_corners, spread_rect, spread_corners,
          shadow_color, data.shadow.blur_sigma, data.inset));
    } else {
      draw.reset(new DrawRectShadowSpread(graphics_state_, draw_state_,
          spread_rect, spread_corners, data.rect, data.rounded_corners,
          shadow_color));
    }
  } else {
    if (spread_corners) {
      spread_corners = spread_corners->Inset(
          -data.spread, -data.spread, -data.spread, -data.spread);
    }
    spread_rect.Outset(data.spread, data.spread);
    if (spread_rect.IsEmpty()) {
      // Negative spread shenanigans! Nothing to draw.
      return;
    }
    if (data.shadow.blur_sigma > 0.0f) {
      draw.reset(new DrawRectShadowBlur(graphics_state_, draw_state_,
          data.rect, data.rounded_corners, spread_rect, spread_corners,
          shadow_color, data.shadow.blur_sigma, data.inset));
    } else {
      draw.reset(new DrawRectShadowSpread(graphics_state_, draw_state_,
          data.rect, data.rounded_corners, spread_rect, spread_corners,
          shadow_color));
    }
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
  out_content_rect->SetRect(0.0f, 0.0f, 0.0f, 0.0f);

  if (!allow_offscreen_targets_) {
    failed_offscreen_target_request_ = true;
    return;
  }

  math::RectF node_bounds(node->GetBounds());
  math::RectF mapped_bounds(draw_state_.transform.MapRect(node_bounds));
  if (mapped_bounds.IsEmpty()) {
    return;
  }

  // Request a slightly larger render target than the calculated bounds. The
  // rasterizer may use an extra pixel along the edge of anti-aliased objects.
  const float kBorderWidth = 1.0f;
  math::PointF content_offset(
      std::floor(mapped_bounds.x() - kBorderWidth),
      std::floor(mapped_bounds.y() - kBorderWidth));
  math::SizeF content_size(
      std::ceil(mapped_bounds.right() + kBorderWidth - content_offset.x()),
      std::ceil(mapped_bounds.bottom() + kBorderWidth - content_offset.y()));

  // Get a suitable cache of the render tree node if one exists, or allocate
  // a new offscreen target if possible. The OffscreenTargetErrorFunction will
  // determine whether any caches are fit for use.
  *out_content_cached = offscreen_target_manager_->GetCachedOffscreenTarget(
      node, base::Bind(&OffscreenTargetErrorFunction, mapped_bounds),
      out_target_info);
  if (!(*out_content_cached)) {
    offscreen_target_manager_->AllocateOffscreenTarget(node,
        content_size, mapped_bounds, out_target_info);
  }

  // If no offscreen target could be allocated, then the render tree node will
  // be rendered directly onto the main framebuffer. Use the current scissor
  // to minimize what has to be drawn.
  if (out_target_info->framebuffer == nullptr) {
    math::RectF content_bounds(content_offset, content_size);
    content_bounds.Intersect(draw_state_.scissor);
    if (content_bounds.IsEmpty()) {
      return;
    }
    content_offset = content_bounds.origin();
    content_size = content_bounds.size();
  }

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

  // If no offscreen target was available, then just render directly onto the
  // current render target.
  if (target_info.framebuffer == nullptr) {
    base::Closure rasterize_callback = base::Bind(fallback_rasterize_,
        node, fallback_render_target_, draw_state_.transform, content_rect,
        draw_state_.opacity, kFallbackShouldFlush);
    scoped_ptr<DrawObject> draw(new DrawCallback(rasterize_callback));
    AddExternalDraw(draw.Pass(), content_rect, node->GetTypeId());
    return;
  }

  // Setup draw for the contents as needed.
  if (!content_is_cached) {
    // Cache the results when drawn with 100% opacity, then draw the cached
    // results at the desired opacity. This avoids having to generate different
    // caches under varying opacity filters.
    float old_opacity = draw_state_.opacity;
    draw_state_.opacity = 1.0f;
    FallbackRasterize(node, target_info, content_rect);
    draw_state_.opacity = old_opacity;
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
  // It is not permitted to render to an offscreen target while already
  // rendering to an offscreen target. To allow this path, ensure that
  // render targets are not used as both the read and write targets for any
  // call. (Although these reads and writes should occur in different regions
  // of the target, not all drivers may handle this properly.) Also ensure the
  // draw object manager will sort these draws properly.
  DCHECK(!render_target_is_offscreen_);

  uint32_t rasterize_flags = 0;

  // Pre-translate the content so it starts in target_info.region.
  math::Matrix3F content_transform =
      math::TranslateMatrix(target_info.region.x() - content_rect.x(),
                            target_info.region.y() - content_rect.y()) *
      draw_state_.transform;
  base::Closure rasterize_callback = base::Bind(fallback_rasterize_,
      node, target_info.skia_canvas, content_transform, target_info.region,
      draw_state_.opacity, rasterize_flags);
  scoped_ptr<DrawObject> draw(new DrawCallback(rasterize_callback));

  backend::RenderTarget* old_render_target = render_target_;
  bool old_render_target_is_offscreen = render_target_is_offscreen_;

  render_target_ = target_info.framebuffer;
  render_target_is_offscreen_ = true;
  AddExternalDraw(draw.Pass(), target_info.region, node->GetTypeId());

  render_target_ = old_render_target;
  render_target_is_offscreen_ = old_render_target_is_offscreen;
}

// Add draw objects to render |node| to an offscreen render target at
//   100% opacity.
// |out_texture| and |out_texcoord_transform| describe the texture subregion
//   that will contain the result of rendering |node|. If not enough memory
//   is available for the offscreen target, then |out_texture| will be null.
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

  if (target_info.framebuffer == nullptr) {
    // No offscreen target was available.
    *out_texture = nullptr;
    return;
  }

  *out_texture = target_info.framebuffer->GetColorTexture();
  *out_texcoord_transform = GetTexcoordTransform(target_info);

  if (!content_is_cached) {
    // Cache the results at 100% opacity. The caller is responsible for
    // drawing the cached results at the desired opacity. This avoids having
    // to generate different caches under varying opacity filters.
    float old_opacity = draw_state_.opacity;
    draw_state_.opacity = 1.0f;

    // Try to use the native rasterizer to handle the offscreen rendering.
    // However, because offscreen targets are actually regions of a texture
    // atlas, some drivers may not properly handle reading and writing to
    // the same texture -- even if the operations occur in different regions.
    // So native offscreen handling can only occur if |node| or its children
    // do not also need offscreen targets (or this particular target). The
    // alternative is to use the fallback rasterizer since it allocates its
    // own render targets as needed.
    //
    // Ideally, pre-check |node| and its children to see if they will need
    // an offscreen target. However, this would result in a lot of duplicate
    // code. So just process the nodes into draws, and if any of them requests
    // an offscreen target, then remove all the recently added draws, and use
    // the fallback rasterizer instead of the native rasterizer.
    uint32_t last_valid_draw_id = last_draw_id_;
    bool old_allow_offscreen_targets = allow_offscreen_targets_;
    bool old_failed_offscreen_target_request = failed_offscreen_target_request_;
    allow_offscreen_targets_ = false;
    failed_offscreen_target_request_ = false;

    // Push a new render state to rasterize to the offscreen render target.
    DrawObject::BaseState old_draw_state = draw_state_;
    SkCanvas* old_fallback_render_target = fallback_render_target_;
    backend::RenderTarget* old_render_target = render_target_;
    bool old_render_target_is_offscreen = render_target_is_offscreen_;

    // Adjust the transform to render into target_info.region.
    draw_state_.transform =
        math::TranslateMatrix(target_info.region.x() - out_content_rect->x(),
                              target_info.region.y() - out_content_rect->y()) *
        draw_state_.transform;
    draw_state_.scissor = RoundRectFToInt(target_info.region);
    fallback_render_target_ = target_info.skia_canvas;
    render_target_ = target_info.framebuffer;
    render_target_is_offscreen_ = true;

    node->Accept(this);

    draw_state_ = old_draw_state;
    fallback_render_target_ = old_fallback_render_target;
    render_target_ = old_render_target;
    render_target_is_offscreen_ = old_render_target_is_offscreen;

    bool use_fallback_rasterizer = failed_offscreen_target_request_;
    allow_offscreen_targets_ = old_allow_offscreen_targets;
    failed_offscreen_target_request_ = old_failed_offscreen_target_request;

    if (use_fallback_rasterizer) {
      // The node or one of its children needed an offscreen target, so this
      // cannot be rendered natively. Remove all the draws added for |node|,
      // and just use the fallback rasterizer instead.
      draw_object_manager_->RemoveDraws(last_valid_draw_id);
      FallbackRasterize(node, target_info, *out_content_rect);
    }

    draw_state_.opacity = old_opacity;
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
    last_draw_id_ = draw_object_manager_->AddOffscreenDraw(object.Pass(),
        DrawObjectManager::kBlendNone, draw_type, render_target_,
        draw_state_.transform.MapRect(local_bounds));
  } else {
    last_draw_id_ = draw_object_manager_->AddOnscreenDraw(object.Pass(),
        DrawObjectManager::kBlendNone, draw_type, render_target_,
        draw_state_.transform.MapRect(local_bounds));
  }
}

void RenderTreeNodeVisitor::AddTransparentDraw(scoped_ptr<DrawObject> object,
    const math::RectF& local_bounds) {
  base::TypeId draw_type = object->GetTypeId();
  if (render_target_is_offscreen_) {
    last_draw_id_ = draw_object_manager_->AddOffscreenDraw(object.Pass(),
        DrawObjectManager::kBlendSrcAlpha, draw_type, render_target_,
        draw_state_.transform.MapRect(local_bounds));
  } else {
    last_draw_id_ = draw_object_manager_->AddOnscreenDraw(object.Pass(),
        DrawObjectManager::kBlendSrcAlpha, draw_type, render_target_,
        draw_state_.transform.MapRect(local_bounds));
  }
}

void RenderTreeNodeVisitor::AddExternalDraw(scoped_ptr<DrawObject> object,
    const math::RectF& world_bounds, base::TypeId draw_type) {
  if (render_target_is_offscreen_) {
    last_draw_id_ = draw_object_manager_->AddOffscreenDraw(object.Pass(),
        DrawObjectManager::kBlendExternal, draw_type, render_target_,
        world_bounds);
  } else {
    last_draw_id_ = draw_object_manager_->AddOnscreenDraw(object.Pass(),
        DrawObjectManager::kBlendExternal, draw_type, render_target_,
        world_bounds);
  }
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
