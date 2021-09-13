// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/configuration.h"
#if SB_API_VERSION >= 12 || SB_HAS(GLES2)

#include "cobalt/renderer/rasterizer/egl/render_tree_node_visitor.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/optional.h"
#include "base/trace_event/trace_event.h"
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
#include "cobalt/renderer/rasterizer/skia/skottie_animation.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

using common::utils::IsOpaque;
using common::utils::IsTransparent;

namespace {

typedef float ColorTransformMatrix[16];

using render_tree::kMultiPlaneImageFormatYUV3PlaneBT601FullRange;
using render_tree::kMultiPlaneImageFormatYUV3PlaneBT709;

const render_tree::ColorRGBA kOpaqueWhite(1.0f, 1.0f, 1.0f, 1.0f);
const render_tree::ColorRGBA kTransparentBlack(0.0f, 0.0f, 0.0f, 0.0f);

const ColorTransformMatrix kBT601FullRangeColorTransformMatrixInColumnMajor = {
    1.0f,   1.0f,      1.0f, 0.0f, 0.0f,   -0.34414f, 1.772f,  0.0f,
    1.402f, -0.71414f, 0.0f, 0.0f, -0.701, 0.529f,    -0.886f, 1.f,
};

const ColorTransformMatrix kBT709ColorTransformMatrixInColumnMajor = {
    1.164f, 1.164f,  1.164f, 0.0f, 0.0f,      -0.213f,  2.112f,    0.0f,
    1.793f, -0.533f, 0.0f,   0.0f, -0.96925f, 0.30025f, -1.12875f, 1.0f};

const ColorTransformMatrix& GetColorTransformMatrixInColumnMajor(
    render_tree::MultiPlaneImageFormat format) {
  const float* transform;
  if (format == kMultiPlaneImageFormatYUV3PlaneBT601FullRange) {
    return kBT601FullRangeColorTransformMatrixInColumnMajor;
  }
  DCHECK_EQ(format, kMultiPlaneImageFormatYUV3PlaneBT709);
  return kBT709ColorTransformMatrixInColumnMajor;
}

bool IsUniformSolidColor(const render_tree::Border& border) {
  return border.left.style == render_tree::kBorderStyleSolid &&
         border.right.style == render_tree::kBorderStyleSolid &&
         border.top.style == render_tree::kBorderStyleSolid &&
         border.bottom.style == render_tree::kBorderStyleSolid &&
         border.left.color == border.right.color &&
         border.top.color == border.bottom.color &&
         border.left.color == border.top.color;
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
      target.region.width() * scale_x, 0, target.region.x() * scale_x, 0,
      target.region.height() * scale_y, 1.0f + target.region.y() * scale_y, 0,
      0, 1);
}

math::SizeF GetTransformScale(const math::Matrix3F& transform) {
  math::PointF mapped_origin = transform * math::PointF(0, 0);
  math::PointF mapped_x = transform * math::PointF(1, 0);
  math::PointF mapped_y = transform * math::PointF(0, 1);
  math::Vector2dF mapped_vecx(mapped_x.x() - mapped_origin.x(),
                              mapped_x.y() - mapped_origin.y());
  math::Vector2dF mapped_vecy(mapped_y.x() - mapped_origin.x(),
                              mapped_y.y() - mapped_origin.y());
  return math::SizeF(mapped_vecx.Length(), mapped_vecy.Length());
}

bool ImageNodeSupportedNatively(render_tree::ImageNode* image_node) {
  // The image node may contain nothing. For example, when it represents a video
  // element before any frame is decoded.
  if (!image_node->data().source) {
    return true;
  }

  // Ensure any required backend processing is done to create the necessary
  // GPU resource. This must be done to verify whether the GPU resource can
  // be rendered by the shader.
  skia::Image* skia_image =
      base::polymorphic_downcast<skia::Image*>(image_node->data().source.get());
  skia_image->EnsureInitialized();

  if (skia_image->GetTypeId() == base::GetTypeId<skia::MultiPlaneImage>()) {
    skia::HardwareMultiPlaneImage* hardware_image =
        base::polymorphic_downcast<skia::HardwareMultiPlaneImage*>(skia_image);
    // TODO: Also enable rendering of three plane BT.709 images here.
    return hardware_image->GetFormat() ==
           kMultiPlaneImageFormatYUV3PlaneBT601FullRange;
  }
  if (skia_image->GetTypeId() == base::GetTypeId<skia::SinglePlaneImage>() &&
      skia_image->CanRenderInSkia()) {
    skia::HardwareFrontendImage* hardware_image =
        base::polymorphic_downcast<skia::HardwareFrontendImage*>(skia_image);
    // We don't yet handle alternative formats that piggyback on a GL_RGBA
    // texture.  This comes up, for example, with UYVY (YUV 422) textures.
    return !hardware_image->alternate_rgba_format();
  }
  return false;
}

