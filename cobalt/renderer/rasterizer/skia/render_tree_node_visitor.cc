/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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
#include "cobalt/renderer/rasterizer/skia/cobalt_skia_type_conversions.h"
#include "cobalt/renderer/rasterizer/skia/font.h"
#include "cobalt/renderer/rasterizer/skia/glyph_buffer.h"
#include "cobalt/renderer/rasterizer/skia/image.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/effects/SkYUV2RGBShader.h"

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

// Setting this define to 1 will result in a SkCanvas::flush() call being made
// after each node is visited.  This is useful for debugging Skia code since
// otherwise the actual execution of Skia commands will likely be queued and
// deferred for later execution by skia.
#define ENABLE_FLUSH_AFTER_EVERY_NODE 0

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

void ConvertMatrixFromCobaltToSkia(const math::Matrix3F& cobalt_matrix,
                                   SkMatrix* skia_matrix) {
  // Shorten the variable name.
  const math::Matrix3F& cm = cobalt_matrix;

  skia_matrix->setAll(
      cm.Get(0, 0), cm.Get(0, 1), cm.Get(0, 2),
      cm.Get(1, 0), cm.Get(1, 1), cm.Get(1, 2),
      cm.Get(2, 0), cm.Get(2, 1), cm.Get(2, 2));
}

float GetTransformedVectorScale(const SkMatrix& matrix,
                                const SkVector& vector) {
  SkVector result;
  matrix.mapVectors(&result, &vector, 1);
  return sqrt(result.x() * result.x() + result.y() * result.y());
}

SkiaRenderTreeNodeVisitor::SkiaRenderTreeNodeVisitor(
    SkCanvas* render_target,
    const CreateScratchSurfaceFunction* create_scratch_surface_function)
    : render_target_(render_target),
      create_scratch_surface_function_(create_scratch_surface_function) {}

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

void SkiaRenderTreeNodeVisitor::Visit(
    render_tree::CompositionNode* composition_node) {
#if ENABLE_RENDER_TREE_VISITOR_TRACING
  TRACE_EVENT0("cobalt::renderer", "Visit(CompositionNode)");
#endif

  const render_tree::CompositionNode::Children& children =
      composition_node->data().children();

  if (children.empty()) {
    return;
  }

  render_target_->translate(composition_node->data().offset().x(),
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
    render_target_->getClipDeviceBounds(&canvas_boundsi);
    canvas_bounds = SkRect::Make(canvas_boundsi);
    total_matrix = render_target_->getTotalMatrix();
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

  render_target_->translate(-composition_node->data().offset().x(),
                            -composition_node->data().offset().y());

#if ENABLE_FLUSH_AFTER_EVERY_NODE
  render_target_->flush();
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
    SkCanvas* canvas,
    const base::optional<render_tree::ViewportFilter>& filter) {
  if (!filter) {
    return;
  }

  if (!filter->has_rounded_corners()) {
    SkRect filter_viewport(CobaltRectFToSkiaRect(filter->viewport()));
    canvas->clipRect(filter_viewport);
  } else {
    canvas->clipPath(
        RoundedRectToSkiaPath(filter->viewport(), filter->rounded_corners()),
        SkRegion::kIntersect_Op, true /* doAntiAlias */);
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
    SkAutoTUnref<SkBlurImageFilter> skia_blur_filter(SkBlurImageFilter::Create(
        blur_filter->blur_sigma(), blur_filter->blur_sigma()));
    paint->setImageFilter(skia_blur_filter.get());
  }
}

}  // namespace

