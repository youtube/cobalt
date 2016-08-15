/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/renderer/rasterizer/blitter/render_tree_node_visitor.h"

#include "base/bind.h"
#include "cobalt/math/matrix3_f.h"
#include "cobalt/math/rect.h"
#include "cobalt/math/rect_f.h"
#include "cobalt/math/size.h"
#include "cobalt/math/transform_2d.h"
#include "cobalt/math/vector2d_f.h"
#include "cobalt/renderer/rasterizer/blitter/cobalt_blitter_conversions.h"
#include "cobalt/renderer/rasterizer/blitter/image.h"
#include "cobalt/renderer/rasterizer/blitter/skia_blitter_conversions.h"
#include "cobalt/renderer/rasterizer/common/offscreen_render_coordinate_mapping.h"
#include "starboard/blitter.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageInfo.h"

#if SB_HAS(BLITTER)

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
using render_tree::SolidColorBrush;
using render_tree::ViewportFilter;

RenderTreeNodeVisitor::RenderTreeNodeVisitor(
    SbBlitterDevice device, SbBlitterContext context,
    const RenderState& render_state,
    skia::SoftwareRasterizer* software_rasterizer,
    SurfaceCacheDelegate* surface_cache_delegate,
    common::SurfaceCache* surface_cache)
    : software_rasterizer_(software_rasterizer),
      device_(device),
      context_(context),
      render_state_(render_state),
      surface_cache_delegate_(surface_cache_delegate),
      surface_cache_(surface_cache) {
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
  common::SurfaceCache::Block cache_block(surface_cache_, composition_node);
  if (cache_block.Cached()) return;

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

    scoped_push.emplace(&render_state_.bounds_stack,
                        RectFToRect(render_state_.transform.TransformRect(
                            viewport_filter.viewport())));
  }

  if (!filter_node->data().opacity_filter ||
      filter_node->data().opacity_filter->opacity() == 1.0f) {
    source->Accept(this);
  } else if (filter_node->data().opacity_filter->opacity() != 0.0f) {
    // If the opacity is set to 0, the contents are invisible and we are
    // trivially done.  However, if we made it into this branch, then
    // we know that opacity is in the range (0, 1), exclusive.
    float opacity = filter_node->data().opacity_filter->opacity();

    // Render our source subtree to an offscreen surface, and then we will
    // re-render it to our main render target with an alpha value applied to it.
    OffscreenRender offscreen_render = RenderToOffscreenSurface(source);
    if (!SbBlitterIsSurfaceValid(offscreen_render.surface)) {
      // This can happen if the output area of the source node is 0, in which
      // case we're trivially done.
      return;
    }

    SbBlitterSurfaceInfo offscreen_surface_info;
    CHECK(SbBlitterGetSurfaceInfo(offscreen_render.surface,
                                  &offscreen_surface_info));

    // Now blit our offscreen surface to our main render target with opacity
    // applied.
    SbBlitterSetBlending(context_, true);
    SbBlitterSetModulateBlitsWithColor(context_, true);
    SbBlitterSetColor(
        context_,
        SbBlitterColorFromRGBA(255, 255, 255, static_cast<int>(255 * opacity)));
    SbBlitterBlitRectToRect(
        context_, offscreen_render.surface,
        SbBlitterMakeRect(0, 0, offscreen_surface_info.width,
                          offscreen_surface_info.height),
        SbBlitterMakeRect(
            offscreen_render.position.x(), offscreen_render.position.y(),
            offscreen_surface_info.width, offscreen_surface_info.height));

    // Destroy our temporary offscreeen surface now that we are done with it.
    SbBlitterDestroySurface(offscreen_render.surface);
  }
}

void RenderTreeNodeVisitor::Visit(render_tree::ImageNode* image_node) {
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
  SbBlitterSetBlending(context_, true);
  SbBlitterSetModulateBlitsWithColor(context_, false);
  SbBlitterBlitRectToRectTiled(
      context_, blitter_image->surface(),
      RectFToBlitterRect(local_transform.TransformRect(RectF(image_size))),
      RectFToBlitterRect(render_state_.transform.TransformRect(
          image_node->data().destination_rect)));
}

