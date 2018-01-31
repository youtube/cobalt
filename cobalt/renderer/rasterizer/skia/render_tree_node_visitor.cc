// Copyright 2014 Google Inc. All Rights Reserved.
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

#include "cobalt/renderer/rasterizer/skia/render_tree_node_visitor.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "base/debug/trace_event.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/math/rect.h"
#include "cobalt/math/rect_f.h"
#include "cobalt/math/size.h"
#include "cobalt/math/size_conversions.h"
#include "cobalt/math/transform_2d.h"
#include "cobalt/render_tree/border.h"
#include "cobalt/render_tree/brush_visitor.h"
#include "cobalt/render_tree/color_rgba.h"
#include "cobalt/render_tree/composition_node.h"
#include "cobalt/render_tree/filter_node.h"
#include "cobalt/render_tree/image_node.h"
#include "cobalt/render_tree/matrix_transform_node.h"
#include "cobalt/render_tree/rect_node.h"
#include "cobalt/render_tree/rect_shadow_node.h"
#include "cobalt/render_tree/rounded_corners.h"
#include "cobalt/render_tree/text_node.h"
#include "cobalt/renderer/rasterizer/common/offscreen_render_coordinate_mapping.h"
#include "cobalt/renderer/rasterizer/common/utils.h"
#include "cobalt/renderer/rasterizer/skia/cobalt_skia_type_conversions.h"
#include "cobalt/renderer/rasterizer/skia/font.h"
#include "cobalt/renderer/rasterizer/skia/glyph_buffer.h"
#include "cobalt/renderer/rasterizer/skia/image.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/effects/SkNV122RGBShader.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/effects/SkYUV2RGBShader.h"
#include "cobalt/renderer/rasterizer/skia/software_image.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "third_party/skia/include/core/SkClipOp.h"
#include "third_party/skia/include/core/SkFilterQuality.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "third_party/skia/include/core/SkRRect.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"
#include "third_party/skia/include/effects/SkBlurMaskFilter.h"
#include "third_party/skia/include/effects/SkDropShadowImageFilter.h"
#include "third_party/skia/include/effects/SkGradientShader.h"

// Setting this define to 1 will enable TRACE_EVENT calls to be made from
// all render node visitations.  It is by default set to 0 because it generates
// enough calls that performance is affected.
#define ENABLE_RENDER_TREE_VISITOR_TRACING 0

// Setting this define to 1 will filter the trace events enabled by
// ENABLE_RENDER_TREE_VISITOR_TRACING so that trace events will only be
// recorded for leaf nodes.
#define FILTER_RENDER_TREE_VISITOR_TRACING 1

// Setting this define to 1 will result in a SkCanvas::flush() call being made
// after each node is visited.  This is useful for debugging Skia code since
// otherwise the actual execution of Skia commands will likely be queued and
// deferred for later execution by skia.
#define ENABLE_FLUSH_AFTER_EVERY_NODE 0

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

RenderTreeNodeVisitor::RenderTreeNodeVisitor(
    SkCanvas* render_target,
    const CreateScratchSurfaceFunction* create_scratch_surface_function,
    const base::Closure& reset_skia_context_function,
    const RenderImageFallbackFunction& render_image_fallback_function,
    const RenderImageWithMeshFallbackFunction& render_image_with_mesh_function,
    Type visitor_type)
    : draw_state_(render_target),
      create_scratch_surface_function_(create_scratch_surface_function),
      visitor_type_(visitor_type),
      reset_skia_context_function_(reset_skia_context_function),
      render_image_fallback_function_(render_image_fallback_function),
      render_image_with_mesh_function_(render_image_with_mesh_function) {}

namespace {
// Returns whether the specified node is within the canvas' bounds or not.
bool NodeIsWithinCanvasBounds(const SkMatrix& total_matrix,
                              const SkRect& canvas_bounds,
                              const render_tree::Node& node) {
  SkRect sk_child_bounds(CobaltRectFToSkiaRect(node.GetBounds()));
  SkRect sk_child_bounds_absolute;

  // Use the total matrix to compute the node's bounding rectangle in the
  // canvas' coordinates.
  total_matrix.mapRect(&sk_child_bounds_absolute, sk_child_bounds);

  // Return if the node's bounding rectangle intersects with the canvas'
  // bounding rectangle.
  return sk_child_bounds_absolute.intersect(canvas_bounds);
}
}  // namespace

void RenderTreeNodeVisitor::Visit(
    render_tree::CompositionNode* composition_node) {
#if ENABLE_RENDER_TREE_VISITOR_TRACING && !FILTER_RENDER_TREE_VISITOR_TRACING
  TRACE_EVENT0("cobalt::renderer", "Visit(CompositionNode)");
#endif

  const render_tree::CompositionNode::Children& children =
      composition_node->data().children();

  if (children.empty()) {
    return;
  }

  draw_state_.render_target->translate(composition_node->data().offset().x(),
                                       composition_node->data().offset().y());

  // If we have more than one child (there is little to be gained by performing
  // these calculations otherwise since our bounding rectangle is equal to
  // our child's), retrieve our current total matrix and canvas viewport
  // rectangle so that we can later check if each child is within or outside
  // the viewport.
  base::optional<SkRect> canvas_bounds;
  base::optional<SkMatrix> total_matrix;
  if (children.size() > 1) {
    SkIRect canvas_boundsi;
    draw_state_.render_target->getDeviceClipBounds(&canvas_boundsi);
    canvas_bounds = SkRect::Make(canvas_boundsi);
    total_matrix = draw_state_.render_target->getTotalMatrix();
  }

  for (render_tree::CompositionNode::Children::const_iterator iter =
           children.begin();
       iter != children.end(); ++iter) {
    // No need to proceed if the child node is outside our canvas' viewport
    // rectangle.
    if (!canvas_bounds ||
        NodeIsWithinCanvasBounds(*total_matrix, *canvas_bounds, **iter)) {
      (*iter)->Accept(this);
    }
  }

  draw_state_.render_target->translate(-composition_node->data().offset().x(),
                                       -composition_node->data().offset().y());

#if ENABLE_FLUSH_AFTER_EVERY_NODE
  draw_state_.render_target->flush();
#endif
}

namespace {
SkRRect RoundedRectToSkia(const math::RectF& rect,
                          const render_tree::RoundedCorners& rounded_corners) {
  SkVector radii[4] = {
      {rounded_corners.top_left.horizontal, rounded_corners.top_left.vertical},
      {rounded_corners.top_right.horizontal,
       rounded_corners.top_right.vertical},
      {rounded_corners.bottom_right.horizontal,
       rounded_corners.bottom_right.vertical},
      {rounded_corners.bottom_left.horizontal,
       rounded_corners.bottom_left.vertical},
  };

  SkRRect rrect;
  rrect.setRectRadii(
      SkRect::MakeXYWH(rect.x(), rect.y(), rect.width(), rect.height()), radii);
  return rrect;
}

SkPath RoundedRectToSkiaPath(
    const math::RectF& rect,
    const render_tree::RoundedCorners& rounded_corners) {
  SkPath path;
  path.addRRect(RoundedRectToSkia(rect, rounded_corners),
                SkPath::kCW_Direction);
  return path;
}

void ApplyViewportMask(
    RenderTreeNodeVisitorDrawState* draw_state,
    const base::optional<render_tree::ViewportFilter>& filter) {
  if (!filter) {
    return;
  }

  if (!filter->has_rounded_corners()) {
    SkRect filter_viewport(CobaltRectFToSkiaRect(filter->viewport()));
    draw_state->render_target->clipRect(filter_viewport);
  } else {
    draw_state->render_target->clipPath(
        RoundedRectToSkiaPath(filter->viewport(), filter->rounded_corners()),
        true /* doAntiAlias */);
    draw_state->clip_is_rect = false;
  }
}

SkColor ToSkColor(const render_tree::ColorRGBA& color) {
  return SkColorSetARGB(color.a() * 255, color.r() * 255, color.g() * 255,
                        color.b() * 255);
}

void ApplyBlurFilterToPaint(
    SkPaint* paint,
    const base::optional<render_tree::BlurFilter>& blur_filter) {
  if (blur_filter && blur_filter->blur_sigma() > 0.0f) {
    sk_sp<SkImageFilter> skia_blur_filter(SkBlurImageFilter::Make(
        blur_filter->blur_sigma(), blur_filter->blur_sigma(), nullptr));
    paint->setImageFilter(skia_blur_filter);
  }
}

}  // namespace