bool RoundedRectContainsNonRoundedRect(
    const math::RectF& outer_rect,
    const render_tree::RoundedCorners& outer_corners,
    const math::RectF& inner_rect) {
  // Calculate the biggest (by area), the tallest, and the widest non-rounded
  // rects contained by the rounded rect. See if one of these contains the
  // inner rect.
  float inset_left = std::max(outer_corners.top_left.horizontal,
                              outer_corners.bottom_left.horizontal);
  float inset_right = std::max(outer_corners.top_right.horizontal,
                               outer_corners.bottom_right.horizontal);
  float inset_top = std::max(outer_corners.top_left.vertical,
                             outer_corners.top_right.vertical);
  float inset_bottom = std::max(outer_corners.bottom_left.vertical,
                                outer_corners.bottom_right.vertical);

  // Given an ellipse centered at the origin with radii A and B, the inscribed
  // rectangle with the largest area would extend A * sqrt(2) / 2 and
  // B * sqrt(2) / 2 units in each quadrant.
  const float kInsetScale = 0.2929f;  // 1 - sqrt(2) / 2
  math::RectF biggest_rect(outer_rect);
  biggest_rect.Inset(kInsetScale * inset_left, kInsetScale * inset_top,
                     kInsetScale * inset_right, kInsetScale * inset_bottom);

  math::RectF tallest_rect(outer_rect);
  tallest_rect.Inset(inset_left, 0, inset_right, 0);

  math::RectF widest_rect(outer_rect);
  widest_rect.Inset(0, inset_top, 0, inset_bottom);

  return biggest_rect.Contains(inner_rect) ||
         tallest_rect.Contains(inner_rect) || widest_rect.Contains(inner_rect);
}

bool RoundedRectContainsRoundedRect(
    const math::RectF& orect, const render_tree::RoundedCorners& ocorners,
    const math::RectF& irect, const render_tree::RoundedCorners& icorners) {
  // Use a quick and simple comparison. This may return false negatives,
  // but never return false positives.
  return orect.Contains(irect) &&
         ocorners.top_left.horizontal <= icorners.top_left.horizontal &&
         ocorners.top_left.vertical <= icorners.top_left.vertical &&
         ocorners.top_right.horizontal <= icorners.top_right.horizontal &&
         ocorners.top_right.vertical <= icorners.top_right.vertical &&
         ocorners.bottom_right.horizontal <= icorners.bottom_right.horizontal &&
         ocorners.bottom_right.vertical <= icorners.bottom_right.vertical &&
         ocorners.bottom_left.horizontal <= icorners.bottom_left.horizontal &&
         ocorners.bottom_left.vertical <= icorners.bottom_left.vertical;
}

bool RoundedViewportSupportedForSource(
    render_tree::Node* source, math::Vector2dF offset,
    const render_tree::ViewportFilter& filter) {
  base::TypeId source_type = source->GetTypeId();
  if (source_type == base::GetTypeId<render_tree::ImageNode>()) {
    // Image nodes are supported if the image can be rendered natively.
    render_tree::ImageNode* image_node =
        base::polymorphic_downcast<render_tree::ImageNode*>(source);
    return ImageNodeSupportedNatively(image_node);
  } else if (source_type == base::GetTypeId<render_tree::RectNode>()) {
    // Rect nodes are supported if they are fully contained by the filter
    // (i.e. the filter effectively does nothing).
    const render_tree::RectNode::Builder& rect_data =
        base::polymorphic_downcast<render_tree::RectNode*>(source)->data();
    math::RectF content_rect(rect_data.rect);
    content_rect.Offset(offset);
    if (rect_data.rounded_corners) {
      return RoundedRectContainsRoundedRect(
          filter.viewport(), filter.rounded_corners(), content_rect,
          *rect_data.rounded_corners);
    } else {
      return RoundedRectContainsNonRoundedRect(
          filter.viewport(), filter.rounded_corners(), content_rect);
    }
  } else if (source_type == base::GetTypeId<render_tree::CompositionNode>()) {
    // If this is a composition of valid sources, then rendering with a rounded
    // viewport is also supported.
    render_tree::CompositionNode* composition_node =
        base::polymorphic_downcast<render_tree::CompositionNode*>(source);
    typedef render_tree::CompositionNode::Children Children;
    const Children& children = composition_node->data().children();
    offset += composition_node->data().offset();

    for (Children::const_iterator iter = children.begin();
         iter != children.end(); ++iter) {
      if (!RoundedViewportSupportedForSource(iter->get(), offset, filter)) {
        return false;
      }
    }
    return true;
  } else if (source_type == base::GetTypeId<render_tree::FilterNode>()) {
    const render_tree::FilterNode::Builder& filter_data =
        base::polymorphic_downcast<render_tree::FilterNode*>(source)->data();
    // If the inner rounded viewport filter is contained by the outer
    // rounded viewport filter, then just render the inner filter.
    if (filter_data.viewport_filter &&
        filter_data.viewport_filter->has_rounded_corners() &&
        !filter_data.opacity_filter && !filter_data.blur_filter &&
        !filter_data.map_to_mesh_filter) {
      math::RectF viewport_rect = filter_data.viewport_filter->viewport();
      viewport_rect.Offset(offset);
      return RoundedRectContainsRoundedRect(
                 filter.viewport(), filter.rounded_corners(), viewport_rect,
                 filter_data.viewport_filter->rounded_corners()) &&
             RoundedViewportSupportedForSource(filter_data.source.get(),
                                               math::Vector2dF(),
                                               *filter_data.viewport_filter);
    }
    return false;
  }

  bool is_transform =
      source_type == base::GetTypeId<render_tree::MatrixTransformNode>() ||
      source_type == base::GetTypeId<render_tree::MatrixTransform3DNode>();
  if (!is_transform) {
    // Generic nodes are handled if they are fully contained in the filter
    // (i.e. the filter effectively does nothing). Transform nodes are not
    // handled because their interaction with the rounded viewport filter is
    // not handled.
    math::RectF node_bounds(source->GetBounds());
    node_bounds.Offset(offset);
    return RoundedRectContainsNonRoundedRect(
        filter.viewport(), filter.rounded_corners(), node_bounds);
  }

  return false;
}