void RenderTreeNodeVisitor::Visit(
    render_tree::MatrixTransformNode* matrix_transform_node) {
  common::SurfaceCache::Block cache_block(surface_cache_,
                                          matrix_transform_node);
  if (cache_block.Cached()) return;

  const Matrix3F& transform = matrix_transform_node->data().transform;

  if (transform.Get(1, 0) != 0 || transform.Get(0, 1) != 0) {
    // The Starboard Blitter API does not support rotations/shears, so we must
    // fallback to software in order to render the entire subtree.
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
  SbBlitterSetColor(context_, SbBlitterColorFromRGBA(0, 0, 0, 0));
  SbBlitterSetBlending(context_, false);
  SbBlitterFillRect(context_,
                    RectFToBlitterRect(render_state_.transform.TransformRect(
                        punch_through_video_node->data().rect)));
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
  SbBlitterSetColor(context, RenderTreeToBlitterColor(color));
  SbBlitterSetBlending(context, true);

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
  if (rect_node->data().background_brush) {
    if (rect_node->data().background_brush->GetTypeId() !=
        base::GetTypeId<SolidColorBrush>()) {
      // We can only render solid color rectangles, if we have a more
      // complicated brush (like gradients), fallback to software.
      RenderWithSoftwareRenderer(rect_node);
      return;
    }
  }

  const RectF& transformed_rect =
      render_state_.transform.TransformRect(rect_node->data().rect);

  // Render the solid color fill, if a brush exists.
  if (rect_node->data().background_brush) {
    SbBlitterSetBlending(context_, true);

    SolidColorBrush* solid_color_brush =
        base::polymorphic_downcast<SolidColorBrush*>(
            rect_node->data().background_brush.get());
    ColorRGBA color = solid_color_brush->color();

    SbBlitterSetColor(context_, RenderTreeToBlitterColor(color));

    SbBlitterFillRect(context_, RectFToBlitterRect(transformed_rect));
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

      RenderRectNodeBorder(context_, border.left.color, left_width, right_width,
                           top_width, bottom_width, transformed_rect);
    }
  }
}

void RenderTreeNodeVisitor::Visit(
    render_tree::RectShadowNode* rect_shadow_node) {
  RenderWithSoftwareRenderer(rect_shadow_node);
}

void RenderTreeNodeVisitor::Visit(render_tree::TextNode* text_node) {
  RenderWithSoftwareRenderer(text_node);
}

void RenderTreeNodeVisitor::RenderWithSoftwareRenderer(
    render_tree::Node* node) {
  common::SurfaceCache::Block cache_block(surface_cache_, node);
  if (cache_block.Cached()) return;

  common::OffscreenRenderCoordinateMapping coord_mapping =
      common::GetOffscreenRenderCoordinateMapping(
          node->GetBounds(), render_state_.transform.ToMatrix(),
          render_state_.bounds_stack.Top());
  if (coord_mapping.output_bounds.IsEmpty()) {
    // There's nothing to render if the bounds are 0.
    return;
  }

  DCHECK_GE(0.001f, std::abs(1.0f -
                             render_state_.transform.scale().x() *
                                 coord_mapping.output_post_scale.x()));
  DCHECK_GE(0.001f, std::abs(1.0f -
                             render_state_.transform.scale().y() *
                                 coord_mapping.output_post_scale.y()));

  SkImageInfo output_image_info = SkImageInfo::MakeN32(
      coord_mapping.output_bounds.width(), coord_mapping.output_bounds.height(),
      kPremul_SkAlphaType);

  // Allocate the pixels for the output image.
  SbBlitterPixelDataFormat blitter_pixel_data_format =
      SkiaToBlitterPixelFormat(output_image_info.colorType());
  DCHECK(SbBlitterIsPixelFormatSupportedByPixelData(device_,
                                                    blitter_pixel_data_format));
  SbBlitterPixelData pixel_data = SbBlitterCreatePixelData(
      device_, coord_mapping.output_bounds.width(),
      coord_mapping.output_bounds.height(), blitter_pixel_data_format);
  CHECK(SbBlitterIsPixelDataValid(pixel_data));

  SkBitmap bitmap;
  bitmap.installPixels(output_image_info,
                       SbBlitterGetPixelDataPointer(pixel_data),
                       SbBlitterGetPixelDataPitchInBytes(pixel_data));

  // Setup our Skia canvas that we will be using as the target for all CPU Skia
  // output.
  SkCanvas canvas(bitmap);
  canvas.clear(SkColorSetARGB(0, 0, 0, 0));

  Transform sub_render_transform(coord_mapping.sub_render_transform);

  // Now setup our canvas so that the render tree will be rendered to the top
  // left corner instead of at node->GetBounds().origin().
  canvas.translate(sub_render_transform.translate().x(),
                   sub_render_transform.translate().y());
  // And finally set the scale on our target canvas to match that of the current
  // |render_state_.transform|.
  canvas.scale(sub_render_transform.scale().x(),
               sub_render_transform.scale().y());

  // Use the Skia software rasterizer to render our subtree.
  software_rasterizer_->Submit(node, &canvas);

  // Create a surface out of the now populated pixel data.
  SbBlitterSurface surface =
      SbBlitterCreateSurfaceFromPixelData(device_, pixel_data);

  math::RectF output_rect = coord_mapping.output_bounds;
  output_rect.Offset(coord_mapping.output_pre_translate);
  output_rect.Offset(render_state_.transform.translate());

  // Finally blit the resulting surface to our actual render target.
  SbBlitterSetBlending(context_, true);
  SbBlitterSetModulateBlitsWithColor(context_, false);
  SbBlitterBlitRectToRect(
      context_, surface,
      SbBlitterMakeRect(0, 0, coord_mapping.output_bounds.width(),
                        coord_mapping.output_bounds.height()),
      RectFToBlitterRect(output_rect));

  // Clean up our temporary surface.
  SbBlitterDestroySurface(surface);
}