void RenderTreeNodeVisitor::RenderFilterViaOffscreenSurface(
    const render_tree::FilterNode::Builder& filter_node) {
  const SkMatrix& total_matrix_skia =
      draw_state_.render_target->getTotalMatrix();
  math::Matrix3F total_matrix = SkiaMatrixToCobalt(total_matrix_skia);

  SkIRect canvas_boundsi;
  draw_state_.render_target->getDeviceClipBounds(&canvas_boundsi);

  common::OffscreenRenderCoordinateMapping coord_mapping =
      common::GetOffscreenRenderCoordinateMapping(
          filter_node.GetBounds(), total_matrix,
          math::Rect(canvas_boundsi.x(), canvas_boundsi.y(),
                     canvas_boundsi.width(), canvas_boundsi.height()));
  if (coord_mapping.output_bounds.size().GetArea() == 0) {
    return;
  }

  // Create a scratch surface upon which we will render the source subtree.
  scoped_ptr<ScratchSurface> scratch_surface(
      create_scratch_surface_function_->Run(
          math::Size(coord_mapping.output_bounds.width(),
                     coord_mapping.output_bounds.height())));
  if (!scratch_surface) {
    DLOG(ERROR) << "Error creating scratch image surface (width = "
                << coord_mapping.output_bounds.width()
                << ", height = " << coord_mapping.output_bounds.height()
                << "), probably because "
                   "we are low on memory, or the requested dimensions are "
                   "too large or invalid.";
    // In this case, we quit early and not render anything.
    return;
  }

  // Our canvas is guaranteed to already be cleared to ARGB(0, 0, 0, 0).
  SkCanvas* canvas = scratch_surface->GetSurface()->getCanvas();
  DCHECK(canvas);

  // Transform our drawing coordinates so we only render the source tree within
  // the viewport.
  canvas->setMatrix(CobaltMatrixToSkia(coord_mapping.sub_render_transform));

  // Render our source sub-tree into the offscreen surface.
  {
    RenderTreeNodeVisitor sub_visitor(
        canvas, create_scratch_surface_function_, reset_skia_context_function_,
        render_image_fallback_function_, render_image_with_mesh_function_,
        kType_SubVisitor);
    filter_node.source->Accept(&sub_visitor);
  }

  // With the source subtree rendered to our temporary scratch surface, we
  // will now render it to our parent render target with an alpha value set to
  // the opacity value.
  SkPaint paint;
  if (filter_node.opacity_filter) {
    paint.setARGB(filter_node.opacity_filter->opacity() * 255, 255, 255, 255);
  }
  // Use nearest neighbor when filtering texture data, since the source and
  // destination rectangles should be exactly equal.
  paint.setFilterQuality(kNone_SkFilterQuality);

  // We've already used the draw_state_.render_target's scale when rendering to
  // the offscreen surface, so reset the scale for now.
  draw_state_.render_target->save();
  ApplyBlurFilterToPaint(&paint, filter_node.blur_filter);
  ApplyViewportMask(&draw_state_, filter_node.viewport_filter);

  draw_state_.render_target->setMatrix(CobaltMatrixToSkia(
      math::TranslateMatrix(coord_mapping.output_pre_translate) * total_matrix *
      math::ScaleMatrix(coord_mapping.output_post_scale)));

  sk_sp<SkImage> image(scratch_surface->GetSurface()->makeImageSnapshot());
  DCHECK(image);

  SkRect dest_rect = SkRect::MakeXYWH(coord_mapping.output_bounds.x(),
                                      coord_mapping.output_bounds.y(),
                                      coord_mapping.output_bounds.width(),
                                      coord_mapping.output_bounds.height());

  SkRect source_rect = SkRect::MakeWH(coord_mapping.output_bounds.width(),
                                      coord_mapping.output_bounds.height());

  draw_state_.render_target->drawImageRect(image.get(), source_rect, dest_rect,
                                           &paint);

  // Finally restore our parent render target's original transform for the
  // next draw call.
  draw_state_.render_target->restore();
}

namespace {
// Returns true if we permit this source render tree to be rendered under a
// rounded corners filter mask.  In general we avoid doing this becaue it can
// multiply the number of shaders required by the system, however in some cases
// we want to do this still because there is a performance advantage.
bool SourceCanRenderWithRoundedCorners(render_tree::Node* source) {
  // If we are filtering an image, which is a frequent scenario that is worth
  // optimizing for with a custom shader.
  if (source->GetTypeId() == base::GetTypeId<render_tree::ImageNode>()) {
    return true;
  } else if (source->GetTypeId() ==
             base::GetTypeId<render_tree::CompositionNode>()) {
    // If we are a composition of valid sources, then we also allow
    // rendering through a viewport here.
    render_tree::CompositionNode* composition_node =
        base::polymorphic_downcast<render_tree::CompositionNode*>(source);
    typedef render_tree::CompositionNode::Children Children;
    const Children& children = composition_node->data().children();

    for (Children::const_iterator iter = children.begin();
         iter != children.end(); ++iter) {
      if (!SourceCanRenderWithRoundedCorners(iter->get())) {
        return false;
      }
    }
    return true;
  }

  return false;
}

// Tries to render a mapped-to-mesh node, returning false if it fails (which
// would happen if we try to apply a map-to-mesh filter to a render tree node
// that we don't currently support).
bool TryRenderMapToRect(
    render_tree::Node* source, const render_tree::MapToMeshFilter& mesh_filter,
    const RenderTreeNodeVisitor::RenderImageWithMeshFallbackFunction&
        render_onto_mesh,
    RenderTreeNodeVisitorDrawState* draw_state) {
  if (source->GetTypeId() == base::GetTypeId<render_tree::ImageNode>()) {
    render_tree::ImageNode* image_node =
        base::polymorphic_downcast<render_tree::ImageNode*>(source);
    // Since we don't have the tools to render equirectangular meshes directly
    // within Skia, delegate the task to our host rasterizer.
    render_onto_mesh.Run(image_node, mesh_filter, draw_state);
    return true;
  } else if (source->GetTypeId() ==
             base::GetTypeId<render_tree::CompositionNode>()) {
    // If we are a composition of a single node with no translation, and that
    // single node can itself be valid source, then follow through to the
    // single child and try to render it with map-to-mesh.
    render_tree::CompositionNode* composition_node =
        base::polymorphic_downcast<render_tree::CompositionNode*>(source);
    typedef render_tree::CompositionNode::Children Children;
    const Children& children = composition_node->data().children();
    if (children.size() == 1 && composition_node->data().offset().IsZero() &&
        TryRenderMapToRect(children[0].get(), mesh_filter, render_onto_mesh,
                           draw_state)) {
      return true;
    }
  }
  return false;
}

}  // namespace

void RenderTreeNodeVisitor::Visit(render_tree::FilterNode* filter_node) {
  // If we're dealing with a map-to-mesh filter, handle it all by itself.
  if (filter_node->data().map_to_mesh_filter) {
    render_tree::Node* source = filter_node->data().source;
    if (!source) {
      return;
    }
    // WIP TODO: pass in the mesh in the filter here.
    if (TryRenderMapToRect(source, *filter_node->data().map_to_mesh_filter,
                           render_image_with_mesh_function_, &draw_state_)) {
#if ENABLE_FLUSH_AFTER_EVERY_NODE
      draw_state_.render_target->flush();
#endif
    } else {
      DCHECK(false) << "We don't support map-to-mesh on other than ImageNodes.";
    }
    return;
  }

#if ENABLE_RENDER_TREE_VISITOR_TRACING && !FILTER_RENDER_TREE_VISITOR_TRACING
  TRACE_EVENT0("cobalt::renderer", "Visit(FilterNode)");

  if (filter_node->data().opacity_filter) {
    TRACE_EVENT_INSTANT1("cobalt::renderer", "opacity", "opacity",
                         filter_node->data().opacity_filter->opacity());
  }
  if (filter_node->data().viewport_filter) {
    TRACE_EVENT_INSTANT2(
        "cobalt::renderer", "viewport", "width",
        filter_node->data().viewport_filter->viewport().width(), "height",
        filter_node->data().viewport_filter->viewport().height());
  }
  if (filter_node->data().blur_filter) {
    TRACE_EVENT_INSTANT1("cobalt::renderer", "blur", "blur_sigma",
                         filter_node->data().blur_filter->blur_sigma());
  }
#endif  // ENABLE_RENDER_TREE_VISITOR_TRACING

  if (filter_node->data().opacity_filter &&
      filter_node->data().opacity_filter->opacity() <= 0.0f) {
    // The opacity 0, so we have nothing to render.
    return;
  }

  if ((!filter_node->data().opacity_filter ||
       filter_node->data().opacity_filter->opacity() == 1.0f) &&
      !filter_node->data().viewport_filter &&
      (!filter_node->data().blur_filter ||
       filter_node->data().blur_filter->blur_sigma() == 0.0f)) {
    // No filter was set, so just rasterize the source render tree as normal.
    filter_node->data().source->Accept(this);
    return;
  }

  const SkMatrix& total_matrix = draw_state_.render_target->getTotalMatrix();

  bool has_rounded_corners =
      filter_node->data().viewport_filter &&
      filter_node->data().viewport_filter->has_rounded_corners();

  // Our general strategy for applying the filter is to render to a texture
  // and then render that texture with a viewport mask applied.  However Skia
  // can optimize for us if we render the subtree source directly with a mask
  // applied.  This has the potential to greatly increase the total number of
  // shaders we need to support, so we only take advantage of this in certain
  // cases.
  bool can_render_with_clip_mask_directly =
      !filter_node->data().blur_filter &&
      // If an opacity filter is being applied, we must render to a separate
      // texture first.
      (!filter_node->data().opacity_filter ||
       common::utils::NodeCanRenderWithOpacity(filter_node->data().source)) &&
      // If transforms are applied to the viewport, then we will render to
      // a separate texture first.
      total_matrix.rectStaysRect() &&
      // If the viewport mask has rounded corners, in general we will render
      // to a bitmap before applying the mask in order to limit the number of
      // shader permutations (brush types * mask types).  However there are
      // some exceptions, for performance reasons.
      (!has_rounded_corners ||
       SourceCanRenderWithRoundedCorners(filter_node->data().source));

  if (can_render_with_clip_mask_directly) {
    RenderTreeNodeVisitorDrawState original_draw_state(draw_state_);

    draw_state_.render_target->save();
    // Remember the value of |clip_is_rect| because ApplyViewportMask may
    // modify it.
    bool clip_was_rect = draw_state_.clip_is_rect;
    ApplyViewportMask(&draw_state_, filter_node->data().viewport_filter);

    if (filter_node->data().opacity_filter) {
      draw_state_.opacity *= filter_node->data().opacity_filter->opacity();
    }
    filter_node->data().source->Accept(this);

    draw_state_.clip_is_rect = clip_was_rect;
    draw_state_.render_target->restore();
    draw_state_ = original_draw_state;
  } else {
    RenderFilterViaOffscreenSurface(filter_node->data());
  }
#if ENABLE_FLUSH_AFTER_EVERY_NODE
  draw_state_.render_target->flush();
#endif
}

