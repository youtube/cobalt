// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/renderer/rasterizer/blitter/render_tree_node_visitor.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/math/matrix3_f.h"
#include "cobalt/math/rect.h"
#include "cobalt/math/rect_f.h"
#include "cobalt/math/size.h"
#include "cobalt/math/transform_2d.h"
#include "cobalt/math/vector2d_f.h"
#include "cobalt/renderer/rasterizer/blitter/cobalt_blitter_conversions.h"
#include "cobalt/renderer/rasterizer/blitter/image.h"
#include "cobalt/renderer/rasterizer/blitter/linear_gradient.h"
#include "cobalt/renderer/rasterizer/blitter/skia_blitter_conversions.h"
#include "cobalt/renderer/rasterizer/common/offscreen_render_coordinate_mapping.h"
#include "cobalt/renderer/rasterizer/common/utils.h"
#include "starboard/blitter.h"

#if SB_HAS(BLITTER)

// This define exists so that developers can quickly toggle it temporarily and
// obtain trace results for the render tree visit process here.  In general
// though it slows down tracing too much to leave it enabled.
#define ENABLE_RENDER_TREE_VISITOR_TRACING 0

#if ENABLE_RENDER_TREE_VISITOR_TRACING
#define TRACE_EVENT0_IF_ENABLED(x) TRACE_EVENT0("cobalt::renderer", x)
#else
#define TRACE_EVENT0_IF_ENABLED(x)
#endif

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace blitter {

using math::Matrix3F;
using math::Rect;
using math::RectF;
using math::Size;
using math::Vector2dF;
using render_tree::Border;
using render_tree::Brush;
using render_tree::ColorRGBA;
using render_tree::ColorStop;
using render_tree::ColorStopList;
using render_tree::LinearGradientBrush;
using render_tree::SolidColorBrush;
using render_tree::ViewportFilter;

RenderTreeNodeVisitor::RenderTreeNodeVisitor(
    SbBlitterDevice device, SbBlitterContext context,
    const RenderState& render_state, ScratchSurfaceCache* scratch_surface_cache,
    SurfaceCacheDelegate* surface_cache_delegate,
    common::SurfaceCache* surface_cache,
    CachedSoftwareRasterizer* software_surface_cache,
    LinearGradientCache* linear_gradient_cache)
    : device_(device),
      context_(context),
      render_state_(render_state),
      scratch_surface_cache_(scratch_surface_cache),
      surface_cache_delegate_(surface_cache_delegate),
      surface_cache_(surface_cache),
      software_surface_cache_(software_surface_cache),
      linear_gradient_cache_(linear_gradient_cache) {
  DCHECK_EQ(surface_cache_delegate_ == NULL, surface_cache_ == NULL);
  if (surface_cache_delegate_) {
    // Update our surface cache delegate to point to this render tree node
    // visitor and our canvas.
    surface_cache_scoped_context_.emplace(
        surface_cache_delegate_, &render_state_,
        base::Bind(&RenderTreeNodeVisitor::SetRenderState,
                   base::Unretained(this)));
  }
}

void RenderTreeNodeVisitor::Visit(
    render_tree::CompositionNode* composition_node) {
  TRACE_EVENT0_IF_ENABLED("Visit(CompositionNode)");

  const render_tree::CompositionNode::Children& children =
      composition_node->data().children();

  if (children.empty()) {
    return;
  }

  render_state_.transform.ApplyOffset(composition_node->data().offset());
  for (render_tree::CompositionNode::Children::const_iterator iter =
           children.begin();
       iter != children.end(); ++iter) {
    if (render_state_.transform.TransformRect((*iter)->GetBounds())
            .Intersects(RectF(render_state_.bounds_stack.Top()))) {
      (*iter)->Accept(this);
    }
  }
  render_state_.transform.ApplyOffset(-composition_node->data().offset());
}

void RenderTreeNodeVisitor::Visit(render_tree::FilterNode* filter_node) {
  TRACE_EVENT0_IF_ENABLED("Visit(FilterNode)");

  if (filter_node->data().blur_filter) {
    // The Starboard Blitter API does not support blur filters, so we fallback
    // to software for this.
    RenderWithSoftwareRenderer(filter_node);
    return;
  }

  render_tree::Node* source = filter_node->data().source.get();

  // Will be made active if a viewport filter is set.
  base::optional<BoundsStack::ScopedPush> scoped_push;

  if (filter_node->data().viewport_filter) {
    const ViewportFilter& viewport_filter =
        *filter_node->data().viewport_filter;

    if (viewport_filter.has_rounded_corners()) {
      RenderWithSoftwareRenderer(filter_node);
      return;
    }

    scoped_push.emplace(
        &render_state_.bounds_stack,
        cobalt::math::Rect::RoundFromRectF(
            render_state_.transform.TransformRect(viewport_filter.viewport())));
  }

  if (!filter_node->data().opacity_filter ||
      filter_node->data().opacity_filter->opacity() == 1.0f) {
    source->Accept(this);
  } else if (filter_node->data().opacity_filter->opacity() != 0.0f) {
    // If the opacity is set to 0, the contents are invisible and we are
    // trivially done.  However, if we made it into this branch, then
    // we know that opacity is in the range (0, 1), exclusive.
    float opacity = filter_node->data().opacity_filter->opacity();

    if (common::utils::NodeCanRenderWithOpacity(source)) {
      float original_opacity = render_state_.opacity;
      render_state_.opacity *= opacity;

      source->Accept(this);

      render_state_.opacity = original_opacity;
      return;
    }

    // Render our source subtree to an offscreen surface, and then we will
    // re-render it to our main render target with an alpha value applied to it.
    scoped_ptr<OffscreenRender> offscreen_render =
        RenderToOffscreenSurface(source);
    if (!offscreen_render) {
      // This can happen if the output area of the source node is 0, in which
      // case we're trivially done.
      return;
    }

    SbBlitterSurface offscreen_surface =
        offscreen_render->scratch_surface->GetSurface();

    // Now blit our offscreen surface to our main render target with opacity
    // applied.
    SbBlitterSetBlending(context_, true);
    SbBlitterSetModulateBlitsWithColor(context_, true);
    SbBlitterSetColor(
        context_,
        SbBlitterColorFromRGBA(255, 255, 255, static_cast<int>(255 * opacity)));
    SbBlitterBlitRectToRect(
        context_, offscreen_surface,
        SbBlitterMakeRect(0, 0, offscreen_render->destination_rect.width(),
                          offscreen_render->destination_rect.height()),
        RectFToBlitterRect(offscreen_render->destination_rect));
  }
}

void RenderTreeNodeVisitor::Visit(render_tree::ImageNode* image_node) {
  TRACE_EVENT0_IF_ENABLED("Visit(ImageNode)");
  // The image_node may contain nothing. For example, when it represents a video
  // or other kind of animated image element before any frame is decoded.
  if (!image_node->data().source) {
    return;
  }

  // All Blitter API images derive from skia::Image (so that they can be
  // compatible with the Skia software renderer), so we start here by casting
  // to skia::Image.
  skia::Image* skia_image =
      base::polymorphic_downcast<skia::Image*>(image_node->data().source.get());
  const Size& image_size = skia_image->GetSize();

  if (skia_image->GetTypeId() == base::GetTypeId<skia::MultiPlaneImage>()) {
    // Let software Skia deal with multiplane (e.g. YUV) image rendering.
    RenderWithSoftwareRenderer(image_node);
    return;
  }

  const Matrix3F& local_matrix = image_node->data().local_transform;
  if (local_matrix.Get(1, 0) != 0 || local_matrix.Get(0, 1) != 0) {
    // The Starboard Blitter API does not support local texture transforms that
    // involve rotations or shears, so we must fallback to software to perform
    // these.
    RenderWithSoftwareRenderer(image_node);
    return;
  }

  // All single-plane images are guaranteed to be of type SinglePlaneImage.
  SinglePlaneImage* blitter_image =
      base::polymorphic_downcast<SinglePlaneImage*>(skia_image);

  // Ensure any required backend processing is done to create the necessary
  // GPU resource.
  if (blitter_image->EnsureInitialized()) {
    // Restore the original render target, since it is possible that rendering
    // took place to another render target in this call.
    SbBlitterSetRenderTarget(context_, render_state_.render_target);
  }

  // Apply the local image coordinate transform to the source rectangle.  Note
  // that the render tree local transform matrix is normalized, but the Blitter
  // API source rectangle is specified in pixel units, so we must multiply the
  // offset by |image_size| in order to get the correct values.
  Transform local_transform;
  local_transform.ApplyScale(
      Vector2dF(1.0f / local_matrix.Get(0, 0), 1.0f / local_matrix.Get(1, 1)));
  local_transform.ApplyOffset(
      Vector2dF(-local_matrix.Get(0, 2) * image_size.width(),
                -local_matrix.Get(1, 2) * image_size.height()));

  // Render the image.
  if (render_state_.opacity < 1.0f) {
    SbBlitterSetBlending(context_, true);
    SbBlitterSetModulateBlitsWithColor(context_, true);
    SbBlitterSetColor(
        context_,
        SbBlitterColorFromRGBA(255, 255, 255,
                               static_cast<int>(255 * render_state_.opacity)));
  } else {
    SbBlitterSetBlending(context_, !skia_image->IsOpaque());
    SbBlitterSetModulateBlitsWithColor(context_, false);
  }

  SbBlitterBlitRectToRectTiled(
      context_, blitter_image->surface(),
      RectFToBlitterRect(local_transform.TransformRect(RectF(image_size))),
      RectFToBlitterRect(render_state_.transform.TransformRect(
          image_node->data().destination_rect)));
}

void RenderTreeNodeVisitor::Visit(
    render_tree::MatrixTransform3DNode* matrix_transform_3d_node) {
  TRACE_EVENT0_IF_ENABLED("Visit(MatrixTransform3DNode)");
  // Ignore the 3D transform matrix, it cannot be implemented within the Blitter
  // API.
  matrix_transform_3d_node->data().source->Accept(this);
}

void RenderTreeNodeVisitor::Visit(
    render_tree::MatrixTransformNode* matrix_transform_node) {
  TRACE_EVENT0_IF_ENABLED("Visit(MatrixTransformNode)");

  const Matrix3F& transform = matrix_transform_node->data().transform;

  if (transform.Get(1, 0) != 0 || transform.Get(0, 1) != 0 ||
      transform.Get(0, 0) < 0 || transform.Get(1, 1) < 0) {
    // The Starboard Blitter API does not support rotations/shears/flips, so we
    // must fallback to software in order to render the entire subtree.
    RenderWithSoftwareRenderer(matrix_transform_node);
    return;
  }

  Transform old_transform(render_state_.transform);

  render_state_.transform.ApplyOffset(
      Vector2dF(transform.Get(0, 2), transform.Get(1, 2)));
  render_state_.transform.ApplyScale(
      Vector2dF(transform.Get(0, 0), transform.Get(1, 1)));

  matrix_transform_node->data().source->Accept(this);

  render_state_.transform = old_transform;
}

void RenderTreeNodeVisitor::Visit(
    render_tree::PunchThroughVideoNode* punch_through_video_node) {
  TRACE_EVENT0_IF_ENABLED("Visit(PunchThroughVideoNode)");

  SbBlitterRect blitter_rect =
      RectFToBlitterRect(render_state_.transform.TransformRect(
          punch_through_video_node->data().rect));

  punch_through_video_node->data().set_bounds_cb.Run(
      math::Rect(blitter_rect.x, blitter_rect.y, blitter_rect.width,
                 blitter_rect.height));

  SbBlitterSetColor(context_, SbBlitterColorFromRGBA(0, 0, 0, 0));
  SbBlitterSetBlending(context_, false);
  SbBlitterFillRect(context_, blitter_rect);
}

namespace {
bool AllBorderSidesHaveSameColorAndStyle(const Border& border) {
  return border.left.style == border.right.style &&
         border.left.style == border.top.style &&
         border.left.style == border.bottom.style &&
         border.left.color == border.right.color &&
         border.left.color == border.top.color &&
         border.left.color == border.bottom.color;
}

SbBlitterColor RenderTreeToBlitterColor(const ColorRGBA& color) {
  return SbBlitterColorFromRGBA(color.r() * 255, color.g() * 255,
                                color.b() * 255, color.a() * 255);
}

void RenderRectNodeBorder(SbBlitterContext context, ColorRGBA color, float left,
                          float right, float top, float bottom,
                          const RectF& rect) {
  SbBlitterColor blitter_color = RenderTreeToBlitterColor(color);
  SbBlitterSetColor(context, blitter_color);

  if (SbBlitterAFromColor(blitter_color) < 255) {
    SbBlitterSetBlending(context, true);
  } else {
    SbBlitterSetBlending(context, false);
  }

  // We draw four rectangles, one for each border edge.  They have the following
  // layout:
  //
  //  -------------
  //  | |   T     |
  //  | |---------|
  //  | |       | |
  //  |L|       |R|
  //  | |       | |
  //  |---------| |
  //  |     B   | |
  //  -------------

  // Left
  SbBlitterFillRect(context, RectFToBlitterRect(RectF(rect.x(), rect.y(), left,
                                                      rect.height() - bottom)));
  // Bottom
  SbBlitterFillRect(context,
                    RectFToBlitterRect(RectF(rect.x(), rect.bottom() - bottom,
                                             rect.width() - right, bottom)));
  // Right
  SbBlitterFillRect(
      context, RectFToBlitterRect(RectF(rect.right() - right, rect.y() + top,
                                        right, rect.height() - top)));
  // Top
  SbBlitterFillRect(
      context, RectFToBlitterRect(
                   RectF(rect.x() + left, rect.y(), rect.width() - left, top)));
}

}  // namespace

void RenderTreeNodeVisitor::Visit(render_tree::RectNode* rect_node) {
  TRACE_EVENT0_IF_ENABLED("Visit(RectNode)");

  if (rect_node->data().rounded_corners) {
    // We can't render rounded corners through the Blitter API.
    RenderWithSoftwareRenderer(rect_node);
    return;
  }
  if (rect_node->data().border) {
    if (!AllBorderSidesHaveSameColorAndStyle(*rect_node->data().border)) {
      // If the borders don't all have the same color and style, we can't
      // render them with the Blitter API (because we can't just render 4
      // rectangles), so fallback to software.
      RenderWithSoftwareRenderer(rect_node);
      return;
    }
  }

  const RectF& transformed_rect =
      render_state_.transform.TransformRect(rect_node->data().rect);

  if (rect_node->data().background_brush) {
    base::TypeId background_brush_typeid(
        rect_node->data().background_brush->GetTypeId());

    if (background_brush_typeid == base::GetTypeId<SolidColorBrush>()) {
      // Render the solid color fill, if a brush exists.
      SolidColorBrush* solid_color_brush =
          base::polymorphic_downcast<SolidColorBrush*>(
              rect_node->data().background_brush.get());
      ColorRGBA color = solid_color_brush->color();

      if (render_state_.opacity < 1.0f) {
        color.set_a(color.a() * render_state_.opacity);
      }

      SbBlitterSetBlending(context_, color.a() < 1.0f);
      SbBlitterSetColor(context_, RenderTreeToBlitterColor(color));

      SbBlitterFillRect(context_, RectFToBlitterRect(transformed_rect));
    } else if (background_brush_typeid ==
               base::GetTypeId<LinearGradientBrush>()) {
      DCHECK(rect_node != NULL);
      bool rendered_gradient = RenderLinearGradient(
          device_, context_, render_state_, *rect_node, linear_gradient_cache_);
      if (!rendered_gradient) {
        RenderWithSoftwareRenderer(rect_node);
        return;
      }
    } else {
      // If we have a more complicated brush fallback to software.
      RenderWithSoftwareRenderer(rect_node);
      return;
    }
  }

  // Render the border, if it exists.
  if (rect_node->data().border) {
    const Border& border = *rect_node->data().border;
    DCHECK(AllBorderSidesHaveSameColorAndStyle(border));

    if (border.left.style != render_tree::kBorderStyleNone) {
      DCHECK_EQ(render_tree::kBorderStyleSolid, border.left.style);

      float left_width =
          border.left.width * render_state_.transform.scale().x();
      float right_width =
          border.right.width * render_state_.transform.scale().x();
      float top_width = border.top.width * render_state_.transform.scale().y();
      float bottom_width =
          border.bottom.width * render_state_.transform.scale().y();

      ColorRGBA color = border.left.color;
      if (render_state_.opacity < 1.0f) {
        color.set_a(color.a() * render_state_.opacity);
      }
      RenderRectNodeBorder(context_, color, left_width, right_width, top_width,
                           bottom_width, transformed_rect);
    }
  }
}

void RenderTreeNodeVisitor::Visit(
    render_tree::RectShadowNode* rect_shadow_node) {
  TRACE_EVENT0_IF_ENABLED("Visit(RectShadowNode)");

  RenderWithSoftwareRenderer(rect_shadow_node);
}

void RenderTreeNodeVisitor::Visit(render_tree::TextNode* text_node) {
  TRACE_EVENT0_IF_ENABLED("Visit(TextNode)");

  RenderWithSoftwareRenderer(text_node);
}

void RenderTreeNodeVisitor::RenderWithSoftwareRenderer(
    render_tree::Node* node) {
  TRACE_EVENT0("cobalt::renderer", "RenderWithSoftwareRenderer()");
  CachedSoftwareRasterizer::SurfaceReference software_surface_reference(
      software_surface_cache_, node, render_state_.transform);
  CachedSoftwareRasterizer::Surface software_surface =
      software_surface_reference.surface();
  if (!SbBlitterIsSurfaceValid(software_surface.surface)) {
    return;
  }

  Transform apply_transform(render_state_.transform);
  apply_transform.ApplyScale(software_surface.coord_mapping.output_post_scale);
  math::RectF output_rectf = apply_transform.TransformRect(
      software_surface.coord_mapping.output_bounds);
  // We can simulate a "pre-multiply" by translation by offsetting the final
  // output rectangle by the pre-translate, effectively resulting in the
  // translation being applied last, as intended.
  output_rectf.Offset(software_surface.coord_mapping.output_pre_translate);
  SbBlitterRect output_blitter_rect = RectFToBlitterRect(output_rectf);

  SbBlitterSetBlending(context_, true);

  if (render_state_.opacity < 1.0f) {
    SbBlitterSetModulateBlitsWithColor(context_, true);
    SbBlitterSetColor(
        context_,
        SbBlitterColorFromRGBA(255, 255, 255,
                               static_cast<int>(255 * render_state_.opacity)));
  } else {
    SbBlitterSetModulateBlitsWithColor(context_, false);
  }

// Blit the software rasterized surface to our actual render target.
#if defined(ENABLE_DEBUG_CONSOLE)
  if (render_state_.highlight_software_draws && software_surface.created) {
    SbBlitterSetColor(context_, SbBlitterColorFromRGBA(0, 255, 0, 255));
    SbBlitterFillRect(context_, output_blitter_rect);
  } else  // NOLINT(readability/braces)
#endif    // defined(ENABLE_DEBUG_CONSOLE)
  {
    TRACE_EVENT0("cobalt::renderer", "SbBlitterBlitRectToRect()");
    SbBlitterBlitRectToRect(
        context_, software_surface.surface,
        SbBlitterMakeRect(
            0, 0, software_surface.coord_mapping.output_bounds.width(),
            software_surface.coord_mapping.output_bounds.height()),
        output_blitter_rect);
  }
}

scoped_ptr<RenderTreeNodeVisitor::OffscreenRender>
RenderTreeNodeVisitor::RenderToOffscreenSurface(render_tree::Node* node) {
  TRACE_EVENT0_IF_ENABLED("RenderToOffscreenSurface()");

  common::OffscreenRenderCoordinateMapping coord_mapping =
      common::GetOffscreenRenderCoordinateMapping(
          node->GetBounds(), render_state_.transform.ToMatrix(),
          render_state_.bounds_stack.Top());
  if (coord_mapping.output_bounds.IsEmpty()) {
    // There's nothing to render if the bounds are 0.
    return scoped_ptr<OffscreenRender>();
  }
  DCHECK_GE(0.001f, std::abs(1.0f -
                             render_state_.transform.scale().x() *
                                 coord_mapping.output_post_scale.x()));
  DCHECK_GE(0.001f, std::abs(1.0f -
                             render_state_.transform.scale().y() *
                                 coord_mapping.output_post_scale.y()));

  scoped_ptr<CachedScratchSurface> scratch_surface(new CachedScratchSurface(
      scratch_surface_cache_, coord_mapping.output_bounds.size()));
  SbBlitterSurface surface = scratch_surface->GetSurface();
  if (!SbBlitterIsSurfaceValid(surface)) {
    return scoped_ptr<RenderTreeNodeVisitor::OffscreenRender>();
  }

  SbBlitterRenderTarget render_target =
      SbBlitterGetRenderTargetFromSurface(surface);

  SbBlitterSetRenderTarget(context_, render_target);

  // Render to the sub-surface.
  RenderTreeNodeVisitor sub_visitor(
      device_, context_,
      RenderState(
          render_target, Transform(coord_mapping.sub_render_transform),
          BoundsStack(context_, Rect(coord_mapping.output_bounds.size()))),
      scratch_surface_cache_, surface_cache_delegate_, surface_cache_,
      software_surface_cache_, linear_gradient_cache_);
  node->Accept(&sub_visitor);

  // Restore our original render target.
  SbBlitterSetRenderTarget(context_, render_state_.render_target);
  // Restore our context's scissor rectangle to what it was before we switched
  // render targets.
  render_state_.bounds_stack.UpdateContext();

  math::PointF output_point = coord_mapping.output_bounds.origin() +
                              coord_mapping.output_pre_translate +
                              render_state_.transform.translate();

  scoped_ptr<OffscreenRender> ret(new OffscreenRender());
  ret->destination_rect =
      math::RectF(output_point, coord_mapping.output_bounds.size());
  ret->scratch_surface = scratch_surface.Pass();

  return ret.Pass();
}

}  // namespace blitter
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // #if SB_HAS(BLITTER)