void SkiaRenderTreeNodeVisitor::RenderFilterViaOffscreenSurface(
    const render_tree::FilterNode::Builder& filter_node) {
  const SkMatrix& total_matrix = render_target_->getTotalMatrix();

  SkIRect canvas_boundsi;
  render_target_->getClipDeviceBounds(&canvas_boundsi);
  SkRect canvas_bounds = SkRect::Make(canvas_boundsi);

  SkMatrix total_matrix_inverse;
  if (!total_matrix.invert(&total_matrix_inverse)) {
    // If the total matrix is not invertible, then its determinant is 0 and so
    // it maps all areas to 0, and so we have nothing to render in this case.
    return;
  }

  // Intersect the filter bounds with the canvas bounds so that we know the
  // minimal scratch surface that we should render to.
  math::RectF filter_bounds = filter_node.GetBounds();
  SkRect filter_clipped_bounds;
  total_matrix_inverse.mapRect(&filter_clipped_bounds, canvas_bounds);
  if (!filter_clipped_bounds.intersect(CobaltRectFToSkiaRect(filter_bounds))) {
    // Nothing to do here, the entire filter is outside of the parent viewport
    // so it will not be visible.
    return;
  }

  // Apply the scale from the total matrix first to the source render tree in
  // order to avoid pixellation which could be caused by the scale of filter
  // node.
  SkVector direction_x = {1, 0};
  SkVector direction_y = {0, 1};
  float scale_x = GetTransformedVectorScale(total_matrix, direction_x);
  float scale_y = GetTransformedVectorScale(total_matrix, direction_y);

  // Determine the size needed by our offscreen surface.
  SkRect scaled_filter_bounds = SkRect::MakeXYWH(
      filter_clipped_bounds.x() * scale_x, filter_clipped_bounds.y() * scale_y,
      filter_clipped_bounds.width() * scale_x,
      filter_clipped_bounds.height() * scale_y);

  scaled_filter_bounds.roundOut();

  // Create a scratch surface upon which we will render the source subtree.
  scoped_ptr<ScratchSurface> scratch_surface(
      create_scratch_surface_function_->Run(
          math::Size(static_cast<int>(scaled_filter_bounds.width()),
                     static_cast<int>(scaled_filter_bounds.height()))));
  if (!scratch_surface) {
    DLOG(ERROR) << "Error creating scratch image surface (width = "
                << scaled_filter_bounds.width()
                << ", height = " << scaled_filter_bounds.height()
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
  canvas->translate(-scaled_filter_bounds.x(), -scaled_filter_bounds.y());
  canvas->scale(scale_x, scale_y);

  // Render our source sub-tree into the offscreen surface.
  SkiaRenderTreeNodeVisitor sub_visitor(canvas,
                                        create_scratch_surface_function_);
  filter_node.source->Accept(&sub_visitor);

  // With the source subtree rendered to our temporary scratch surface, we
  // will now render it to our parent render target with an alpha value set to
  // the opacity value.
  SkPaint paint;
  if (filter_node.opacity_filter) {
    paint.setARGB(filter_node.opacity_filter->opacity() * 255, 255, 255, 255);
  }
  // Use nearest neighbor when filtering texture data, since the source and
  // destination rectangles should be exactly equal.
  paint.setFilterLevel(SkPaint::kNone_FilterLevel);

  // We've already used the render_target_'s scale when rendering to the
  // offscreen surface, so reset the scale for now.
  render_target_->save();
  ApplyBlurFilterToPaint(&paint, filter_node.blur_filter);
  ApplyViewportMask(render_target_, filter_node.viewport_filter);
  render_target_->scale(1.0f / scale_x, 1.0f / scale_y);

  SkAutoTUnref<SkImage> image(
      scratch_surface->GetSurface()->newImageSnapshot());
  DCHECK(image);

  SkRect source_rect = SkRect::MakeWH(scaled_filter_bounds.width(),
                                      scaled_filter_bounds.height());

  render_target_->drawImageRect(image, &source_rect, scaled_filter_bounds,
                                &paint);

  // Finally restore our parent render target's original transform for the
  // next draw call.
  render_target_->restore();
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
}  // namespace

void SkiaRenderTreeNodeVisitor::Visit(render_tree::FilterNode* filter_node) {
#if ENABLE_RENDER_TREE_VISITOR_TRACING
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

  const SkMatrix& total_matrix = render_target_->getTotalMatrix();

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
      !filter_node->data().opacity_filter &&
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
    render_target_->save();
    ApplyViewportMask(render_target_, filter_node->data().viewport_filter);
    filter_node->data().source->Accept(this);
    render_target_->restore();
  } else {
    RenderFilterViaOffscreenSurface(filter_node->data());
  }
#if ENABLE_FLUSH_AFTER_EVERY_NODE
  render_target_->flush();
#endif
}