namespace {

// Skia expects local texture coordinate transform matrices to map from image
// space coordinates to output rectangle coordinates, i.e. map a texture to
// its output rectangle.  Since render_tree local coordinates assume they are
// operating in a normalized coordinate system, we must perform some
// transformations.
void ConvertLocalTransformMatrixToSkiaShaderFormat(
    const math::Size& input_size,
    const math::RectF& output_rect,
    SkMatrix* local_transform_matrix) {
  // First transform to normalized coordinates, where the input transform
  // specified by local_transform_matrix is expecting to take place.
  local_transform_matrix->preScale(
      1.0f / static_cast<float>(input_size.width()),
      1.0f / static_cast<float>(input_size.height()));

  // After our transformation is applied, we must then transform to the
  // output rectangle space.
  local_transform_matrix->postScale(output_rect.width(), output_rect.height());
  local_transform_matrix->postTranslate(output_rect.x(), output_rect.y());
}

bool IsScaleAndTranslateOnly(const math::Matrix3F& mat) {
  return mat.Get(0, 1) == 0.0f && mat.Get(1, 0) == 0.0f;
}

bool LocalCoordsStaysWithinUnitBox(const math::Matrix3F& mat) {
  return mat.Get(0, 2) <= 0.0f && 1.0f - mat.Get(0, 2) <= mat.Get(0, 0) &&
         mat.Get(1, 2) <= 0.0f && 1.0f - mat.Get(1, 2) <= mat.Get(1, 1);
}

SkPaint CreateSkPaintForImageRendering(
    const RenderTreeNodeVisitorDrawState& draw_state, bool is_opaque) {
  SkPaint paint;
  // |kLow_SkFilterQuality| is used for bilinear interpolation of images.
  paint.setFilterQuality(kLow_SkFilterQuality);

  if (draw_state.opacity < 1.0f) {
    paint.setAlpha(draw_state.opacity * 255);
  } else if (is_opaque && draw_state.clip_is_rect) {
    paint.setBlendMode(SkBlendMode::kSrc);
  }

  return paint;
}

void RenderSinglePlaneImage(SinglePlaneImage* single_plane_image,
                            RenderTreeNodeVisitorDrawState* draw_state,
                            const math::RectF& destination_rect,
                            const math::Matrix3F* local_transform) {
  DCHECK(!single_plane_image->GetContentRegion());
  SkPaint paint = CreateSkPaintForImageRendering(
      *draw_state, single_plane_image->IsOpaque());

  // In the most frequent by far case where the normalized transformed image
  // texture coordinates lie within the unit square, then we must ensure NOT
  // to interpolate texel values with wrapped texels across the image borders.
  // This can result in strips of incorrect colors around the edges of images.
  // Thus, if the normalized texture coordinates lie within the unit box, we
  // will blit the image using drawBitmapRectToRect() which handles the bleeding
  // edge problem for us.

  // Determine the source rectangle, in image texture pixel coordinates.
  const math::Size& img_size = single_plane_image->GetSize();
  sk_sp<SkImage> image = single_plane_image->GetImage();

  if (IsScaleAndTranslateOnly(*local_transform) &&
      LocalCoordsStaysWithinUnitBox(*local_transform)) {
    float width = img_size.width() / local_transform->Get(0, 0);
    float height = img_size.height() / local_transform->Get(1, 1);
    float x = -width * local_transform->Get(0, 2);
    float y = -height * local_transform->Get(1, 2);

    if (image) {
      SkRect src = SkRect::MakeXYWH(x, y, width, height);
      draw_state->render_target->drawImageRect(
          image, src, CobaltRectFToSkiaRect(destination_rect), &paint);
    }
  } else {
    // Use the more general approach which allows arbitrary local texture
    // coordinate matrices.
    SkMatrix skia_local_transform = CobaltMatrixToSkia(*local_transform);

    ConvertLocalTransformMatrixToSkiaShaderFormat(img_size, destination_rect,
                                                  &skia_local_transform);

    if (image) {
      sk_sp<SkShader> image_shader =
          image->makeShader(SkShader::kRepeat_TileMode,
                            SkShader::kRepeat_TileMode, &skia_local_transform);

      paint.setShader(image_shader);

      draw_state->render_target->drawRect(
          CobaltRectFToSkiaRect(destination_rect), paint);
    }
  }
}

void RenderMultiPlaneImage(MultiPlaneImage* multi_plane_image,
                           RenderTreeNodeVisitorDrawState* draw_state,
                           const math::RectF& destination_rect,
                           const math::Matrix3F* local_transform) {
  UNREFERENCED_PARAMETER(multi_plane_image);
  UNREFERENCED_PARAMETER(draw_state);
  UNREFERENCED_PARAMETER(destination_rect);
  UNREFERENCED_PARAMETER(local_transform);

  // Multi-plane images like YUV images are not supported when using the
  // software rasterizers.
  NOTIMPLEMENTED();
  NOTREACHED();
}

}  // namespace

void RenderTreeNodeVisitor::Visit(render_tree::ImageNode* image_node) {
  // The image_node may contain nothing. For example, when it represents a video
  // element before any frame is decoded.
  if (!image_node->data().source) {
    return;
  }

#if ENABLE_RENDER_TREE_VISITOR_TRACING
  TRACE_EVENT0("cobalt::renderer", "Visit(ImageNode)");
#endif
  skia::Image* image =
      base::polymorphic_downcast<skia::Image*>(image_node->data().source.get());

  // Creating an image via a resource provider may simply return a frontend
  // image object and enqueue the initialization of a backend image (to be
  // performed on the rasterizer thread).  This function ensures that that
  // initialization is completed.  This would normally not be an issue if
  // an image is submitted as part of a render tree submission, however we
  // may end up with a non-backend-initialized image if a new image is selected
  // during an animation callback (e.g. video playback can do this).
  // This should be a quick operation, and it needs to happen eventually, so
  // if it is not done now, it will be done in the next frame, or the one after
  // that.
  if (image->EnsureInitialized()) {
    // EnsureInitialized() may make a number of GL calls that results in GL
    // state being modified behind Skia's back, therefore we have Skia reset its
    // state in this case.
    if (!reset_skia_context_function_.is_null()) {
      reset_skia_context_function_.Run();
    }
  }

  // We issue different skia rasterization commands to render the image
  // depending on whether it's single or multi planed.
  if (!image->CanRenderInSkia()) {
    render_image_fallback_function_.Run(image_node, &draw_state_);
  } else {
    if (image->GetTypeId() == base::GetTypeId<SinglePlaneImage>()) {
      SinglePlaneImage* single_plane_image =
          base::polymorphic_downcast<SinglePlaneImage*>(image);

      RenderSinglePlaneImage(single_plane_image, &draw_state_,
                             image_node->data().destination_rect,
                             &(image_node->data().local_transform));
    } else if (image->GetTypeId() == base::GetTypeId<MultiPlaneImage>()) {
      RenderMultiPlaneImage(base::polymorphic_downcast<MultiPlaneImage*>(image),
                            &draw_state_, image_node->data().destination_rect,
                            &(image_node->data().local_transform));
    } else {
      NOTREACHED();
    }
  }

#if ENABLE_FLUSH_AFTER_EVERY_NODE
  draw_state_.render_target->flush();
#endif
}