RenderTreeNodeVisitor::OffscreenRender
RenderTreeNodeVisitor::RenderToOffscreenSurface(render_tree::Node* node) {
  common::OffscreenRenderCoordinateMapping coord_mapping =
      common::GetOffscreenRenderCoordinateMapping(
          node->GetBounds(), render_state_.transform.ToMatrix(),
          render_state_.bounds_stack.Top());
  if (coord_mapping.output_bounds.IsEmpty()) {
    // There's nothing to render if the bounds are 0.
    OffscreenRender ret;
    ret.surface = kSbBlitterInvalidSurface;
    return ret;
  }
  DCHECK_GE(0.001f, std::abs(1.0f -
                             render_state_.transform.scale().x() *
                                 coord_mapping.output_post_scale.x()));
  DCHECK_GE(0.001f, std::abs(1.0f -
                             render_state_.transform.scale().y() *
                                 coord_mapping.output_post_scale.y()));

  SbBlitterSurface surface = SbBlitterCreateRenderTargetSurface(
      device_, coord_mapping.output_bounds.width(),
      coord_mapping.output_bounds.height(), kSbBlitterSurfaceFormatRGBA8);
  SbBlitterRenderTarget render_target =
      SbBlitterGetRenderTargetFromSurface(surface);

  SbBlitterSetRenderTarget(context_, render_target);

  // Clear the background to fully transparent before we begin rendering the
  // subtree.
  SbBlitterSetBlending(context_, false);
  SbBlitterSetColor(context_, SbBlitterColorFromRGBA(0, 0, 0, 0));
  SbBlitterFillRect(
      context_, RectToBlitterRect(Rect(coord_mapping.output_bounds.size())));

  // Render to the sub-surface.
  RenderTreeNodeVisitor sub_visitor(
      device_, context_,
      RenderState(
          render_target, Transform(coord_mapping.sub_render_transform),
          BoundsStack(context_, Rect(coord_mapping.output_bounds.size()))),
      software_rasterizer_, surface_cache_delegate_, surface_cache_);
  node->Accept(&sub_visitor);

  // Restore our original render target.
  SbBlitterSetRenderTarget(context_, render_state_.render_target);
  // Restore our context's scissor rectangle to what it was before we switched
  // render targets.
  render_state_.bounds_stack.UpdateContext();

  math::PointF output_point = coord_mapping.output_bounds.origin() +
                              coord_mapping.output_pre_translate +
                              render_state_.transform.translate();

  OffscreenRender ret;
  ret.position.SetPoint(output_point.x(), output_point.y());
  ret.surface = surface;

  return ret;
}

}  // namespace blitter
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // #if SB_HAS(BLITTER)