class RasterizeSkiaImageVisitor : public SkiaImageVisitor {
 public:
  RasterizeSkiaImageVisitor(SkCanvas* render_target,
                            const math::RectF& destination_rect,
                            const math::Matrix3F* local_transform)
      : render_target_(render_target),
        destination_rect_(destination_rect),
        local_transform_(local_transform) {}

  void VisitSinglePlaneImage(SkiaSinglePlaneImage* single_plane_image) OVERRIDE;
  void VisitMultiPlaneImage(SkiaMultiPlaneImage* multi_plane_image) OVERRIDE;

 private:
  SkCanvas* render_target_;
  const math::RectF& destination_rect_;
  const math::Matrix3F* local_transform_;
};

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
}  // namespace

void RasterizeSkiaImageVisitor::VisitSinglePlaneImage(
    SkiaSinglePlaneImage* single_plane_image) {
  SkPaint paint;
  paint.setFilterLevel(SkPaint::kLow_FilterLevel);

  // In the most frequent by far case where the normalized transformed image
  // texture coordinates lie within the unit square, then we must ensure NOT
  // to interpolate texel values with wrapped texels across the image borders.
  // This can result in strips of incorrect colors around the edges of images.
  // Thus, if the normalized texture coordinates lie within the unit box, we
  // will blit the image using drawBitmapRectToRect() which handles the bleeding
  // edge problem for us.
  if (IsScaleAndTranslateOnly(*local_transform_) &&
      LocalCoordsStaysWithinUnitBox(*local_transform_)) {
    // Determine the source rectangle, in image texture pixel coordinates.
    const math::Size& img_size = single_plane_image->GetSize();

    float width = img_size.width() / local_transform_->Get(0, 0);
    float height = img_size.height() / local_transform_->Get(1, 1);
    float x = -width * local_transform_->Get(0, 2);
    float y = -height * local_transform_->Get(1, 2);

    SkRect src = SkRect::MakeXYWH(x, y, width, height);

    render_target_->drawBitmapRectToRect(
        single_plane_image->GetBitmap(), &src,
        CobaltRectFToSkiaRect(destination_rect_), &paint);
  } else {
    // Use the more general approach which allows arbitrary local texture
    // coordinate matrices.
    SkMatrix skia_local_transform;
    ConvertMatrixFromCobaltToSkia(*local_transform_, &skia_local_transform);

    ConvertLocalTransformMatrixToSkiaShaderFormat(single_plane_image->GetSize(),
                                                  destination_rect_,
                                                  &skia_local_transform);

    SkAutoTUnref<SkShader> image_shader(SkShader::CreateBitmapShader(
        single_plane_image->GetBitmap(), SkShader::kRepeat_TileMode,
        SkShader::kRepeat_TileMode, &skia_local_transform));
    paint.setShader(image_shader);

    render_target_->drawRect(CobaltRectFToSkiaRect(destination_rect_), paint);
  }
}