void RenderTreeNodeVisitor::Visit(
    render_tree::MatrixTransform3DNode* matrix_transform_3d_node) {
#if ENABLE_RENDER_TREE_VISITOR_TRACING && !FILTER_RENDER_TREE_VISITOR_TRACING
  TRACE_EVENT0("cobalt::renderer", "Visit(MatrixTransform3DNode)");
#endif

  glm::mat4 before = draw_state_.transform_3d;
  draw_state_.transform_3d *= matrix_transform_3d_node->data().transform;

  matrix_transform_3d_node->data().source->Accept(this);

  draw_state_.transform_3d = before;

#if ENABLE_FLUSH_AFTER_EVERY_NODE
  draw_state_.render_target->flush();
#endif
}

void RenderTreeNodeVisitor::Visit(
    render_tree::MatrixTransformNode* matrix_transform_node) {
#if ENABLE_RENDER_TREE_VISITOR_TRACING && !FILTER_RENDER_TREE_VISITOR_TRACING
  TRACE_EVENT0("cobalt::renderer", "Visit(MatrixTransformNode)");
#endif

  // Concatenate the matrix transform to the render target and then continue
  // on with rendering our source.
  draw_state_.render_target->save();

  draw_state_.render_target->concat(
      CobaltMatrixToSkia(matrix_transform_node->data().transform));

  matrix_transform_node->data().source->Accept(this);

  draw_state_.render_target->restore();

#if ENABLE_FLUSH_AFTER_EVERY_NODE
  draw_state_.render_target->flush();
#endif
}

void RenderTreeNodeVisitor::Visit(
    render_tree::PunchThroughVideoNode* punch_through_video_node) {
#if ENABLE_RENDER_TREE_VISITOR_TRACING
  TRACE_EVENT0("cobalt::renderer", "Visit(PunchThroughVideoNode)");
#endif

  if (visitor_type_ == kType_SubVisitor) {
    DLOG(ERROR) << "Punch through alpha images cannot be rendered via "
                   "offscreen surfaces (since the offscreen surface will then "
                   "be composed using blending).  Unfortunately, you will have "
                   "to find another way to arrange your render tree if you "
                   "wish to use punch through alpha (e.g. ensure no opacity "
                   "filters exist as an ancestor to this node).";
    // We proceed anyway, just in case things happen to work out.
  }

  const math::RectF& math_rect = punch_through_video_node->data().rect;

  SkRect sk_rect = SkRect::MakeXYWH(math_rect.x(), math_rect.y(),
                                    math_rect.width(), math_rect.height());
  SkMatrix total_matrix = draw_state_.render_target->getTotalMatrix();

  SkRect sk_rect_transformed;
  total_matrix.mapRect(&sk_rect_transformed, sk_rect);

  math::RectF transformed_rectf(
      sk_rect_transformed.x(), sk_rect_transformed.y(),
      sk_rect_transformed.width(), sk_rect_transformed.height());
  math::Rect transformed_rect = math::Rect::RoundFromRectF(transformed_rectf);
  punch_through_video_node->data().set_bounds_cb.Run(transformed_rect);

  SkPaint paint;
  paint.setBlendMode(SkBlendMode::kSrc);
  paint.setARGB(0, 0, 0, 0);

  draw_state_.render_target->drawRect(sk_rect, paint);

#if ENABLE_FLUSH_AFTER_EVERY_NODE
  draw_state_.render_target->flush();
#endif
}

namespace {

class SkiaBrushVisitor : public render_tree::BrushVisitor {
 public:
  explicit SkiaBrushVisitor(SkPaint* paint,
                            const RenderTreeNodeVisitorDrawState& draw_state)
      : paint_(paint), draw_state_(draw_state) {}

  void Visit(
      const cobalt::render_tree::SolidColorBrush* solid_color_brush) override;
  void Visit(const cobalt::render_tree::LinearGradientBrush*
                 linear_gradient_brush) override;
  void Visit(const cobalt::render_tree::RadialGradientBrush*
                 radial_gradient_brush) override;

 private:
  SkPaint* paint_;
  const RenderTreeNodeVisitorDrawState& draw_state_;
};

void SkiaBrushVisitor::Visit(
    const cobalt::render_tree::SolidColorBrush* solid_color_brush) {
  const cobalt::render_tree::ColorRGBA& color = solid_color_brush->color();

  float alpha = color.a() * draw_state_.opacity;
  if (alpha == 1.0f) {
    paint_->setBlendMode(SkBlendMode::kSrc);
  } else {
    paint_->setBlendMode(SkBlendMode::kSrcOver);
  }

  paint_->setARGB(alpha * 255, color.r() * 255, color.g() * 255,
                  color.b() * 255);
}

namespace {

// Helper struct to convert render_tree::ColorStopList to a format that Skia
// methods will easily accept.
struct SkiaColorStops {
  explicit SkiaColorStops(const render_tree::ColorStopList& color_stops)
      : colors(color_stops.size()),
        positions(color_stops.size()),
        has_alpha(false) {
    for (size_t i = 0; i < color_stops.size(); ++i) {
      if (color_stops[i].color.HasAlpha()) {
        has_alpha = true;
      }

      colors[i] = ToSkColor(color_stops[i].color);
      positions[i] = color_stops[i].position;
    }
  }

  std::vector<SkColor> colors;
  std::vector<SkScalar> positions;
  bool has_alpha;
};

}  // namespace

void SkiaBrushVisitor::Visit(
    const cobalt::render_tree::LinearGradientBrush* linear_gradient_brush) {
  SkPoint points[2] = {
      {linear_gradient_brush->source().x(),
       linear_gradient_brush->source().y()},
      {linear_gradient_brush->dest().x(), linear_gradient_brush->dest().y()}};

  SkiaColorStops skia_color_stops(linear_gradient_brush->color_stops());

  sk_sp<SkShader> shader(SkGradientShader::MakeLinear(
      points, &skia_color_stops.colors[0], &skia_color_stops.positions[0],
      linear_gradient_brush->color_stops().size(), SkShader::kClamp_TileMode,
      SkGradientShader::kInterpolateColorsInPremul_Flag, NULL));
  paint_->setShader(shader);

  if (!skia_color_stops.has_alpha && draw_state_.opacity == 1.0f) {
    paint_->setBlendMode(SkBlendMode::kSrc);
  } else {
    if (draw_state_.opacity < 1.0f) {
      paint_->setAlpha(255 * draw_state_.opacity);
    }
    paint_->setBlendMode(SkBlendMode::kSrcOver);
  }
}

void SkiaBrushVisitor::Visit(
    const cobalt::render_tree::RadialGradientBrush* radial_gradient_brush) {
  SkPoint center = {radial_gradient_brush->center().x(),
                    radial_gradient_brush->center().y()};

  SkiaColorStops skia_color_stops(radial_gradient_brush->color_stops());

  // Skia accepts parameters for a circular gradient only, but it does accept
  // a local matrix that we can use to stretch it into an ellipse, so we set
  // up our circular radius based on our x radius and then stretch the gradient
  // vertically to the y radius.
  SkMatrix local_matrix;
  local_matrix.setIdentity();
  float scale_y =
      radial_gradient_brush->radius_y() / radial_gradient_brush->radius_x();
  // And we'll need to add a translation as well to keep the center at the
  // center despite the scale.
  float translate_y = (1.0f - scale_y) * radial_gradient_brush->center().y();
  local_matrix.setTranslateY(translate_y);
  local_matrix.setScaleY(scale_y);

  sk_sp<SkShader> shader(SkGradientShader::MakeRadial(
      center, radial_gradient_brush->radius_x(), &skia_color_stops.colors[0],
      &skia_color_stops.positions[0],
      radial_gradient_brush->color_stops().size(), SkShader::kClamp_TileMode,
      SkGradientShader::kInterpolateColorsInPremul_Flag, &local_matrix));
  paint_->setShader(shader);

  if (!skia_color_stops.has_alpha && draw_state_.opacity == 1.0f) {
    paint_->setBlendMode(SkBlendMode::kSrc);
  } else {
    if (draw_state_.opacity < 1.0f) {
      paint_->setAlpha(255 * draw_state_.opacity);
    }
    paint_->setBlendMode(SkBlendMode::kSrcOver);
  }
}

void DrawRectWithBrush(RenderTreeNodeVisitorDrawState* draw_state,
                       cobalt::render_tree::Brush* brush,
                       const math::RectF& rect) {
  // Setup our paint object according to the brush parameters set on the
  // rectangle.
  SkPaint paint;
  SkiaBrushVisitor brush_visitor(&paint, *draw_state);
  brush->Accept(&brush_visitor);

  if (!draw_state->render_target->getTotalMatrix().preservesAxisAlignment()) {
    // Enable anti-aliasing if we're rendering a rotated or skewed box.
    paint.setAntiAlias(true);
  }

  draw_state->render_target->drawRect(
      SkRect::MakeXYWH(rect.x(), rect.y(), rect.width(), rect.height()), paint);
}

namespace {
class CheckForSolidBrushVisitor : public render_tree::BrushVisitor {
 public:
  CheckForSolidBrushVisitor() : is_solid_brush_(false) {}

