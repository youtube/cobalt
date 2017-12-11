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
#include "cobalt/renderer/rasterizer/egl/draw_rect_border.h"
#include "cobalt/renderer/rasterizer/egl/draw_rect_color_texture.h"
#include "cobalt/renderer/rasterizer/egl/draw_rect_linear_gradient.h"
#include "cobalt/renderer/rasterizer/egl/draw_rect_radial_gradient.h"
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

bool IsOpaque(float opacity) {
  return opacity >= 0.999f;
}

math::RectF RoundOut(const math::RectF& input, float pad) {
  float left = std::floor(input.x() - pad);
  float right = std::ceil(input.right() + pad);
  float top = std::floor(input.y() - pad);
  float bottom = std::ceil(input.bottom() + pad);
  return math::RectF(left, top, right - left, bottom - top);
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

  // Use the cached contents' sub-pixel offset as the error rating.
  math::PointF desired_offset(
      desired_bounds.x() - std::floor(desired_bounds.x()),
      desired_bounds.y() - std::floor(desired_bounds.y()));
  math::PointF cached_offset(
      cached_bounds.x() - std::floor(cached_bounds.x()),
      cached_bounds.y() - std::floor(cached_bounds.y()));
  float error_x = std::abs(desired_offset.x() - cached_offset.x());
  float error_y = std::abs(desired_offset.y() - cached_offset.y());

  // Any sub-pixel offset is okay. Return something less than 1.
  return (error_x + error_y) * 0.49f;
}