void RasterizeSkiaImageVisitor::VisitMultiPlaneImage(
    SkiaMultiPlaneImage* multi_plane_image) {
  DCHECK_EQ(render_tree::kMultiPlaneImageFormatYUV3PlaneBT709,
            multi_plane_image->GetFormat());

  SkMatrix skia_local_transform;
  ConvertMatrixFromCobaltToSkia(*local_transform_, &skia_local_transform);

  const SkBitmap& y_bitmap = multi_plane_image->GetBitmap(0);
  SkMatrix y_matrix = skia_local_transform;
  ConvertLocalTransformMatrixToSkiaShaderFormat(
      math::Size(y_bitmap.width(), y_bitmap.height()), destination_rect_,
      &y_matrix);

  const SkBitmap& u_bitmap = multi_plane_image->GetBitmap(1);
  SkMatrix u_matrix = skia_local_transform;
  ConvertLocalTransformMatrixToSkiaShaderFormat(
      math::Size(u_bitmap.width(), u_bitmap.height()), destination_rect_,
      &u_matrix);

  const SkBitmap& v_bitmap = multi_plane_image->GetBitmap(2);
  SkMatrix v_matrix = skia_local_transform;
  ConvertLocalTransformMatrixToSkiaShaderFormat(
      math::Size(v_bitmap.width(), v_bitmap.height()), destination_rect_,
      &v_matrix);

  SkAutoTUnref<SkShader> yuv2rgb_shader(
      SkNEW_ARGS(SkYUV2RGBShader, (kRec709_SkYUVColorSpace, y_bitmap, y_matrix,
                                   u_bitmap, u_matrix, v_bitmap, v_matrix)));

  SkPaint paint;
  paint.setFilterLevel(SkPaint::kLow_FilterLevel);
  paint.setShader(yuv2rgb_shader);
  render_target_->drawRect(CobaltRectFToSkiaRect(destination_rect_), paint);
}

void SkiaRenderTreeNodeVisitor::Visit(render_tree::ImageNode* image_node) {
  // The image_node may contain nothing. For example, when it represents a video
  // element before any frame is decoded.
  if (!image_node->data().source) {
    return;
  }

#if ENABLE_RENDER_TREE_VISITOR_TRACING
  TRACE_EVENT0("cobalt::renderer", "Visit(ImageNode)");
#endif
  SkiaImage* image =
      base::polymorphic_downcast<SkiaImage*>(image_node->data().source.get());

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
  image->EnsureInitialized();

  // We issue different skia rasterization commands to render the image
  // depending on whether it's single or multi planed.
  RasterizeSkiaImageVisitor image_visitor(
      render_target_, image_node->data().destination_rect,
      &(image_node->data().local_transform));
  image->Accept(&image_visitor);

#if ENABLE_FLUSH_AFTER_EVERY_NODE
  render_target_->flush();
#endif
}

void SkiaRenderTreeNodeVisitor::Visit(
    render_tree::MatrixTransformNode* matrix_transform_node) {
#if ENABLE_RENDER_TREE_VISITOR_TRACING
  TRACE_EVENT0("cobalt::renderer", "Visit(MatrixTransformNode)");
#endif

  // Concatenate the matrix transform to the render target and then continue
  // on with rendering our source.
  render_target_->save();

  SkMatrix skia_matrix;
  ConvertMatrixFromCobaltToSkia(matrix_transform_node->data().transform,
                                &skia_matrix);
  render_target_->concat(skia_matrix);

  matrix_transform_node->data().source->Accept(this);

  render_target_->restore();

#if ENABLE_FLUSH_AFTER_EVERY_NODE
  render_target_->flush();
#endif
}

namespace {

class SkiaBrushVisitor : public render_tree::BrushVisitor {
 public:
  explicit SkiaBrushVisitor(SkPaint* paint) : paint_(paint) {}

  void Visit(
      const cobalt::render_tree::SolidColorBrush* solid_color_brush) OVERRIDE;
  void Visit(const cobalt::render_tree::LinearGradientBrush*
                 linear_gradient_brush) OVERRIDE;
  void Visit(const cobalt::render_tree::RadialGradientBrush*
                 radial_gradient_brush) OVERRIDE;