  void Visit(
      const cobalt::render_tree::SolidColorBrush* solid_color_brush) override {
    is_solid_brush_ = true;
  }

  void Visit(const cobalt::render_tree::LinearGradientBrush*
                 linear_gradient_brush) override {}
  void Visit(const cobalt::render_tree::RadialGradientBrush*
                 radial_gradient_brush) override {}

  bool is_solid_brush() const { return is_solid_brush_; }

 private:
  bool is_solid_brush_;
};

// DChecks that the brush is a solid brush.
bool CheckForSolidBrush(cobalt::render_tree::Brush* brush) {
  CheckForSolidBrushVisitor visitor;
  brush->Accept(&visitor);
  return visitor.is_solid_brush();
}
}  // namespace

void DrawRoundedRectWithBrush(
    RenderTreeNodeVisitorDrawState* draw_state, render_tree::Brush* brush,
    const math::RectF& rect,
    const render_tree::RoundedCorners& rounded_corners) {
  if (!CheckForSolidBrush(brush)) {
    NOTREACHED() << "Only solid brushes are currently supported for this shape "
                    "in Cobalt.";
    return;
  }

  SkPaint paint;
  SkiaBrushVisitor brush_visitor(&paint, *draw_state);
  brush->Accept(&brush_visitor);

  paint.setAntiAlias(true);
  draw_state->render_target->drawPath(
      RoundedRectToSkiaPath(rect, rounded_corners), paint);
}

void DrawUniformSolidNonRoundRectBorder(
    RenderTreeNodeVisitorDrawState* draw_state,
    const math::RectF& rect,
    float border_width,
    const render_tree::ColorRGBA& border_color,
    bool anti_alias) {
  SkPaint paint;
  const float alpha = border_color.a() * draw_state->opacity;
  paint.setARGB(alpha * 255,
                border_color.r() * 255,
                border_color.g() * 255,
                border_color.b() * 255);
  paint.setAntiAlias(anti_alias);
  if (alpha == 1.0f) {
    paint.setBlendMode(SkBlendMode::kSrc);
  } else {
    paint.setBlendMode(SkBlendMode::kSrcOver);
  }
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setStrokeWidth(border_width);
  SkRect skrect;
  const float half_border_width = border_width * 0.5f;
  skrect.set(rect.x() + half_border_width,
             rect.y() + half_border_width,
             rect.right() - half_border_width,
             rect.bottom() - half_border_width);
  draw_state->render_target->drawRect(skrect, paint);
}

void DrawQuadWithColorIfBorderIsSolid(
    render_tree::BorderStyle border_style,
    RenderTreeNodeVisitorDrawState* draw_state,
    const render_tree::ColorRGBA& color, const SkPoint* points,
    bool anti_alias) {
  if (border_style == render_tree::kBorderStyleSolid) {
    SkPaint paint;
    float alpha = color.a();
    alpha *= draw_state->opacity;
    paint.setARGB(alpha * 255, color.r() * 255, color.g() * 255,
                  color.b() * 255);
    paint.setAntiAlias(anti_alias);
    if (alpha == 1.0f) {
      paint.setBlendMode(SkBlendMode::kSrc);
    } else {
      paint.setBlendMode(SkBlendMode::kSrcOver);
    }

    SkPath path;
    path.addPoly(points, 4, true);
    draw_state->render_target->drawPath(path, paint);
  } else {
    DCHECK_EQ(border_style, render_tree::kBorderStyleNone);
  }
}

// Draw 4 trapezoids for 4 directions.
//       A ___________ B
//        |\_________/|
//        ||E       F||
//        ||         ||
//        ||G_______H||
//        |/_________\|
//       C             D
void DrawSolidNonRoundRectBorder(RenderTreeNodeVisitorDrawState* draw_state,
                                 const math::RectF& rect,
                                 const render_tree::Border& border) {
  // Check if the border colors are the same or not to determine whether we
  // should be using antialiasing.  If the border colors are different, then
  // there will be visible diagonal edges in the output and so we would like to
  // render with antialiasing enabled to smooth those diagonal edges.
  bool border_colors_are_same = border.top.color == border.left.color &&
                                border.top.color == border.bottom.color &&
                                border.top.color == border.right.color;

  // If any of the border edges have width less than this threshold, they will
  // use antialiasing as otherwise depending on the border's fractional
  // position, it may have one extra pixel visible, which is a large percentage
  // of its small width.
  const float kAntiAliasWidthThreshold = 3.0f;

  // Use faster draw function if possible.
  if (border_colors_are_same &&
      border.top.style == render_tree::kBorderStyleSolid &&
      border.bottom.style == render_tree::kBorderStyleSolid &&
      border.left.style == render_tree::kBorderStyleSolid &&
      border.right.style == render_tree::kBorderStyleSolid &&
      border.top.width == border.bottom.width &&
      border.top.width == border.left.width &&
      border.top.width == border.right.width) {
    bool anti_alias = border.top.width < kAntiAliasWidthThreshold ||
                      border.bottom.width < kAntiAliasWidthThreshold ||
                      border.left.width < kAntiAliasWidthThreshold ||
                      border.right.width < kAntiAliasWidthThreshold;
    DrawUniformSolidNonRoundRectBorder(draw_state, rect,
        border.top.width, border.top.color, anti_alias);
    return;
  }

  // Top
  SkPoint top_points[4] = {
      {rect.x(), rect.y()},                                                 // A
      {rect.x() + border.left.width, rect.y() + border.top.width},          // E
      {rect.right() - border.right.width, rect.y() + border.top.width},     // F
      {rect.right(), rect.y()}};                                            // B
  DrawQuadWithColorIfBorderIsSolid(
      border.top.style, draw_state, border.top.color, top_points,
      border.top.width < kAntiAliasWidthThreshold ? true
                                                  : !border_colors_are_same);

  // Left
  SkPoint left_points[4] = {
      {rect.x(), rect.y()},                                                 // A
      {rect.x(), rect.bottom()},                                            // C
      {rect.x() + border.left.width, rect.bottom() - border.bottom.width},  // G
      {rect.x() + border.left.width, rect.y() + border.top.width}};         // E
  DrawQuadWithColorIfBorderIsSolid(
      border.left.style, draw_state, border.left.color, left_points,
      border.left.width < kAntiAliasWidthThreshold ? true
                                                   : !border_colors_are_same);

  // Bottom
  SkPoint bottom_points[4] = {
      {rect.x() + border.left.width, rect.bottom() - border.bottom.width},  // G
      {rect.x(), rect.bottom()},                                            // C
      {rect.right(), rect.bottom()},                                        // D
      {rect.right() - border.right.width,
       rect.bottom() - border.bottom.width}};                               // H
  DrawQuadWithColorIfBorderIsSolid(
      border.bottom.style, draw_state, border.bottom.color, bottom_points,
      border.bottom.width < kAntiAliasWidthThreshold ? true
                                                     : !border_colors_are_same);

  // Right
  SkPoint right_points[4] = {
      {rect.right() - border.right.width, rect.y() + border.top.width},     // F
      {rect.right() - border.right.width,
       rect.bottom() - border.bottom.width},                                // H
      {rect.right(), rect.bottom()},                                        // D
      {rect.right(), rect.y()}};                                            // B
  DrawQuadWithColorIfBorderIsSolid(
      border.right.style, draw_state, border.right.color, right_points,
      border.right.width < kAntiAliasWidthThreshold ? true
                                                    : !border_colors_are_same);
}

void DrawSolidRoundedRectBorderToRenderTarget(
    RenderTreeNodeVisitorDrawState* draw_state, const math::RectF& rect,
    const render_tree::RoundedCorners& rounded_corners,
    const math::RectF& content_rect,
    const render_tree::RoundedCorners& inner_rounded_corners,
    const render_tree::ColorRGBA& color) {
  SkPaint paint;
  paint.setAntiAlias(true);

  float alpha = color.a();
  alpha *= draw_state->opacity;
  paint.setARGB(alpha * 255, color.r() * 255, color.g() * 255, color.b() * 255);
  if (alpha == 1.0f) {
    paint.setBlendMode(SkBlendMode::kSrc);
  } else {
    paint.setBlendMode(SkBlendMode::kSrcOver);
  }

  draw_state->render_target->drawDRRect(
      RoundedRectToSkia(rect, rounded_corners),
      RoundedRectToSkia(content_rect, inner_rounded_corners), paint);
}

namespace {
bool IsCircle(const math::SizeF& size,
              const render_tree::RoundedCorners& rounded_corners) {
  if (size.width() != size.height()) {
    return false;
  }

  float radius = size.width() * 0.5f;
  // Corners cannot be "more round" than a circle, so we check using >= rather
  // than == here.
  return rounded_corners.AllCornersGE(
      render_tree::RoundedCorner(radius, radius));
}

bool AllBorderSidesShareSameProperties(const render_tree::Border& border) {
  return (border.top == border.left && border.top == border.right &&
          border.top == border.bottom);
}

base::optional<SkPoint> CalculateIntersectionPoint(const SkPoint& a,
                                                   const SkPoint& b,
                                                   const SkPoint& c,
                                                   const SkPoint& d) {
  // Given 2 lines, each represented by 2 points (points a and b lie on line 1
  // and points c and d lie on line 2), calculates the intersection point of
  // these 2 lines if they intersect.
  const float ab_width = b.x() - a.x();
  const float ab_height = b.y() - a.y();
  const float cd_width = d.x() - c.x();
  const float cd_height = d.y() - c.y();
  base::optional<SkPoint> intersection;

  const float determinant = (ab_width * cd_height) - (ab_height * cd_width);
  if (determinant) {
    const float param =
        ((c.x() - a.x()) * cd_height - (c.y() - a.y()) * cd_width) /
        determinant;
    intersection =
        SkPoint({a.x() + param * ab_width, a.y() + param * ab_height});
  }

  return intersection;
}
}  // namespace

void SetUpDrawStateClipPath(RenderTreeNodeVisitorDrawState* draw_state,
                            const SkPoint* points) {
  draw_state->render_target->restore();
  draw_state->render_target->save();

  SkPath path;
  path.addPoly(points, 4, true);

  draw_state->render_target->clipPath(path, SkClipOp::kIntersect, true);
}

void DrawSolidRoundedRectBorderByEdge(
    RenderTreeNodeVisitorDrawState* draw_state, const math::RectF& rect,
    const render_tree::RoundedCorners& rounded_corners,
    const math::RectF& content_rect,
    const render_tree::RoundedCorners& inner_rounded_corners,
    const render_tree::Border& border) {
  // Render each border edge seperately using SkCanvas::clipPath() to clip out
  // each edge's region from the overall RRect.
  // Divide the area into 4 trapezoids, each represented by 4 points that
  // encompass the clipped out region for a border edge (including the rounded
  // corners). Draw within each clipped out region seperately.
  //       A ___________ B
  //        |\_________/|
  //        ||E       F||
  //        ||         ||
  //        ||G_______H||
  //        |/_________\|
  //       C             D
  // NOTE: As described in
  // https://www.w3.org/TR/css3-background/#corner-transitions, the spec does
  // not give an exact function for calculating the center of color/style
  // transitions between adjoining borders, so the following math resembles
  // Chrome: https://cs.chromium.org/BoxBorderPainter::ClipBorderSidePolygon.
  // The center of a transition (represented by points E, F, G and H above) is
  // calculated by finding the intersection point of 2 lines:
  // 1) The line consisting of the outer point (ex: point A, B, C or D) and this
  // outer point adjusted by the corresponding edge's border width (inner
  // point).
  // 2) The line consisting of the inner point adjusted by the corresponding
  // edge's border radius in the x-direction and the inner point adjusted by the
  // corresponding edge's border radius in the y-direction.
  // If no intersection point exists, the transition center is just the inner
  // point.

  base::optional<SkPoint> intersection;
  // Top Left
  const SkPoint top_left_outer = {rect.x(), rect.y()};
  SkPoint top_left_inner = {rect.x() + border.left.width,
                            rect.y() + border.top.width};

  if (!inner_rounded_corners.top_left.IsSquare()) {
    intersection = CalculateIntersectionPoint(
        top_left_outer, top_left_inner,
        SkPoint({top_left_inner.x() + inner_rounded_corners.top_left.horizontal,
                 top_left_inner.y()}),
        SkPoint(
            {top_left_inner.x(),
             top_left_inner.y() + inner_rounded_corners.top_left.vertical}));
    if (intersection) {
      top_left_inner = SkPoint({intersection->x(), intersection->y()});
    }
  }

  // Top Right
  const SkPoint top_right_outer = {rect.right(), rect.y()};
  SkPoint top_right_inner = {rect.right() - border.right.width,
                             rect.y() + border.top.width};

  if (!inner_rounded_corners.top_right.IsSquare()) {
    intersection = CalculateIntersectionPoint(
        top_right_outer, top_right_inner,
        SkPoint(
            {top_right_inner.x() - inner_rounded_corners.top_right.horizontal,
             top_right_inner.y()}),
        SkPoint(
            {top_right_inner.x(),
             top_right_inner.y() + inner_rounded_corners.top_right.vertical}));
    if (intersection) {
      top_right_inner = SkPoint({intersection->x(), intersection->y()});
    }
  }

  // Bottom Left
  const SkPoint bottom_left_outer = {rect.x(), rect.bottom()};
  SkPoint bottom_left_inner = {rect.x() + border.left.width,
                               rect.bottom() - border.bottom.width};

  if (!inner_rounded_corners.bottom_left.IsSquare()) {
    intersection = CalculateIntersectionPoint(
        bottom_left_outer, bottom_left_inner,
        SkPoint({bottom_left_inner.x() +
                     inner_rounded_corners.bottom_left.horizontal,
                 bottom_left_inner.y()}),
        SkPoint({bottom_left_inner.x(),
                 bottom_left_inner.y() -
                     inner_rounded_corners.bottom_left.vertical}));
    if (intersection) {
      bottom_left_inner = SkPoint({intersection->x(), intersection->y()});
    }
  }

  // Bottom Right
  const SkPoint bottom_right_outer = {rect.right(), rect.bottom()};
  SkPoint bottom_right_inner = {rect.right() - border.right.width,
                                rect.bottom() - border.bottom.width};

  if (!inner_rounded_corners.bottom_right.IsSquare()) {
    intersection = CalculateIntersectionPoint(
        bottom_right_outer, bottom_right_inner,
        SkPoint({bottom_right_inner.x() -
                     inner_rounded_corners.bottom_right.horizontal,
                 bottom_right_inner.y()}),
        SkPoint({bottom_right_inner.x(),
                 bottom_right_inner.y() -
                     inner_rounded_corners.bottom_right.vertical}));
    if (intersection) {
      bottom_right_inner = SkPoint({intersection->x(), intersection->y()});
    }
  }

  // Top
  SkPoint top_points[4] = {top_left_outer, top_left_inner,     // A, E
                           top_right_inner, top_right_outer};  // F, B
  SetUpDrawStateClipPath(draw_state, top_points);
  DrawSolidRoundedRectBorderToRenderTarget(draw_state, rect, rounded_corners,
                                           content_rect, inner_rounded_corners,
                                           border.top.color);

  // Left
  SkPoint left_points[4] = {top_left_outer, bottom_left_outer,   // A, C
                            bottom_left_inner, top_left_inner};  // G, E
  SetUpDrawStateClipPath(draw_state, left_points);
  DrawSolidRoundedRectBorderToRenderTarget(draw_state, rect, rounded_corners,
                                           content_rect, inner_rounded_corners,
                                           border.left.color);

  // Bottom
  SkPoint bottom_points[4] = {bottom_left_inner, bottom_left_outer,     // G, C
                              bottom_right_outer, bottom_right_inner};  // D, H
  SetUpDrawStateClipPath(draw_state, bottom_points);
  DrawSolidRoundedRectBorderToRenderTarget(draw_state, rect, rounded_corners,
                                           content_rect, inner_rounded_corners,
                                           border.bottom.color);

  // Right
  SkPoint right_points[4] = {top_right_inner, bottom_right_inner,   // F, H
                             bottom_right_outer, top_right_outer};  // D, B
  SetUpDrawStateClipPath(draw_state, right_points);
  DrawSolidRoundedRectBorderToRenderTarget(draw_state, rect, rounded_corners,
                                           content_rect, inner_rounded_corners,
                                           border.right.color);
}

void DrawSolidRoundedRectBorderSoftware(
    RenderTreeNodeVisitorDrawState* draw_state, const math::RectF& rect,
    const render_tree::RoundedCorners& rounded_corners,
    const math::RectF& content_rect,
    const render_tree::RoundedCorners& inner_rounded_corners,
    const render_tree::Border& border) {
  SkImageInfo image_info =
      SkImageInfo::MakeN32(rect.width(), rect.height(), kPremul_SkAlphaType);
  SkBitmap bitmap;

  bool allocation_successful = bitmap.tryAllocPixels(image_info);
  if (!allocation_successful) {
    LOG(WARNING) << "Unable to allocate pixels of size " << rect.width() << "x"
                 << rect.height();
    return;
  }

  SkCanvas canvas(bitmap);
  canvas.clear(SkColorSetARGB(0, 0, 0, 0));

  math::RectF canvas_rect(rect.size());
  math::RectF canvas_content_rect(content_rect);
  canvas_content_rect.Offset(-rect.OffsetFromOrigin());

  RenderTreeNodeVisitorDrawState sub_draw_state(*draw_state);
  sub_draw_state.render_target = &canvas;

  if (AllBorderSidesShareSameProperties(border)) {
    // If all 4 edges share the same border properties, render the whole RRect
    // at once.
    DrawSolidRoundedRectBorderToRenderTarget(
        &sub_draw_state, canvas_rect, rounded_corners, canvas_content_rect,
        inner_rounded_corners, border.top.color);
  } else {
    DrawSolidRoundedRectBorderByEdge(&sub_draw_state, canvas_rect,
                                     rounded_corners, canvas_content_rect,
                                     inner_rounded_corners, border);
  }

  canvas.flush();

  SkPaint render_target_paint;
  render_target_paint.setFilterQuality(kNone_SkFilterQuality);
  draw_state->render_target->drawBitmap(bitmap, rect.x(), rect.y(),
                                        &render_target_paint);
}

void DrawSolidRoundedRectBorder(
    RenderTreeNodeVisitorDrawState* draw_state, const math::RectF& rect,
    const render_tree::RoundedCorners& rounded_corners,
    const math::RectF& content_rect,
    const render_tree::RoundedCorners& inner_rounded_corners,
    const render_tree::Border& border) {
  if (IsCircle(rect.size(), rounded_corners) &&
      AllBorderSidesShareSameProperties(border)) {
    // We are able to render circular borders, whose edges have the same
    // properties, using hardware, so introduce a special case for them.
    DrawSolidRoundedRectBorderToRenderTarget(
        draw_state, rect, rounded_corners, content_rect, inner_rounded_corners,
        border.top.color);
  } else {
    // For now we fallback to software for drawing most rounded corner borders,
    // with some situations specified above being special cased. The reason we
    // do this is to limit then number of shaders that need to be implemented.
    NOTIMPLEMENTED() << "Warning: Software rasterizing either a solid "
                        "rectangle, oval or circle border with different "
                        "properties.";
    DrawSolidRoundedRectBorderSoftware(draw_state, rect, rounded_corners,
                                       content_rect, inner_rounded_corners,
                                       border);
  }
}

}  // namespace