// Return the error value of a given offscreen target cache entry.
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
  math::PointF cached_offset(cached_bounds.x() - std::floor(cached_bounds.x()),
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

RenderTreeNodeVisitor::RenderTreeNodeVisitor(
    GraphicsState* graphics_state, DrawObjectManager* draw_object_manager,
    OffscreenTargetManager* offscreen_target_manager,
    const FallbackRasterizeFunction& fallback_rasterize,
    SkCanvas* fallback_render_target, backend::RenderTarget* render_target,
    const math::Rect& content_rect)
    : graphics_state_(graphics_state),
      draw_object_manager_(draw_object_manager),
      offscreen_target_manager_(offscreen_target_manager),
      fallback_rasterize_(fallback_rasterize),
      fallback_render_target_(fallback_render_target),
      render_target_(render_target),
      onscreen_render_target_(render_target),
      last_draw_id_(0),
      fallback_rasterize_count_(0) {
  draw_state_.scissor.Intersect(content_rect);
}

void RenderTreeNodeVisitor::Visit(render_tree::ClearRectNode* clear_rect_node) {
  if (!IsVisible(clear_rect_node->GetBounds())) {
    return;
  }

  DCHECK_EQ(clear_rect_node->data().rect, clear_rect_node->GetBounds());
  AddClear(clear_rect_node->data().rect, clear_rect_node->data().color);
}

void RenderTreeNodeVisitor::Visit(
    render_tree::CompositionNode* composition_node) {
  const render_tree::CompositionNode::Builder& data = composition_node->data();
  math::Matrix3F old_transform = draw_state_.transform;
  draw_state_.transform =
      draw_state_.transform *
      math::TranslateMatrix(data.offset().x(), data.offset().y());
  draw_state_.rounded_scissor_rect.Offset(-data.offset());
  const render_tree::CompositionNode::Children& children = data.children();
  for (render_tree::CompositionNode::Children::const_iterator iter =
           children.begin();
       iter != children.end(); ++iter) {
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
  draw_state_.transform = draw_state_.transform * data.transform;
  data.source->Accept(this);
  draw_state_.transform = old_transform;
}

void RenderTreeNodeVisitor::Visit(render_tree::FilterNode* filter_node) {
  const render_tree::FilterNode::Builder& data = filter_node->data();

  // Handle viewport-only filter.
  if (data.viewport_filter && !data.opacity_filter && !data.blur_filter &&
      !data.map_to_mesh_filter) {
    const math::Matrix3F& transform = draw_state_.transform;
    if (data.viewport_filter->has_rounded_corners()) {
      // Certain source nodes have an optimized path for rendering inside
      // rounded viewports.
      if (RoundedViewportSupportedForSource(
              data.source.get(), math::Vector2dF(), *data.viewport_filter)) {
        math::RectF old_scissor_rect = draw_state_.rounded_scissor_rect;
        DrawObject::OptionalRoundedCorners old_scissor_corners =
            draw_state_.rounded_scissor_corners;
        draw_state_.rounded_scissor_rect = data.viewport_filter->viewport();
        draw_state_.rounded_scissor_corners =
            data.viewport_filter->rounded_corners();
        data.source->Accept(this);
        draw_state_.rounded_scissor_rect = old_scissor_rect;
        draw_state_.rounded_scissor_corners = old_scissor_corners;
        return;
      } else {
        // The source is too complex and must be rendered to a texture, then
        // that texture will be rendered using the rounded viewport.
        DrawObject::BaseState old_draw_state = draw_state_;

        // Render the source into an offscreen target at 100% opacity in its
        // own local space. To avoid magnification artifacts, scale up the
        // local space to match the source's size when rendered in world space.
        math::Matrix3F texcoord_transform(math::Matrix3F::Identity());
        math::RectF content_rect = data.source->GetBounds();
        const backend::TextureEGL* texture = nullptr;
        if (!IsVisible(content_rect)) {
          return;
        }
        draw_state_.opacity = 1.0f;

        math::SizeF scale = GetTransformScale(draw_state_.transform);
        draw_state_.transform = math::ScaleMatrix(
            std::max(1.0f, scale.width()), std::max(1.0f, scale.height()));

        // Don't clip the source since it is in its own local space.
        bool limit_to_screen_size = false;
        math::RectF mapped_content_rect =
            draw_state_.transform.MapRect(content_rect);
        draw_state_.scissor =
            math::Rect::RoundFromRectF(RoundOut(mapped_content_rect, 0.0f));
        draw_state_.rounded_scissor_corners.reset();

        OffscreenRasterize(data.source, limit_to_screen_size, &texture,
                           &texcoord_transform, &mapped_content_rect);
        if (mapped_content_rect.IsEmpty()) {
          draw_state_ = old_draw_state;
          return;
        }

        // Then render that offscreen target with the rounded filter.
        draw_state_ = old_draw_state;
        draw_state_.rounded_scissor_rect = data.viewport_filter->viewport();
        draw_state_.rounded_scissor_corners =
            data.viewport_filter->rounded_corners();
        std::unique_ptr<DrawObject> draw(new DrawRRectColorTexture(
            graphics_state_, draw_state_, content_rect, kOpaqueWhite, texture,
            texcoord_transform, true /* clamp_texcoords */));
        AddDraw(std::move(draw), content_rect,
                DrawObjectManager::kBlendSrcAlpha);

        draw_state_ = old_draw_state;
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
  if (data.opacity_filter && !data.viewport_filter && !data.blur_filter &&
      !data.map_to_mesh_filter) {
    const float filter_opacity = data.opacity_filter->opacity();
    if (IsTransparent(filter_opacity)) {
      // Totally transparent. Ignore the source.
      return;
    } else if (IsOpaque(filter_opacity)) {
      // Totally opaque. Render like normal.
      data.source->Accept(this);
      return;
    } else if (common::utils::NodeCanRenderWithOpacity(data.source.get())) {
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
      float old_opacity = draw_state_.opacity;
      draw_state_.opacity = 1.0f;
      // Since the offscreen target is mapped to world space, limit the target
      // to the screen size to avoid unnecessarily large offscreen targets.
      bool limit_to_screen_size = true;
      OffscreenRasterize(data.source, limit_to_screen_size, &texture,
                         &texcoord_transform, &content_rect);
      if (content_rect.IsEmpty()) {
        draw_state_.opacity = old_opacity;
        return;
      }

      // The content rect is already in screen space, so reset the transform.
      math::Matrix3F old_transform = draw_state_.transform;
      draw_state_.transform = math::Matrix3F::Identity();
      draw_state_.opacity = old_opacity * filter_opacity;
      std::unique_ptr<DrawObject> draw(new DrawRectColorTexture(
          graphics_state_, draw_state_, content_rect, kOpaqueWhite, texture,
          texcoord_transform, true /* clamp_texcoords */));
      AddDraw(std::move(draw), content_rect, DrawObjectManager::kBlendSrcAlpha);
      draw_state_.opacity = old_opacity;
      draw_state_.transform = old_transform;
      return;
    }
  }

  // Handle blur-only filter.
  if (data.blur_filter && !data.viewport_filter && !data.opacity_filter &&
      !data.map_to_mesh_filter) {
    if (data.blur_filter->blur_sigma() == 0.0f) {
      // Ignorable blur request. Render normally.
      data.source->Accept(this);
      return;
    }
  }

  // No filter.
  if (!data.opacity_filter && !data.viewport_filter && !data.blur_filter &&
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

  bool clamp_texcoords = false;
  skia::Image* skia_image =
      base::polymorphic_downcast<skia::Image*>(data.source.get());
  bool is_opaque = skia_image->IsOpaque() && IsOpaque(draw_state_.opacity);

  // Calculate matrix to transform texture coordinates according to the local
  // transform.
  math::Matrix3F texcoord_transform(math::Matrix3F::Identity());
  if (cobalt::math::IsOnlyScaleAndTranslate(data.local_transform)) {
    texcoord_transform(0, 0) =
        data.local_transform(0, 0) != 0 ? 1.0f / data.local_transform(0, 0) : 0;
    texcoord_transform(1, 1) =
        data.local_transform(1, 1) != 0 ? 1.0f / data.local_transform(1, 1) : 0;
    texcoord_transform(0, 2) =
        -texcoord_transform(0, 0) * data.local_transform(0, 2);
    texcoord_transform(1, 2) =
        -texcoord_transform(1, 1) * data.local_transform(1, 2);
    if (texcoord_transform(0, 0) < 1.0f || texcoord_transform(1, 1) < 1.0f) {
      // Edges may interpolate with texels outside the designated region.
      // Use a fragment shader that clamps the texture coordinates to prevent
      // that from happening.
      clamp_texcoords = true;
    }
  } else {
    texcoord_transform = data.local_transform.Inverse();

    if (texcoord_transform.IsZeros()) {
      DLOG(ERROR) << "data.local_transform is a non invertible matrix.";
      return;
    }
  }

  // Different shaders are used depending on whether the image has a single
  // plane or multiple planes.
  std::unique_ptr<DrawObject> draw;

  if (skia_image->GetTypeId() == base::GetTypeId<skia::SinglePlaneImage>()) {
    skia::HardwareFrontendImage* hardware_image =
        base::polymorphic_downcast<skia::HardwareFrontendImage*>(skia_image);
    if (!hardware_image->GetTextureEGL()) {
      return;
    }

    if (draw_state_.rounded_scissor_corners) {
      // Transparency is used to anti-alias the rounded rect.
      is_opaque = false;
      draw.reset(new DrawRRectColorTexture(
          graphics_state_, draw_state_, data.destination_rect, kOpaqueWhite,
          hardware_image->GetTextureEGL(), texcoord_transform,
          clamp_texcoords));
    } else if (clamp_texcoords || !is_opaque) {
      draw.reset(new DrawRectColorTexture(graphics_state_, draw_state_,
                                          data.destination_rect, kOpaqueWhite,
                                          hardware_image->GetTextureEGL(),
                                          texcoord_transform, clamp_texcoords));
    } else {
      draw.reset(new DrawRectTexture(
          graphics_state_, draw_state_, data.destination_rect,
          hardware_image->GetTextureEGL(), texcoord_transform));
    }
  } else if (skia_image->GetTypeId() ==
             base::GetTypeId<skia::MultiPlaneImage>()) {
    skia::HardwareMultiPlaneImage* hardware_image =
        base::polymorphic_downcast<skia::HardwareMultiPlaneImage*>(skia_image);
    auto image_format = hardware_image->GetFormat();
    auto y_texture_egl = hardware_image->GetTextureEGL(0);
    auto u_texture_egl = hardware_image->GetTextureEGL(1);
    auto v_texture_egl = hardware_image->GetTextureEGL(2);

    if (!y_texture_egl || !u_texture_egl || !v_texture_egl) {
      return;
    }

    const ColorTransformMatrix& color_transform_in_column_major =
        GetColorTransformMatrixInColumnMajor(image_format);

    if (draw_state_.rounded_scissor_corners) {
      // Transparency is used to anti-alias the rounded rect.
      is_opaque = false;
      draw.reset(new DrawRRectColorTexture(
          graphics_state_, draw_state_, data.destination_rect, kOpaqueWhite,
          y_texture_egl, u_texture_egl, v_texture_egl,
          color_transform_in_column_major, texcoord_transform,
          clamp_texcoords));
    } else if (clamp_texcoords || !is_opaque) {
      draw.reset(new DrawRectColorTexture(
          graphics_state_, draw_state_, data.destination_rect, kOpaqueWhite,
          y_texture_egl, u_texture_egl, v_texture_egl,
          color_transform_in_column_major, texcoord_transform,
          clamp_texcoords));
    } else {
      draw.reset(new DrawRectTexture(
          graphics_state_, draw_state_, data.destination_rect, y_texture_egl,
          u_texture_egl, v_texture_egl, color_transform_in_column_major,
          texcoord_transform));
    }
  } else {
    NOTREACHED();
    return;
  }

  AddDraw(std::move(draw), image_node->GetBounds(),
          is_opaque ? DrawObjectManager::kBlendNoneOrSrcAlpha
                    : DrawObjectManager::kBlendSrcAlpha);
}

void RenderTreeNodeVisitor::Visit(render_tree::LottieNode* lottie_node) {
  const render_tree::LottieNode::Builder& data = lottie_node->data();
  math::RectF content_rect = data.destination_rect;
  if (!IsVisible(content_rect)) {
    return;
  }

  skia::SkottieAnimation* animation =
      base::polymorphic_downcast<skia::SkottieAnimation*>(data.animation.get());
  if (animation->GetSize().GetArea() == 0) {
    return;
  }
  animation->SetAnimationTime(data.animation_time);

  // Get an offscreen target to cache the animation. Make the target big enough
  // to avoid scaling artifacts.
  math::SizeF scale = GetTransformScale(draw_state_.transform);
  math::SizeF mapped_size = content_rect.size();
  // Use a uniform scale to avoid impacting aspect ratio calculations.
  mapped_size.Scale(std::max(scale.width(), scale.height()));

  OffscreenTargetManager::TargetInfo target_info;
  if (!offscreen_target_manager_->GetCachedTarget(animation, mapped_size,
                                                  &target_info)) {
    // No pre-existing target was found. Allocate a new target.
    animation->ResetRenderCache();
    offscreen_target_manager_->AllocateCachedTarget(animation, mapped_size,
                                                    &target_info);
  }
  if (target_info.framebuffer == nullptr) {
    // Unable to allocate the render target for the animation cache.
    return;
  }

  // Add a draw call to update the cache.
  std::unique_ptr<DrawObject> update_cache(new DrawCallback(base::Bind(
      &skia::SkottieAnimation::UpdateRenderCache, base::Unretained(animation),
      base::Unretained(target_info.skia_canvas), target_info.region.size())));
  draw_object_manager_->AddBatchedExternalDraw(
      std::move(update_cache), lottie_node->GetTypeId(),
      target_info.framebuffer, target_info.region);

  // Add a draw call to render the cached animation to the current target.
  backend::TextureEGL* texture = target_info.framebuffer->GetColorTexture();
  math::Matrix3F texcoord_transform = GetTexcoordTransform(target_info);
  if (IsOpaque(draw_state_.opacity)) {
    std::unique_ptr<DrawObject> draw(
        new DrawRectTexture(graphics_state_, draw_state_, content_rect, texture,
                            texcoord_transform));
    AddDraw(std::move(draw), content_rect, DrawObjectManager::kBlendSrcAlpha);
  } else {
    std::unique_ptr<DrawObject> draw(new DrawRectColorTexture(
        graphics_state_, draw_state_, content_rect, kOpaqueWhite, texture,
        texcoord_transform, false /* clamp_texcoords */));
    AddDraw(std::move(draw), content_rect, DrawObjectManager::kBlendSrcAlpha);
  }
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

  DCHECK_EQ(data.rect, video_node->GetBounds());
  AddClear(data.rect, kTransparentBlack);
}

void RenderTreeNodeVisitor::Visit(render_tree::RectNode* rect_node) {
  math::RectF node_bounds(rect_node->GetBounds());
  if (!IsVisible(node_bounds)) {
    return;
  }

  const render_tree::RectNode::Builder& data = rect_node->data();
  const std::unique_ptr<render_tree::Brush>& brush = data.background_brush;
  base::Optional<render_tree::RoundedCorners> content_corners;
  math::RectF content_rect(data.rect);
  bool content_rect_drawn = false;

  if (data.rounded_corners) {
    content_corners = *data.rounded_corners;
  }

  // Only solid color brushes are natively supported with rounded corners.
  if (data.rounded_corners && brush &&
      brush->GetTypeId() != base::GetTypeId<render_tree::SolidColorBrush>()) {
    FallbackRasterize(rect_node);
    return;
  }

  // Determine whether the RectNode's border attribute is supported. Update
  // the content and bounds if so.
  std::unique_ptr<DrawObject> draw_border;
  if (data.border) {
    std::unique_ptr<DrawRectBorder> rect_border(
        new DrawRectBorder(graphics_state_, draw_state_, rect_node));
    if (rect_border->IsValid()) {
      node_bounds = rect_border->GetBounds();
      content_rect = rect_border->GetContentRect();
      content_rect_drawn = rect_border->DrawsContentRect();
      draw_border.reset(rect_border.release());
    } else if (data.rounded_corners) {
      // Handle the special case of uniform rounded borders.
      math::Vector2dF scale = math::GetScale2d(draw_state_.transform);
      bool border_is_subpixel = data.border->left.width * scale.x() < 1.0f ||
                                data.border->right.width * scale.x() < 1.0f ||
                                data.border->top.width * scale.y() < 1.0f ||
                                data.border->bottom.width * scale.y() < 1.0f;
      if (IsUniformSolidColor(*data.border) && !border_is_subpixel) {
        math::RectF border_rect(content_rect);
        render_tree::RoundedCorners border_corners = *data.rounded_corners;
        content_rect.Inset(data.border->left.width, data.border->top.width,
                           data.border->right.width, data.border->bottom.width);
        content_corners = data.rounded_corners->Inset(
            data.border->left.width, data.border->top.width,
            data.border->right.width, data.border->bottom.width);
        content_corners = content_corners->Normalize(content_rect);
        draw_border.reset(new DrawRectShadowSpread(
            graphics_state_, draw_state_, content_rect, content_corners,
            border_rect, border_corners, data.border->top.color));
      }
    }
  }
  const bool border_supported = !data.border || draw_border;

  // Determine whether the RectNode's background brush is supported.
  base::TypeId brush_type =
      brush ? brush->GetTypeId() : base::GetTypeId<render_tree::Brush>();
  bool brush_is_solid_and_supported =
      brush_type == base::GetTypeId<render_tree::SolidColorBrush>();
  bool brush_is_linear_and_supported =
      brush_type == base::GetTypeId<render_tree::LinearGradientBrush>();
  bool brush_is_radial_and_supported =
      brush_type == base::GetTypeId<render_tree::RadialGradientBrush>();

  std::unique_ptr<DrawRectRadialGradient> draw_radial;
  if (brush_is_radial_and_supported) {
    const render_tree::RadialGradientBrush* radial_brush =
        base::polymorphic_downcast<const render_tree::RadialGradientBrush*>(
            brush.get());
    draw_radial.reset(new DrawRectRadialGradient(
        graphics_state_, draw_state_, content_rect, *radial_brush,
        base::Bind(&RenderTreeNodeVisitor::GetScratchTexture,
                   base::Unretained(this), base::WrapRefCounted(rect_node))));
    brush_is_radial_and_supported = draw_radial->IsValid();
  }

  const bool brush_supported = !brush || brush_is_solid_and_supported ||
                               brush_is_linear_and_supported ||
                               brush_is_radial_and_supported;

  if (!brush_supported || !border_supported) {
    FallbackRasterize(rect_node);
    return;
  }

  if (draw_border) {
    AddDraw(std::move(draw_border), node_bounds,
            DrawObjectManager::kBlendSrcAlpha);
  }
  if (content_rect_drawn) {
    return;
  }

  // Handle drawing the content.
  if (brush_is_solid_and_supported) {
    const render_tree::SolidColorBrush* solid_brush =
        base::polymorphic_downcast<const render_tree::SolidColorBrush*>(
            brush.get());
    if (content_corners) {
      std::unique_ptr<DrawObject> draw(
          new DrawRRectColor(graphics_state_, draw_state_, content_rect,
                             *content_corners, solid_brush->color()));
      // Transparency is used for anti-aliasing.
      AddDraw(std::move(draw), node_bounds, DrawObjectManager::kBlendSrcAlpha);
    } else {
      std::unique_ptr<DrawObject> draw(new DrawPolyColor(
          graphics_state_, draw_state_, content_rect, solid_brush->color()));
      // Match the blending mode used by other rect node draws to allow
      // merging of the draw objects if possible.
      AddDraw(std::move(draw), node_bounds,
              IsOpaque(draw_state_.opacity * solid_brush->color().a())
                  ? DrawObjectManager::kBlendNoneOrSrcAlpha
                  : DrawObjectManager::kBlendSrcAlpha);
    }
  } else if (brush_is_linear_and_supported) {
    const render_tree::LinearGradientBrush* linear_brush =
        base::polymorphic_downcast<const render_tree::LinearGradientBrush*>(
            brush.get());
    std::unique_ptr<DrawObject> draw(new DrawRectLinearGradient(
        graphics_state_, draw_state_, content_rect, *linear_brush));
    // The draw may use transparent pixels to ensure only pixels in the
    // specified area are modified.
    AddDraw(std::move(draw), node_bounds, DrawObjectManager::kBlendSrcAlpha);
  } else if (brush_is_radial_and_supported) {
    // The colors in the brush may be transparent.
    AddDraw(std::unique_ptr<DrawObject>(draw_radial.release()), node_bounds,
            DrawObjectManager::kBlendSrcAlpha);
  }
}

void RenderTreeNodeVisitor::Visit(render_tree::RectShadowNode* shadow_node) {
  math::RectF node_bounds(shadow_node->GetBounds());
  if (!IsVisible(node_bounds)) {
    return;
  }

  const render_tree::RectShadowNode::Builder& data = shadow_node->data();
  base::Optional<render_tree::RoundedCorners> spread_corners =
      data.rounded_corners;

  std::unique_ptr<DrawObject> draw;
  render_tree::ColorRGBA shadow_color(data.shadow.color);

  math::RectF spread_rect(data.rect);
  spread_rect.Offset(data.shadow.offset);
  if (data.inset) {
    if (spread_corners) {
      spread_corners = spread_corners->Inset(data.spread, data.spread,
                                             data.spread, data.spread);
    }
    spread_rect.Inset(data.spread, data.spread);
    if (!spread_rect.IsEmpty() && data.shadow.blur_sigma > 0.0f) {
      draw.reset(new DrawRectShadowBlur(graphics_state_, draw_state_, data.rect,
                                        data.rounded_corners, spread_rect,
                                        spread_corners, shadow_color,
                                        data.shadow.blur_sigma, data.inset));
    } else {
      draw.reset(new DrawRectShadowSpread(
          graphics_state_, draw_state_, spread_rect, spread_corners, data.rect,
          data.rounded_corners, shadow_color));
    }
  } else {
    if (spread_corners) {
      spread_corners = spread_corners->Inset(-data.spread, -data.spread,
                                             -data.spread, -data.spread);
    }
    spread_rect.Outset(data.spread, data.spread);
    if (spread_rect.IsEmpty()) {
      // Negative spread shenanigans! Nothing to draw.
      return;
    }
    if (data.shadow.blur_sigma > 0.0f) {
      draw.reset(new DrawRectShadowBlur(graphics_state_, draw_state_, data.rect,
                                        data.rounded_corners, spread_rect,
                                        spread_corners, shadow_color,
                                        data.shadow.blur_sigma, data.inset));
    } else {
      draw.reset(new DrawRectShadowSpread(
          graphics_state_, draw_state_, data.rect, data.rounded_corners,
          spread_rect, spread_corners, shadow_color));
    }
  }

  // Transparency is used to skip pixels that are not shadowed.
  AddDraw(std::move(draw), node_bounds, DrawObjectManager::kBlendSrcAlpha);
}

void RenderTreeNodeVisitor::Visit(render_tree::TextNode* text_node) {
  if (!IsVisible(text_node->GetBounds())) {
    return;
  }

  FallbackRasterize(text_node);
}

int64_t RenderTreeNodeVisitor::GetFallbackRasterizeCount() {
  return fallback_rasterize_count_;
}

// Get a scratch texture row region for use in rendering |node|.
void RenderTreeNodeVisitor::GetScratchTexture(
    scoped_refptr<render_tree::Node> node, float size,
    DrawObject::TextureInfo* out_texture_info) {
  // Get the cached texture region or create one.
  OffscreenTargetManager::TargetInfo target_info;
  bool cached = offscreen_target_manager_->GetCachedTarget(
      node.get(), base::Bind(&OffscreenTargetErrorFunction1D, size),
      &target_info);
  if (!cached) {
    offscreen_target_manager_->AllocateCachedTarget(node.get(), size, size,
                                                    &target_info);
  }

  out_texture_info->texture = target_info.framebuffer == nullptr
                                  ? nullptr
                                  : target_info.framebuffer->GetColorTexture();
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
    scoped_refptr<render_tree::Node> node, bool* out_content_cached,
    OffscreenTargetManager::TargetInfo* out_target_info,
    math::RectF* out_content_rect) {
  math::RectF node_bounds(node->GetBounds());
  if (node->GetTypeId() == base::GetTypeId<render_tree::TextNode>()) {
    // Work around bug with text width calculation being slightly too small for
    // cursive text.
    node_bounds.set_width(node_bounds.width() * 1.01f);
  }
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
      draw_state_.transform(0, 0) > 0.0f && draw_state_.transform(1, 1) > 0.0f;
  if (allow_caching) {
    *out_content_cached = offscreen_target_manager_->GetCachedTarget(
        node.get(), base::Bind(&OffscreenTargetErrorFunction, mapped_bounds),
        out_target_info);
    if (!(*out_content_cached)) {
      offscreen_target_manager_->AllocateCachedTarget(
          node.get(), out_content_rect->size(), mapped_bounds, out_target_info);
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

  if (node->GetTypeId() != base::GetTypeId<render_tree::TextNode>()) {
    ++fallback_rasterize_count_;
  }

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
    base::Closure rasterize_callback =
        base::Bind(fallback_rasterize_, node, fallback_render_target_,
                   draw_state_.transform, content_rect, draw_state_.opacity,
                   kFallbackShouldFlush);
    std::unique_ptr<DrawObject> draw(new DrawCallback(rasterize_callback));
    AddExternalDraw(std::move(draw), content_rect, node->GetTypeId());
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
    std::unique_ptr<DrawObject> draw(
        new DrawRectTexture(graphics_state_, draw_state_, content_rect, texture,
                            texcoord_transform));
    AddDraw(std::move(draw), content_rect, DrawObjectManager::kBlendSrcAlpha);
  } else {
    std::unique_ptr<DrawObject> draw(new DrawRectColorTexture(
        graphics_state_, draw_state_, content_rect, kOpaqueWhite, texture,
        texcoord_transform, false /* clamp_texcoords */));
    AddDraw(std::move(draw), content_rect, DrawObjectManager::kBlendSrcAlpha);
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
  base::Closure rasterize_callback = base::Bind(
      fallback_rasterize_, node, target_info.skia_canvas, content_transform,
      target_info.region, draw_state_.opacity, rasterize_flags);
  std::unique_ptr<DrawObject> draw(new DrawCallback(rasterize_callback));

  draw_object_manager_->AddBatchedExternalDraw(
      std::move(draw), node->GetTypeId(), target_info.framebuffer,
      target_info.region);
}

// Add draw objects to render |node| to an offscreen render target.
// |limit_to_screen_size| specifies whether the allocated render target should
//   be limited to the onscreen render target size.
// |out_texture| and |out_texcoord_transform| describe the texture subregion
//   that will contain the result of rendering |node|.
// |out_content_rect| describes the onscreen rect (in screen space) which
//   should be used to render node's contents. This will be IsEmpty() if
//   nothing needs to be rendered.
void RenderTreeNodeVisitor::OffscreenRasterize(
    scoped_refptr<render_tree::Node> node, bool limit_to_screen_size,
    const backend::TextureEGL** out_texture,
    math::Matrix3F* out_texcoord_transform, math::RectF* out_content_rect) {
  // Map the target to world space. This avoids scaling artifacts if the target
  // is magnified.
  math::RectF mapped_bounds = draw_state_.transform.MapRect(node->GetBounds());

  if (!mapped_bounds.IsExpressibleAsRect()) {
    DLOG(WARNING) << "Invalid rectangle of " << mapped_bounds.ToString()
                  << " will not be rendered";
    return;
  }

  // Check whether the node is visible.
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
  if (limit_to_screen_size) {
    target_size.SetToMin(onscreen_render_target_->GetSize());
  }
  offscreen_target_manager_->AllocateUncachedTarget(target_size, &target_info);

  if (!target_info.framebuffer) {
    LOG(ERROR) << "Could not allocate framebuffer (" << target_size.width()
               << "x" << target_size.height()
               << ") for offscreen rasterization.";
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
  draw_object_manager_->AddRenderTargetDependency(old_render_target,
                                                  render_target_);

  // Clear the new render target. (Set the transform to the identity matrix so
  // the bounds for the DrawClear comes out as the entire target region.)
  draw_state_.scissor = math::Rect::RoundFromRectF(target_info.region);
  draw_state_.transform = math::Matrix3F::Identity();
  std::unique_ptr<DrawObject> draw_clear(
      new DrawClear(graphics_state_, draw_state_, kTransparentBlack));
  AddDraw(std::move(draw_clear), target_info.region,
          DrawObjectManager::kBlendNone);

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

void RenderTreeNodeVisitor::AddDraw(std::unique_ptr<DrawObject> object,
                                    const math::RectF& local_bounds,
                                    DrawObjectManager::BlendType blend_type) {
  base::TypeId draw_type = object->GetTypeId();
  math::RectF mapped_bounds = draw_state_.transform.MapRect(local_bounds);
  if (render_target_ != onscreen_render_target_) {
    last_draw_id_ = draw_object_manager_->AddOffscreenDraw(
        std::move(object), blend_type, draw_type, render_target_,
        mapped_bounds);
  } else {
    last_draw_id_ = draw_object_manager_->AddOnscreenDraw(
        std::move(object), blend_type, draw_type, render_target_,
        mapped_bounds);
  }
}

void RenderTreeNodeVisitor::AddExternalDraw(std::unique_ptr<DrawObject> object,
                                            const math::RectF& world_bounds,
                                            base::TypeId draw_type) {
  if (render_target_ != onscreen_render_target_) {
    last_draw_id_ = draw_object_manager_->AddOffscreenDraw(
        std::move(object), DrawObjectManager::kBlendExternal, draw_type,
        render_target_, world_bounds);
  } else {
    last_draw_id_ = draw_object_manager_->AddOnscreenDraw(
        std::move(object), DrawObjectManager::kBlendExternal, draw_type,
        render_target_, world_bounds);
  }
}

void RenderTreeNodeVisitor::AddClear(const math::RectF& rect,
                                     const render_tree::ColorRGBA& color) {
  // Check to see if we're simply trying to clear a non-transformed rectangle
  // on the screen with no filters or effects applied, and if so, issue a
  // clear command instead of a more general draw command, to give the GL
  // driver a better chance to optimize.
  if (!draw_state_.rounded_scissor_corners &&
      draw_state_.transform.IsIdentity() && IsOpaque(draw_state_.opacity)) {
    math::Rect old_scissor = draw_state_.scissor;
    draw_state_.scissor.Intersect(math::Rect::RoundFromRectF(rect));
    std::unique_ptr<DrawObject> draw_clear(
        new DrawClear(graphics_state_, draw_state_, color));
    AddDraw(std::move(draw_clear), rect, DrawObjectManager::kBlendNone);
    draw_state_.scissor = old_scissor;
  } else {
    std::unique_ptr<DrawObject> draw(
        new DrawPolyColor(graphics_state_, draw_state_, rect, color));
    AddDraw(std::move(draw), rect, DrawObjectManager::kBlendNone);
  }
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_API_VERSION >= 12 || SB_HAS(GLES2)