 private:
  SkPaint* paint_;
};

void SkiaBrushVisitor::Visit(
    const cobalt::render_tree::SolidColorBrush* solid_color_brush) {
  const cobalt::render_tree::ColorRGBA& color = solid_color_brush->color();

  paint_->setARGB(color.a() * 255, color.r() * 255, color.g() * 255,
                  color.b() * 255);
}

namespace {

// Helper struct to convert render_tree::ColorStopList to a format that Skia
// methods will easily accept.
struct SkiaColorStops {
  explicit SkiaColorStops(const render_tree::ColorStopList& color_stops)
      : colors(color_stops.size()), positions(color_stops.size()) {
    for (size_t i = 0; i < color_stops.size(); ++i) {
      colors[i] = ToSkColor(color_stops[i].color);
      positions[i] = color_stops[i].position;
    }
  }

  std::vector<SkColor> colors;
  std::vector<SkScalar> positions;
};

}  // namespace

void SkiaBrushVisitor::Visit(
    const cobalt::render_tree::LinearGradientBrush* linear_gradient_brush) {
  SkPoint points[2] = {
      {linear_gradient_brush->source().x(),
       linear_gradient_brush->source().y()},
      {linear_gradient_brush->dest().x(), linear_gradient_brush->dest().y()}};

  SkiaColorStops skia_color_stops(linear_gradient_brush->color_stops());

  SkAutoTUnref<SkShader> shader(SkGradientShader::CreateLinear(
      points, &skia_color_stops.colors[0], &skia_color_stops.positions[0],
      linear_gradient_brush->color_stops().size(), SkShader::kClamp_TileMode,
      SkGradientShader::kInterpolateColorsInPremul_Flag, NULL));
  paint_->setShader(shader);
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

  SkAutoTUnref<SkShader> shader(SkGradientShader::CreateRadial(
      center, radial_gradient_brush->radius_x(), &skia_color_stops.colors[0],
      &skia_color_stops.positions[0],
      radial_gradient_brush->color_stops().size(), SkShader::kClamp_TileMode,
      SkGradientShader::kInterpolateColorsInPremul_Flag, &local_matrix));
  paint_->setShader(shader);
}

void DrawRectWithBrush(SkCanvas* render_target,
                       cobalt::render_tree::Brush* brush,
                       const math::RectF& rect) {
  // Setup our paint object according to the brush parameters set on the
  // rectangle.
  SkPaint paint;
  SkiaBrushVisitor brush_visitor(&paint);
  brush->Accept(&brush_visitor);
  render_target->drawRect(
      SkRect::MakeXYWH(rect.x(), rect.y(), rect.width(), rect.height()), paint);
}

namespace {
class CheckForSolidBrushVisitor : public render_tree::BrushVisitor {
 public:
  CheckForSolidBrushVisitor() : is_solid_brush_(false) {}

  void Visit(
      const cobalt::render_tree::SolidColorBrush* solid_color_brush) OVERRIDE {
    is_solid_brush_ = true;
  }