void RenderTreeNodeVisitor::Visit(render_tree::RectNode* rect_node) {
#if ENABLE_RENDER_TREE_VISITOR_TRACING
  TRACE_EVENT0("cobalt::renderer", "Visit(RectNode)");
#endif
  const math::RectF& rect(rect_node->data().rect);
  math::RectF content_rect(rect);
  if (rect_node->data().border) {
    content_rect.Inset(rect_node->data().border->left.width,
                       rect_node->data().border->top.width,
                       rect_node->data().border->right.width,
                       rect_node->data().border->bottom.width);
  }

  // Apply rounded corners if it exists.
  base::optional<render_tree::RoundedCorners> inner_rounded_corners;
  if (rect_node->data().rounded_corners) {
    if (rect_node->data().border) {
      inner_rounded_corners = rect_node->data().rounded_corners->Inset(
          rect_node->data().border->left.width,
          rect_node->data().border->top.width,
          rect_node->data().border->right.width,
          rect_node->data().border->bottom.width);
    } else {
      inner_rounded_corners = *rect_node->data().rounded_corners;
    }
  }

  if (rect_node->data().background_brush) {
    if (inner_rounded_corners && !inner_rounded_corners->AreSquares()) {
      DrawRoundedRectWithBrush(&draw_state_,
                               rect_node->data().background_brush.get(),
                               content_rect, *inner_rounded_corners);
    } else {
      DrawRectWithBrush(&draw_state_, rect_node->data().background_brush.get(),
                        content_rect);
    }
  }

  if (rect_node->data().border) {
    if (rect_node->data().rounded_corners) {
      DrawSolidRoundedRectBorder(
          &draw_state_, rect, *rect_node->data().rounded_corners, content_rect,
          *inner_rounded_corners, *rect_node->data().border);
    } else {
      DrawSolidNonRoundRectBorder(&draw_state_, rect,
                                  *rect_node->data().border);
    }
  }

#if ENABLE_FLUSH_AFTER_EVERY_NODE
  draw_state_.render_target->flush();
#endif
}