float OffscreenTargetErrorFunction1D(float desired, const float& cached) {
  return std::abs(cached - desired);
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
      onscreen_render_target_(render_target),
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
        data.source->Accept(this);
        draw_state_.rounded_scissor_corners = base::nullopt;
        return;
      }
    } else if (cobalt::math::IsOnlyScaleAndTranslate(transform)) {
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
      draw_state_.scissor.Intersect(
          math::Rect::RoundFromRectF(transformed_viewport));
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
      float old_opacity = draw_state_.opacity;
      draw_state_.transform = math::Matrix3F::Identity();
      draw_state_.opacity *= filter_opacity;
      scoped_ptr<DrawObject> draw(new DrawRectColorTexture(graphics_state_,
          draw_state_, content_rect, kOpaqueWhite, texture,
          texcoord_transform, true /* clamp_texcoords */));
      AddDraw(draw.Pass(), content_rect, DrawObjectManager::kBlendSrcAlpha);
      draw_state_.opacity = old_opacity;
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

  if (!ImageNodeSupportedNatively(image_node)) {
    FallbackRasterize(image_node);
    return;
  }

  skia::Image* skia_image =
      base::polymorphic_downcast<skia::Image*>(data.source.get());
  bool clamp_texcoords = false;
  bool is_opaque = skia_image->IsOpaque() && IsOpaque(draw_state_.opacity);

  // Ensure any required backend processing is done to create the necessary
  // GPU resource.
  skia_image->EnsureInitialized();

  // Calculate matrix to transform texture coordinates according to the local
  // transform.
  math::Matrix3F texcoord_transform(math::Matrix3F::Identity());
  if (cobalt::math::IsOnlyScaleAndTranslate(data.local_transform)) {
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

    if (!hardware_image->GetTextureEGL()) {
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

  AddDraw(draw.Pass(), image_node->GetBounds(), is_opaque ?
          DrawObjectManager::kBlendNoneOrSrcAlpha :
          DrawObjectManager::kBlendSrcAlpha);
}

void RenderTreeNodeVisitor::Visit(
    render_tree::PunchThroughVideoNode* video_node) {
  if (!IsVisible(video_node->GetBounds())) {
    return;
  }

  const render_tree::PunchThroughVideoNode::Builder& data = video_node->data();
  math::RectF mapped_rect_float = draw_state_.transform.MapRect(data.rect);
  math::Rect mapped_rect = math::Rect::RoundFromRectF(mapped_rect_float);
  data.set_bounds_cb.Run(mapped_rect);

  scoped_ptr<DrawObject> draw(new DrawPolyColor(graphics_state_,
      draw_state_, data.rect, kTransparentBlack));
  AddDraw(draw.Pass(), video_node->GetBounds(), DrawObjectManager::kBlendNone);
}

void RenderTreeNodeVisitor::Visit(render_tree::RectNode* rect_node) {
  math::RectF node_bounds(rect_node->GetBounds());
  if (!IsVisible(node_bounds)) {
    return;
  }

  const render_tree::RectNode::Builder& data = rect_node->data();
  const scoped_ptr<render_tree::Brush>& brush = data.background_brush;
  math::RectF content_rect(data.rect);

  // Only solid color brushes are natively supported with rounded corners.
  if (data.rounded_corners && brush &&
      brush->GetTypeId() != base::GetTypeId<render_tree::SolidColorBrush>()) {
    FallbackRasterize(rect_node);
    return;
  }

  // Determine whether the RectNode's border attribute is supported. Update
  // the content and bounds if so.
  scoped_ptr<DrawRectBorder> draw_border;
  if (data.border) {
    draw_border.reset(new DrawRectBorder(graphics_state_, draw_state_,
        rect_node));
    if (draw_border->IsValid()) {
      content_rect = draw_border->GetContentRect();
      node_bounds = draw_border->GetBounds();
    }
  }
  const bool border_supported = !data.border || draw_border->IsValid();

  // Determine whether the RectNode's background brush is supported.
  base::TypeId brush_type = brush ? brush->GetTypeId() :
      base::GetTypeId<render_tree::Brush>();
  bool brush_is_solid_and_supported =
      brush_type == base::GetTypeId<render_tree::SolidColorBrush>();
  bool brush_is_linear_and_supported =
      brush_type == base::GetTypeId<render_tree::LinearGradientBrush>();
  bool brush_is_radial_and_supported =
      brush_type == base::GetTypeId<render_tree::RadialGradientBrush>();

  scoped_ptr<DrawRectRadialGradient> draw_radial;
  if (brush_is_radial_and_supported) {
    const render_tree::RadialGradientBrush* radial_brush =
        base::polymorphic_downcast<const render_tree::RadialGradientBrush*>
            (brush.get());
    draw_radial.reset(new DrawRectRadialGradient(graphics_state_, draw_state_,
        content_rect, *radial_brush,
        base::Bind(&RenderTreeNodeVisitor::GetScratchTexture,
                   base::Unretained(this), make_scoped_refptr(rect_node))));
    brush_is_radial_and_supported = draw_radial->IsValid();
  }

  const bool brush_supported = !brush || brush_is_solid_and_supported ||
      brush_is_linear_and_supported || brush_is_radial_and_supported;

  if (!brush_supported || !border_supported) {
    FallbackRasterize(rect_node);
    return;
  }

  if (draw_border) {
    bool content_rect_drawn = draw_border->DrawsContentRect();
    AddDraw(draw_border.PassAs<DrawObject>(), node_bounds,
            DrawObjectManager::kBlendSrcAlpha);
    if (content_rect_drawn) {
      return;
    }
  }

  // Handle drawing the content.
  if (brush_is_solid_and_supported) {
    const render_tree::SolidColorBrush* solid_brush =
        base::polymorphic_downcast<const render_tree::SolidColorBrush*>
            (brush.get());
    if (data.rounded_corners) {
      scoped_ptr<DrawObject> draw(new DrawRRectColor(graphics_state_,
          draw_state_, content_rect, *data.rounded_corners,
          solid_brush->color()));
      // Transparency is used for anti-aliasing.
      AddDraw(draw.Pass(), node_bounds, DrawObjectManager::kBlendSrcAlpha);
    } else {
      scoped_ptr<DrawObject> draw(new DrawPolyColor(graphics_state_,
          draw_state_, content_rect, solid_brush->color()));
      // Match the blending mode used by other rect node draws to allow
      // merging of the draw objects if possible.
      AddDraw(draw.Pass(), node_bounds,
              IsOpaque(draw_state_.opacity * solid_brush->color().a()) ?
                  DrawObjectManager::kBlendNoneOrSrcAlpha :
                  DrawObjectManager::kBlendSrcAlpha);
    }
  } else if (brush_is_linear_and_supported) {
    const render_tree::LinearGradientBrush* linear_brush =
        base::polymorphic_downcast<const render_tree::LinearGradientBrush*>
            (brush.get());
    scoped_ptr<DrawObject> draw(new DrawRectLinearGradient(graphics_state_,
        draw_state_, content_rect, *linear_brush));
    // The draw may use transparent pixels to ensure only pixels in the
    // specified area are modified.
    AddDraw(draw.Pass(), node_bounds, DrawObjectManager::kBlendSrcAlpha);
  } else if (brush_is_radial_and_supported) {
    // The colors in the brush may be transparent.
    AddDraw(draw_radial.PassAs<DrawObject>(), node_bounds,
            DrawObjectManager::kBlendSrcAlpha);
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
  render_tree::ColorRGBA shadow_color(data.shadow.color);

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
  AddDraw(draw.Pass(), node_bounds, DrawObjectManager::kBlendSrcAlpha);
}

void RenderTreeNodeVisitor::Visit(render_tree::TextNode* text_node) {
  if (!IsVisible(text_node->GetBounds())) {
    return;
  }

  FallbackRasterize(text_node);
}

// Get a scratch texture row region for use in rendering |node|.
void RenderTreeNodeVisitor::GetScratchTexture(
    scoped_refptr<render_tree::Node> node, float size,
    DrawObject::TextureInfo* out_texture_info) {
  // Get the cached texture region or create one.
  OffscreenTargetManager::TargetInfo target_info;
  bool cached = offscreen_target_manager_->GetCachedTarget(node,
      base::Bind(&OffscreenTargetErrorFunction1D, size), &target_info);
  if (!cached) {
    offscreen_target_manager_->AllocateCachedTarget(node, size, size,
        &target_info);
  }

  out_texture_info->texture = target_info.framebuffer == nullptr ? nullptr :
      target_info.framebuffer->GetColorTexture();
  out_texture_info->region = target_info.region;
  out_texture_info->is_new = !cached;
}

// Get a cached offscreen target to render |node|.
// |out_content_cached| is true if the node's contents are already cached in
//   the returned offscreen target.
// |out_target_info| describes the offscreen surface into which |node| should
//   be rendered.
// |out_content_rect| is the onscreen rect (already in screen space) where the
//   offscreen contents should be rendered.
void RenderTreeNodeVisitor::GetCachedTarget(
    scoped_refptr<render_tree::Node> node,
    bool* out_content_cached,
    OffscreenTargetManager::TargetInfo* out_target_info,
    math::RectF* out_content_rect) {
  math::RectF node_bounds(node->GetBounds());
  math::RectF mapped_bounds(draw_state_.transform.MapRect(node_bounds));
  if (mapped_bounds.IsEmpty()) {
    *out_content_cached = true;
    out_content_rect->SetRect(0.0f, 0.0f, 0.0f, 0.0f);
    return;
  }

  // Request a slightly larger render target than the calculated bounds. The
  // rasterizer may use an extra pixel along the edge of anti-aliased objects.
  *out_content_rect = RoundOut(mapped_bounds, 1.0f);

  // Get a suitable cache of the render tree node if one exists, or allocate
  // a new offscreen target if possible. The OffscreenTargetErrorFunction will
  // determine whether any caches are fit for use.

  // Do not cache rotating nodes since these will result in inappropriate
  // reuse of offscreen targets. Transforms that are rotations of angles in
  // the first quadrant will produce the same mapped rect sizes as angles in
  // the other 3 quadrants. Also avoid caching reflections.
  bool allow_caching =
      cobalt::math::IsOnlyScaleAndTranslate(draw_state_.transform) &&
      draw_state_.transform(0, 0) > 0.0f &&
      draw_state_.transform(1, 1) > 0.0f;
  if (allow_caching) {
    *out_content_cached = offscreen_target_manager_->GetCachedTarget(
        node, base::Bind(&OffscreenTargetErrorFunction, mapped_bounds),
        out_target_info);
    if (!(*out_content_cached)) {
      offscreen_target_manager_->AllocateCachedTarget(node,
          out_content_rect->size(), mapped_bounds, out_target_info);
    } else {
      // Maintain the size of the cached contents to avoid scaling artifacts.
      out_content_rect->set_size(out_target_info->region.size());
    }
  } else {
    *out_content_cached = false;
    out_target_info->framebuffer = nullptr;
  }

  // If no offscreen target could be allocated, then the render tree node will
  // be rendered directly onto the current render target. Use the draw scissor
  // to minimize what has to be drawn.
  if (out_target_info->framebuffer == nullptr) {
    out_content_rect->Intersect(draw_state_.scissor);
  }
}

void RenderTreeNodeVisitor::FallbackRasterize(
    scoped_refptr<render_tree::Node> node) {
  OffscreenTargetManager::TargetInfo target_info;
  math::RectF content_rect;

  // Retrieve the previously cached contents or try to allocate a cached
  // render target for the node.
  bool content_is_cached = false;
  GetCachedTarget(node, &content_is_cached, &target_info, &content_rect);

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
  if (IsOpaque(draw_state_.opacity)) {
    scoped_ptr<DrawObject> draw(new DrawRectTexture(graphics_state_,
        draw_state_, content_rect, texture, texcoord_transform));
    AddDraw(draw.Pass(), content_rect, DrawObjectManager::kBlendSrcAlpha);
  } else {
    scoped_ptr<DrawObject> draw(new DrawRectColorTexture(graphics_state_,
        draw_state_, content_rect, kOpaqueWhite, texture, texcoord_transform,
        false /* clamp_texcoords */));
    AddDraw(draw.Pass(), content_rect, DrawObjectManager::kBlendSrcAlpha);
  }

  draw_state_.transform = old_transform;
}

void RenderTreeNodeVisitor::FallbackRasterize(
    scoped_refptr<render_tree::Node> node,
    const OffscreenTargetManager::TargetInfo& target_info,
    const math::RectF& content_rect) {
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

  draw_object_manager_->AddBatchedExternalDraw(draw.Pass(), node->GetTypeId(),
      target_info.framebuffer, target_info.region);
}

// Add draw objects to render |node| to an offscreen render target at
//   100% opacity.
// |out_texture| and |out_texcoord_transform| describe the texture subregion
//   that will contain the result of rendering |node|.
// |out_content_rect| describes the onscreen rect (in screen space) which
//   should be used to render node's contents. This will be IsEmpty() if
//   nothing needs to be rendered.
void RenderTreeNodeVisitor::OffscreenRasterize(
    scoped_refptr<render_tree::Node> node,
    const backend::TextureEGL** out_texture,
    math::Matrix3F* out_texcoord_transform,
    math::RectF* out_content_rect) {
  // Check whether the node is visible.
  math::RectF mapped_bounds = draw_state_.transform.MapRect(node->GetBounds());
  math::RectF rounded_out_bounds = RoundOut(mapped_bounds, 0.0f);
  math::RectF clipped_bounds =
      math::IntersectRects(rounded_out_bounds, draw_state_.scissor);
  if (clipped_bounds.IsEmpty()) {
    out_content_rect->SetRect(0.0f, 0.0f, 0.0f, 0.0f);
    return;
  }

  // Allocate an uncached render target. The smallest render target needed is
  // the clipped bounds' size. However, this may be involved in an animation,
  // so use the mapped size limited to the maximum visible area. This increases
  // the chance that the render target can be recycled in the next frame.
  OffscreenTargetManager::TargetInfo target_info;
  math::SizeF target_size(rounded_out_bounds.size());
  target_size.SetToMin(onscreen_render_target_->GetSize());
  offscreen_target_manager_->AllocateUncachedTarget(target_size, &target_info);

  if (!target_info.framebuffer) {
    LOG(ERROR) << "Could not allocate framebuffer for offscreen rasterization.";
    out_content_rect->SetRect(0.0f, 0.0f, 0.0f, 0.0f);
    return;
  }

  // Only the clipped bounds will be rendered.
  DCHECK_GE(target_info.region.width(), clipped_bounds.width());
  DCHECK_GE(target_info.region.height(), clipped_bounds.height());
  target_info.region.set_size(clipped_bounds.size());
  *out_content_rect = clipped_bounds;
  *out_texture = target_info.framebuffer->GetColorTexture();
  *out_texcoord_transform = GetTexcoordTransform(target_info);

  // Push a new render state to rasterize to the offscreen render target.
  DrawObject::BaseState old_draw_state = draw_state_;
  SkCanvas* old_fallback_render_target = fallback_render_target_;
  backend::RenderTarget* old_render_target = render_target_;
  fallback_render_target_ = target_info.skia_canvas;
  render_target_ = target_info.framebuffer;

  // The contents of this new render target will be used in a draw call to the
  // previous render target. Inform the draw object manager of this dependency
  // so it can sort offscreen draws appropriately.
  draw_object_manager_->AddRenderTargetDependency(
      old_render_target, render_target_);

  // Draw the contents at 100% opacity. The caller will then draw the results
  // onto the main render target at the desired opacity.
  draw_state_.opacity = 1.0f;
  draw_state_.scissor = math::Rect::RoundFromRectF(target_info.region);

  // Clear the new render target. (Set the transform to the identity matrix so
  // the bounds for the DrawClear comes out as the entire target region.)
  draw_state_.transform = math::Matrix3F::Identity();
  scoped_ptr<DrawObject> draw_clear(new DrawClear(graphics_state_,
      draw_state_, kTransparentBlack));
  AddDraw(draw_clear.Pass(), target_info.region, DrawObjectManager::kBlendNone);

  // Adjust the transform to render into target_info.region.
  draw_state_.transform =
      math::TranslateMatrix(target_info.region.x() - clipped_bounds.x(),
                            target_info.region.y() - clipped_bounds.y()) *
      old_draw_state.transform;

  node->Accept(this);

  draw_state_ = old_draw_state;
  fallback_render_target_ = old_fallback_render_target;
  render_target_ = old_render_target;
}

bool RenderTreeNodeVisitor::IsVisible(const math::RectF& bounds) {
  math::RectF rect_bounds = draw_state_.transform.MapRect(bounds);
  math::RectF intersection = IntersectRects(rect_bounds, draw_state_.scissor);
  return !intersection.IsEmpty();
}

void RenderTreeNodeVisitor::AddDraw(scoped_ptr<DrawObject> object,
    const math::RectF& local_bounds, DrawObjectManager::BlendType blend_type) {
  base::TypeId draw_type = object->GetTypeId();
  math::RectF mapped_bounds = draw_state_.transform.MapRect(local_bounds);
  if (render_target_ != onscreen_render_target_) {
    last_draw_id_ = draw_object_manager_->AddOffscreenDraw(
        object.Pass(), blend_type, draw_type, render_target_, mapped_bounds);
  } else {
    last_draw_id_ = draw_object_manager_->AddOnscreenDraw(
        object.Pass(), blend_type, draw_type, render_target_, mapped_bounds);
  }
}

void RenderTreeNodeVisitor::AddExternalDraw(scoped_ptr<DrawObject> object,
    const math::RectF& world_bounds, base::TypeId draw_type) {
  if (render_target_ != onscreen_render_target_) {
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