  void Visit(const cobalt::render_tree::LinearGradientBrush*
                 linear_gradient_brush) OVERRIDE {}
  void Visit(const cobalt::render_tree::RadialGradientBrush*
                 radial_gradient_brush) OVERRIDE {}

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
    SkCanvas* render_target, render_tree::Brush* brush, const math::RectF& rect,
    const render_tree::RoundedCorners& rounded_corners) {
  if (!CheckForSolidBrush(brush)) {
    NOTREACHED() << "Only solid brushes are currently supported for this shape "
                    "in Cobalt.";
    return;
  }

  SkPaint paint;
  SkiaBrushVisitor brush_visitor(&paint);
  brush->Accept(&brush_visitor);

  paint.setAntiAlias(true);
  render_target->drawPath(RoundedRectToSkiaPath(rect, rounded_corners), paint);
}

void DrawQuadWithColorIfBorderIsSolid(render_tree::BorderStyle border_style,
                                      SkCanvas* render_target,
                                      const render_tree::ColorRGBA& color,
                                      const SkPoint* points, bool anti_alias) {
  if (border_style == render_tree::kBorderStyleSolid) {
    SkPaint paint;
    paint.setARGB(color.a() * 255, color.r() * 255, color.g() * 255,
                  color.b() * 255);
    paint.setAntiAlias(anti_alias);

    SkPath path;
    path.addPoly(points, 4, true);
    render_target->drawPath(path, paint);
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
void DrawSolidNonRoundRectBorder(SkCanvas* render_target,
                                 const math::RectF& rect,
                                 const render_tree::Border& border) {
  bool anti_alias = true;

  if (border.top.color == border.left.color &&
      border.top.color == border.bottom.color &&
      border.top.color == border.right.color) {
    // Disable the anti-alias when the borders have the same color.
    anti_alias = false;
  }

  // Top
  SkPoint top_points[4] = {
      {rect.x(), rect.y()},                                              // A
      {rect.x() + border.left.width, rect.y() + border.top.width},       // E
      {rect.right() - border.right.width, rect.y() + border.top.width},  // F
      {rect.right(), rect.y()}};                                         // B
  DrawQuadWithColorIfBorderIsSolid(border.top.style, render_target,
                                   border.top.color, top_points, anti_alias);

  // Left
  SkPoint left_points[4] = {
      {rect.x(), rect.y()},                                                 // A
      {rect.x(), rect.bottom()},                                            // C
      {rect.x() + border.left.width, rect.bottom() - border.bottom.width},  // G
      {rect.x() + border.left.width, rect.y() + border.top.width}};         // E
  DrawQuadWithColorIfBorderIsSolid(border.left.style, render_target,
                                   border.left.color, left_points, anti_alias);

  // Bottom
  SkPoint bottom_points[4] = {
      {rect.x() + border.left.width, rect.bottom() - border.bottom.width},  // G
      {rect.x(), rect.bottom()},                                            // C
      {rect.right(), rect.bottom()},                                        // D
      {rect.right() - border.right.width,
       rect.bottom() - border.bottom.width}};  // H
  DrawQuadWithColorIfBorderIsSolid(border.bottom.style, render_target,
                                   border.bottom.color, bottom_points,
                                   anti_alias);

  // Right
  SkPoint right_points[4] = {
      {rect.right() - border.right.width, rect.y() + border.top.width},  // F
      {rect.right() - border.right.width,
       rect.bottom() - border.bottom.width},  // H
      {rect.right(), rect.bottom()},          // D
      {rect.right(), rect.y()}};              // B
  DrawQuadWithColorIfBorderIsSolid(border.right.style, render_target,
                                   border.right.color, right_points,
                                   anti_alias);
}

void DrawSolidRoundedRectBorderToRenderTarget(
    SkCanvas* render_target, const math::RectF& rect,
    const render_tree::RoundedCorners& rounded_corners,
    const math::RectF& content_rect,
    const render_tree::RoundedCorners& inner_rounded_corners,
    const render_tree::Border& border) {
  SkPaint paint;
  paint.setAntiAlias(true);

  const render_tree::ColorRGBA& color = border.top.color;
  paint.setARGB(color.a() * 255, color.r() * 255, color.g() * 255,
                color.b() * 255);

  render_target->drawDRRect(
      RoundedRectToSkia(rect, rounded_corners),
      RoundedRectToSkia(content_rect, inner_rounded_corners), paint);
}

void DrawSolidRoundedRectBorderSoftware(
    SkCanvas* render_target, const math::RectF& rect,
    const render_tree::RoundedCorners& rounded_corners,
    const math::RectF& content_rect,
    const render_tree::RoundedCorners& inner_rounded_corners,
    const render_tree::Border& border) {
  SkImageInfo image_info =
      SkImageInfo::MakeN32(rect.width(), rect.height(), kPremul_SkAlphaType);
  SkBitmap bitmap;
  bitmap.allocPixels(image_info);

  SkCanvas canvas(bitmap);
  canvas.clear(SkColorSetARGB(0, 0, 0, 0));

  math::RectF canvas_rect(rect.size());
  math::RectF canvas_content_rect(content_rect);
  canvas_content_rect.Offset(-rect.OffsetFromOrigin());
  DrawSolidRoundedRectBorderToRenderTarget(&canvas, canvas_rect,
                                           rounded_corners, canvas_content_rect,
                                           inner_rounded_corners, border);
  canvas.flush();

  SkPaint render_target_paint;
  render_target_paint.setFilterLevel(SkPaint::kNone_FilterLevel);
  render_target->drawBitmap(bitmap, rect.x(), rect.y(), &render_target_paint);
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

bool ValidateSolidBorderProperties(const render_tree::Border& border) {
  if ((border.top == border.left) &&
      (border.top == border.right) &&
      (border.top == border.bottom) &&
      (border.top.style == render_tree::kBorderStyleSolid)) {
    return true;
  } else {
    DLOG(ERROR) << "Border sides have different properties: " << border;
    return false;
  }
}

}  // namespace

void DrawSolidRoundedRectBorder(
    SkCanvas* render_target, const math::RectF& rect,
    const render_tree::RoundedCorners& rounded_corners,
    const math::RectF& content_rect,
    const render_tree::RoundedCorners& inner_rounded_corners,
    const render_tree::Border& border) {
  // We only support rendering of rounded corner borders if each side has
  // the same properties.
  DCHECK(ValidateSolidBorderProperties(border));

  if (IsCircle(rect.size(), rounded_corners)) {
    // We are able to render circular borders using hardware, so introduce
    // a special case for them.
    DrawSolidRoundedRectBorderToRenderTarget(render_target, rect,
                                             rounded_corners, content_rect,
                                             inner_rounded_corners, border);
  } else {
    // For now we fallback to software for drawing most rounded corner borders,
    // with some situations specified above being special cased. The reason we
    // do this is to limit then number of shaders that need to be implemented.
    NOTIMPLEMENTED() << "Warning: Software rasterizing a solid rectangle "
                        "border.";
    DrawSolidRoundedRectBorderSoftware(render_target, rect, rounded_corners,
                                       content_rect, inner_rounded_corners,
                                       border);
  }
}

}  // namespace

void SkiaRenderTreeNodeVisitor::Visit(render_tree::RectNode* rect_node) {
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
      DrawRoundedRectWithBrush(render_target_,
                               rect_node->data().background_brush.get(),
                               content_rect, *inner_rounded_corners);
    } else {
      DrawRectWithBrush(render_target_,
                        rect_node->data().background_brush.get(), content_rect);
    }
  }