namespace {
struct RRect {
  RRect(const math::RectF& rect,
        const base::optional<render_tree::RoundedCorners>& rounded_corners)
      : rect(rect), rounded_corners(rounded_corners) {}

  void Offset(const math::Vector2dF& offset) { rect.Offset(offset); }

  void Inset(float value) {
    rect.Inset(value, value);
    if (rounded_corners) {
      *rounded_corners = rounded_corners->Inset(value, value, value, value);
    }
  }

  math::RectF rect;
  base::optional<render_tree::RoundedCorners> rounded_corners;
};

// |shadow_rect| contains the original rect, offset according to the shadow's
// offset, and expanded by the |spread| parameter.  This is the rectangle that
// will actually be passed to the draw command.
RRect GetShadowRect(const render_tree::RectShadowNode& node) {
  RRect shadow_rect(node.data().rect, node.data().rounded_corners);

  shadow_rect.Offset(node.data().shadow.offset);
  float spread = node.data().spread;
  if (node.data().inset) {
    shadow_rect.Inset(spread);
  } else {
    shadow_rect.Inset(-spread);
  }

  return shadow_rect;
}

SkPaint GetPaintForBoxShadow(const render_tree::Shadow& shadow) {
  SkPaint paint;
  paint.setARGB(shadow.color.a() * 255, shadow.color.r() * 255,
                shadow.color.g() * 255, shadow.color.b() * 255);

  sk_sp<SkMaskFilter> mf(
      SkBlurMaskFilter::Make(kNormal_SkBlurStyle, shadow.blur_sigma));
  paint.setMaskFilter(mf);

  return paint;
}

void DrawInsetRectShadowNode(render_tree::RectShadowNode* node,
                             SkCanvas* canvas) {
#if ENABLE_RENDER_TREE_VISITOR_TRACING
  TRACE_EVENT0("cobalt::renderer", "DrawInsetRectShadowNode()");
#endif
  DCHECK(node->data().inset);

  // We outset the user specified shadow rectangle by the blur extent to know
  // where we should start drawing in order for the blurred result to not appear
  // light at the edges.  There is no point in drawing outside of this rectangle
  // given the fact that all rendering output will be clipped by
  // |node->data().size|.
  const math::RectF& rect = node->data().rect;
  math::RectF shadow_bounds(rect);
  math::Vector2dF blur_extent = node->data().shadow.BlurExtent();
  shadow_bounds.Outset(blur_extent.x(), blur_extent.y());

  // Ensure that |shadow_rect| fits inside of |shadow_bounds| before continuing.
  math::RectF shadow_rect(GetShadowRect(*node).rect);
  shadow_rect.Intersect(shadow_bounds);

  // For inset shadows, we draw 4 rectangles in the shadow's color, all of which
  // wrap around |shadow_rect|, have extents |shadow_bounds|, and are clipped
  // by |rect|.
  SkRect left_shadow_rect =
      SkRect::MakeLTRB(shadow_bounds.x(), shadow_bounds.y(), shadow_rect.x(),
                       shadow_bounds.bottom());
  SkRect top_shadow_rect =
      SkRect::MakeLTRB(shadow_bounds.x(), shadow_bounds.y(),
                       shadow_bounds.right(), shadow_rect.y());
  SkRect right_shadow_rect =
      SkRect::MakeLTRB(shadow_rect.right(), shadow_bounds.y(),
                       shadow_bounds.right(), shadow_bounds.bottom());
  SkRect bottom_shadow_rect =
      SkRect::MakeLTRB(shadow_bounds.x(), shadow_rect.bottom(),
                       shadow_bounds.right(), shadow_bounds.bottom());

  SkRect* draw_rects[] = {
      &left_shadow_rect, &top_shadow_rect, &right_shadow_rect,
      &bottom_shadow_rect,
  };

  SkRect skia_clip_rect = CobaltRectFToSkiaRect(rect);

  SkPaint paint = GetPaintForBoxShadow(node->data().shadow);

  canvas->save();
  canvas->clipRect(skia_clip_rect, SkRegion::kIntersect_Op);
  for (size_t i = 0; i < arraysize(draw_rects); ++i) {
    canvas->drawRect(*draw_rects[i], paint);
  }
  canvas->restore();
}

void DrawOutsetRectShadowNode(render_tree::RectShadowNode* node,
                              SkCanvas* canvas) {
#if ENABLE_RENDER_TREE_VISITOR_TRACING
  TRACE_EVENT0("cobalt::renderer", "DrawOutsetRectShadowNode()");
#endif
  const math::RectF& rect = node->data().rect;

  SkRect skia_clip_rect = CobaltRectFToSkiaRect(rect);

  const math::RectF shadow_rect(GetShadowRect(*node).rect);
  SkRect skia_draw_rect = CobaltRectFToSkiaRect(shadow_rect);

  SkPaint paint = GetPaintForBoxShadow(node->data().shadow);

  if (node->data().inset) {
    std::swap(skia_clip_rect, skia_draw_rect);
  }

  canvas->save();
  canvas->clipRect(skia_clip_rect, SkClipOp::kDifference, true);
  canvas->drawRect(skia_draw_rect, paint);
  canvas->restore();
}

void DrawRoundedRectShadowNode(render_tree::RectShadowNode* node,
                               SkCanvas* canvas) {
  RRect shadow_rrect = GetShadowRect(*node);

  canvas->save();

  SkRRect skia_clip_rrect =
      RoundedRectToSkia(node->data().rect, *node->data().rounded_corners);
  SkRRect skia_shadow_rrect =
      RoundedRectToSkia(shadow_rrect.rect, *shadow_rrect.rounded_corners);

  if (node->data().inset) {
    // Calculate the outer rect to be large enough such that blurring the outer
    // unseen edge will not affect the visible inner region of the shadow.
    math::RectF outer_rect = shadow_rrect.rect;
    math::Vector2dF common_outer_rect_outset =
        node->data().shadow.BlurExtent() +
        math::Vector2dF(node->data().spread, node->data().spread);
    math::InsetsF outer_rect_outset(
        common_outer_rect_outset.x() + node->data().shadow.offset.x(),
        common_outer_rect_outset.y() + node->data().shadow.offset.y(),
        common_outer_rect_outset.x() - node->data().shadow.offset.x(),
        common_outer_rect_outset.y() - node->data().shadow.offset.y());
    outer_rect_outset.set_left(std::max(0.0f, outer_rect_outset.left()));
    outer_rect_outset.set_top(std::max(0.0f, outer_rect_outset.top()));
    outer_rect_outset.set_right(std::max(0.0f, outer_rect_outset.right()));
    outer_rect_outset.set_bottom(std::max(0.0f, outer_rect_outset.bottom()));
    outer_rect.Outset(outer_rect_outset);

    SkRRect skia_outer_draw_rrect =
        RoundedRectToSkia(outer_rect, *shadow_rrect.rounded_corners);

    canvas->clipRRect(skia_clip_rrect, SkClipOp::kIntersect, true);

    SkPaint paint = GetPaintForBoxShadow(node->data().shadow);
    paint.setAntiAlias(true);

    // Draw a rounded rect "ring".  We draw a ring whose outer rectangle is
    // larger than the clip rectangle so that we can blur it without the outer
    // edge's blur reaching the visible region.
    canvas->drawDRRect(skia_outer_draw_rrect, skia_shadow_rrect, paint);
  } else {
    // Draw a rounded rectangle, possibly blurred, as the shadow rectangle,
    // clipped by node->data().rect.
    canvas->clipRRect(skia_clip_rrect, SkClipOp::kDifference, true);

    SkPaint paint = GetPaintForBoxShadow(node->data().shadow);
    paint.setAntiAlias(true);
    canvas->drawRRect(skia_shadow_rrect, paint);
  }

  canvas->restore();
}

}  // namespace