  if (rect_node->data().border) {
    if (rect_node->data().rounded_corners) {
      DrawSolidRoundedRectBorder(
          render_target_, rect, *rect_node->data().rounded_corners,
          content_rect, *inner_rounded_corners, *rect_node->data().border);
    } else {
      DrawSolidNonRoundRectBorder(render_target_, rect,
                                  *rect_node->data().border);
    }
  }

#if ENABLE_FLUSH_AFTER_EVERY_NODE
  render_target_->flush();
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

  SkAutoTUnref<SkMaskFilter> mf(
      SkBlurMaskFilter::Create(kNormal_SkBlurStyle, shadow.blur_sigma));
  paint.setMaskFilter(mf.get());

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
  canvas->clipRect(skia_clip_rect, SkRegion::kDifference_Op, true);
  canvas->drawRect(skia_draw_rect, paint);
  canvas->restore();
}

void DrawRoundedRectShadowNode(render_tree::RectShadowNode* node,
                               SkCanvas* canvas) {
  if (node->data().shadow.blur_sigma > 0.0f) {
    NOTIMPLEMENTED() << "Cobalt doesn't currently support rendering blurred "
                        "box shadows on rounded rectangles.";
    return;
  }
  if (!IsCircle(node->data().rect.size(), *node->data().rounded_corners)) {
    NOTIMPLEMENTED() << "Cobalt does not support box shadows with rounded "
                        "corners that are not circles.";
    return;
  }

  RRect shadow_rrect = GetShadowRect(*node);

  SkRRect skia_clip_rrect =
      RoundedRectToSkia(node->data().rect, *node->data().rounded_corners);
  SkRRect skia_draw_rrect =
      RoundedRectToSkia(shadow_rrect.rect, *shadow_rrect.rounded_corners);

  if (node->data().inset) {
    std::swap(skia_clip_rrect, skia_draw_rrect);
  }

  canvas->save();

  canvas->clipRRect(skia_clip_rrect, SkRegion::kDifference_Op, true);

  SkPaint paint = GetPaintForBoxShadow(node->data().shadow);
  paint.setAntiAlias(true);
  canvas->drawRRect(skia_draw_rrect, paint);

  canvas->restore();
}

}  // namespace

void SkiaRenderTreeNodeVisitor::Visit(
    render_tree::RectShadowNode* rect_shadow_node) {
#if ENABLE_RENDER_TREE_VISITOR_TRACING
  TRACE_EVENT0("cobalt::renderer", "Visit(RectShadowNode)");
#endif
  if (rect_shadow_node->data().rounded_corners) {
    DrawRoundedRectShadowNode(rect_shadow_node, render_target_);
  } else {
    if (rect_shadow_node->data().inset) {
      DrawInsetRectShadowNode(rect_shadow_node, render_target_);
    } else {
      DrawOutsetRectShadowNode(rect_shadow_node, render_target_);
    }
  }

#if ENABLE_FLUSH_AFTER_EVERY_NODE
  render_target_->flush();
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
    // TODO(***REMOVED***): We could easily switch to using a blur filter at this
    //               point. Ideally we would just use a blur filter to do all
    //               blurred text rendering.  Unfortunately, performance is
    //               currently terrible when using blur filters.  Since we
    //               don't use blur filters (instead the blur is rasterized into
    //               the glyph atlas by software), we choose not to snap to
    //               using them when the blur reaches a certain point to avoid
    //               discontinuity in blur appearance and performance issues.
    NOTIMPLEMENTED() << "Cobalt does not yet support text blurs with Gaussian "
                        "sigmas larger than 20.";
  } else {
    SkiaGlyphBuffer* skia_glyph_buffer =
        base::polymorphic_downcast<SkiaGlyphBuffer*>(glyph_buffer.get());

    SkPaint paint(SkiaFont::GetDefaultSkPaint());
    paint.setARGB(color.a() * 255, color.r() * 255, color.g() * 255,
                  color.b() * 255);

    if (blur_sigma > 0.0f) {
      SkAutoTUnref<SkMaskFilter> mf(
          SkBlurMaskFilter::Create(kNormal_SkBlurStyle, blur_sigma,
                                   SkBlurMaskFilter::kHighQuality_BlurFlag));
      paint.setMaskFilter(mf.get());
    }

    SkAutoTUnref<const SkTextBlob> text_blob(skia_glyph_buffer->GetTextBlob());
    render_target->drawTextBlob(text_blob.get(), position.x(), position.y(),
                                paint);
  }
}
}  // namespace

void SkiaRenderTreeNodeVisitor::Visit(render_tree::TextNode* text_node) {
#if ENABLE_RENDER_TREE_VISITOR_TRACING
  TRACE_EVENT0("cobalt::renderer", "Visit(TextNode)");
#endif
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
          render_target_, text_node->data().glyph_buffer, shadow.color,
          math::PointAtOffsetFromOrigin(text_node->data().offset +
                                        shadow.offset),
          shadow.blur_sigma == 0.0f ? blur_zero_sigma : shadow.blur_sigma);
    }
  }

  // Finally render the main text.
  RenderText(
      render_target_, text_node->data().glyph_buffer, text_node->data().color,
      math::PointAtOffsetFromOrigin(text_node->data().offset), blur_zero_sigma);

#if ENABLE_FLUSH_AFTER_EVERY_NODE
  render_target_->flush();
#endif
}

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