void RenderTreeNodeVisitor::Visit(
    render_tree::RectShadowNode* rect_shadow_node) {
#if ENABLE_RENDER_TREE_VISITOR_TRACING
  TRACE_EVENT0("cobalt::renderer", "Visit(RectShadowNode)");
#endif
  DCHECK_EQ(1.0f, draw_state_.opacity);

  if (rect_shadow_node->data().rounded_corners) {
    DrawRoundedRectShadowNode(rect_shadow_node, draw_state_.render_target);
  } else {
    if (rect_shadow_node->data().inset) {
      DrawInsetRectShadowNode(rect_shadow_node, draw_state_.render_target);
    } else {
      DrawOutsetRectShadowNode(rect_shadow_node, draw_state_.render_target);
    }
  }

#if ENABLE_FLUSH_AFTER_EVERY_NODE
  draw_state_.render_target->flush();
#endif
}

namespace {
void RenderText(SkCanvas* render_target,
                const scoped_refptr<render_tree::GlyphBuffer>& glyph_buffer,
                const render_tree::ColorRGBA& color,
                const math::PointF& position, float blur_sigma) {
#if ENABLE_RENDER_TREE_VISITOR_TRACING
  TRACE_EVENT0("cobalt::renderer", "RenderText()");
#endif
  if (blur_sigma > 20.0f) {
    // TODO: We could easily switch to using a blur filter at this point.
    //       Ideally we would just use a blur filter to do all blurred text
    //       rendering. Unfortunately, performance is currently terrible when
    //       using blur filters. Since we don't use blur filters (instead the
    //       blur is rasterized into the glyph atlas by software), we choose not
    //       to snap to using them when the blur reaches a certain point to
    //       avoid discontinuity in blur appearance and performance issues.
    NOTIMPLEMENTED() << "Cobalt does not yet support text blurs with Gaussian "
                        "sigmas larger than 20.";
  } else {
    GlyphBuffer* skia_glyph_buffer =
        base::polymorphic_downcast<GlyphBuffer*>(glyph_buffer.get());

    SkPaint paint(Font::GetDefaultSkPaint());
    paint.setARGB(color.a() * 255, color.r() * 255, color.g() * 255,
                  color.b() * 255);

    if (blur_sigma > 0.0f) {
      sk_sp<SkMaskFilter> mf(
          SkBlurMaskFilter::Make(kNormal_SkBlurStyle, blur_sigma,
                                 SkBlurMaskFilter::kHighQuality_BlurFlag));
      paint.setMaskFilter(mf);
    }

    sk_sp<const SkTextBlob> text_blob(skia_glyph_buffer->GetTextBlob());
    render_target->drawTextBlob(text_blob.get(), position.x(), position.y(),
                                paint);
  }
}
}  // namespace

void RenderTreeNodeVisitor::Visit(render_tree::TextNode* text_node) {
#if ENABLE_RENDER_TREE_VISITOR_TRACING && !FILTER_RENDER_TREE_VISITOR_TRACING
  TRACE_EVENT0("cobalt::renderer", "Visit(TextNode)");
#endif
  DCHECK_EQ(1.0f, draw_state_.opacity);

  // If blur was used for any of the shadows, apply a little bit of blur
  // to all of them, to ensure that Skia follows the same path for rendering
  // them all (i.e. we don't want the main text to use distance field rendering
  // and the other text use standard bitmap font rendering), which ensures that
  // they will be pixel-aligned with each other.
  float blur_zero_sigma = 0.0f;

  if (text_node->data().shadows) {
    const std::vector<render_tree::Shadow>& shadows =
        *text_node->data().shadows;

    for (size_t i = 0; i < shadows.size(); ++i) {
      if (shadows[i].blur_sigma > 0.0f) {
        // At least one shadow is using a blur, so set a small blur on all
        // text renders.
        blur_zero_sigma = 0.000001f;
        break;
      }
    }

    int num_shadows = static_cast<int>(shadows.size());
    for (int i = num_shadows - 1; i >= 0; --i) {
      // Shadows are rendered in back to front order.
      const render_tree::Shadow& shadow = shadows[i];

      RenderText(
          draw_state_.render_target, text_node->data().glyph_buffer,
          shadow.color, math::PointAtOffsetFromOrigin(text_node->data().offset +
                                                      shadow.offset),
          shadow.blur_sigma == 0.0f ? blur_zero_sigma : shadow.blur_sigma);
    }
  }

  // Finally render the main text.
  RenderText(draw_state_.render_target, text_node->data().glyph_buffer,
             text_node->data().color,
             math::PointAtOffsetFromOrigin(text_node->data().offset),
             blur_zero_sigma);

#if ENABLE_FLUSH_AFTER_EVERY_NODE
  draw_state_.render_target->flush();
#endif
}

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
