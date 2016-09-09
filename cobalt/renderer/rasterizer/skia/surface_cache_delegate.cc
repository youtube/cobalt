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

#include "cobalt/renderer/rasterizer/skia/surface_cache_delegate.h"

#include <string>

#include "base/debug/trace_event.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/math/transform_2d.h"
#include "cobalt/math/vector2d_f.h"
#include "cobalt/math/vector3d_f.h"
#include "cobalt/renderer/rasterizer/skia/cobalt_skia_type_conversions.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

SurfaceCacheDelegate::SurfaceCacheDelegate(
    const CreateSkSurfaceFunction& create_sk_surface_function,
    const math::Size& max_surface_size)
    : create_sk_surface_function_(create_sk_surface_function),
      max_surface_size_(max_surface_size),
      draw_state_(NULL)
#if defined(ENABLE_DEBUG_CONSOLE)
      ,
      toggle_cache_highlights_(false),
      toggle_cache_highlights_command_handler_(
          "toggle_cache_highlights",
          base::Bind(&SurfaceCacheDelegate::OnToggleCacheHighlights,
                     base::Unretained(this)),
          "Toggles cache highlights showing render tree nodes that are cached.",
          "Will replace render tree node images with translucent red "
          "rectangles so that cached nodes can be visualized on screen.")
#endif
{
}

void SurfaceCacheDelegate::UpdateCanvasScale() {
  math::Matrix3F total_matrix =
      SkiaMatrixToCobalt(draw_state_->render_target->getTotalMatrix());
  math::Vector3dF column_0(total_matrix.column(0));
  math::Vector3dF column_1(total_matrix.column(1));
  scale_ = math::Vector2dF(column_0.Length(), column_1.Length());
}

#if defined(ENABLE_DEBUG_CONSOLE)
void SurfaceCacheDelegate::OnToggleCacheHighlights(const std::string& message) {
  toggle_cache_highlights_ = !toggle_cache_highlights_;
}
#endif

void SurfaceCacheDelegate::ApplySurface(
    common::SurfaceCache::CachedSurface* surface) {
  TRACE_EVENT0("cobalt::renderer", "SurfaceCacheDelegate::ApplySurface()");
  CachedSurface* skia_surface =
      base::polymorphic_downcast<CachedSurface*>(surface);

  if (!skia_surface->image()) {
    return;
  }

  draw_state_->render_target->save();
  SkMatrix total_matrix = draw_state_->render_target->getTotalMatrix();
  // We "preScale()" the "post scale" because skia uses "pre" to mean that the
  // transform will be applied before the other transforms, whereas
  // common::OffscreenRenderCoordinateMapping() uses "post" to mean that the
  // transform matrix should be post-multiplied by the existing matrix.
  total_matrix.preScale(skia_surface->output_post_scale().x(),
                        skia_surface->output_post_scale().y());
  total_matrix.postTranslate(skia_surface->output_pre_translate().x(),
                             skia_surface->output_pre_translate().y());
  draw_state_->render_target->setMatrix(total_matrix);

  SkPaint paint;
  paint.setFilterLevel(SkPaint::kLow_FilterLevel);
  if (draw_state_->opacity < 1.0f) {
    paint.setAlpha(draw_state_->opacity * 255);
  }

  SkRect dest_rect = SkRect::MakeXYWH(skia_surface->output_bounds().x(),
                                      skia_surface->output_bounds().y(),
                                      skia_surface->output_bounds().width(),
                                      skia_surface->output_bounds().height());

  SkRect source_rect = SkRect::MakeWH(skia_surface->output_bounds().width(),
                                      skia_surface->output_bounds().height());

#if defined(ENABLE_DEBUG_CONSOLE)
  if (toggle_cache_highlights_) {
    paint.setARGB(128, 255, 128, 128);
    draw_state_->render_target->drawRect(dest_rect, paint);
  } else  // NOLINT(readability/braces)
#endif
  {
    draw_state_->render_target->drawImageRect(skia_surface->image(),
                                              &source_rect, dest_rect, &paint);
  }

  draw_state_->render_target->restore();
}

void SurfaceCacheDelegate::StartRecording(const math::RectF& local_bounds) {
  TRACE_EVENT0("cobalt::renderer", "SurfaceCacheDelegate::StartRecording()");
  DCHECK(!recording_data_);

  // If the scale is less than one, render to the offscreen surface as if the
  // scale is 1, since the scale may be animating and this way we do not need
  // to rerender the subtree each frame of the animation.  If the scale is
  // larger than 1, we cache it as is and if it is animating, performance will
  // suffer.
  math::Vector2dF inv_scale(scale_.x() < 1.0f ? 1.0f / scale_.x() : 1.0f,
                            scale_.y() < 1.0f ? 1.0f / scale_.y() : 1.0f);
  math::Matrix3F total_matrix =
      SkiaMatrixToCobalt(draw_state_->render_target->getTotalMatrix()) *
      math::ScaleMatrix(inv_scale);

  // Figure where we should render to the offscreen surface and then how to map
  // that later to the onscreen surface when applying the cached surface.  We
  // ignore the viewport so that we cache all of partially on-screen surfaces.
  common::OffscreenRenderCoordinateMapping coord_mapping =
      common::GetOffscreenRenderCoordinateMapping(local_bounds, total_matrix,
                                                  base::optional<math::Rect>());
  // If the output has an area of 0 then there is nothing to cache.  This should
  // not be common.
  if (coord_mapping.output_bounds.size().GetArea() == 0) {
    recording_data_.emplace(*draw_state_, coord_mapping,
                            static_cast<SkSurface*>(NULL));
    return;
  }

  // Now create a SkSurface and set it up as the new canvas for our client to
  // target.
  SkSurface* surface = create_sk_surface_function_.Run(
      math::Size(coord_mapping.output_bounds.width(),
                 coord_mapping.output_bounds.height()));
  CHECK(surface);

  recording_data_.emplace(*draw_state_, coord_mapping, surface);

  SkCanvas* offscreen_canvas = surface->getCanvas();
  draw_state_->render_target = offscreen_canvas;
  draw_state_->opacity = 1.0f;

  // Clear the draw area to RGBA(0, 0, 0, 0) for a fresh scratch surface before
  // returning.
  draw_state_->render_target->drawARGB(0, 0, 0, 0, SkXfermode::kClear_Mode);

  draw_state_->render_target->setMatrix(
      CobaltMatrixToSkia(coord_mapping.sub_render_transform));
}

common::SurfaceCache::CachedSurface* SurfaceCacheDelegate::EndRecording() {
  TRACE_EVENT0("cobalt::renderer", "SurfaceCacheDelegate::EndRecording()");
  DCHECK(recording_data_);

  *draw_state_ = recording_data_->original_draw_state;

  // Save the results as a CachedSurface and return them.
  CachedSurface* cached_surface =
      new CachedSurface(recording_data_->surface,
                        recording_data_->coord_mapping.output_post_scale,
                        recording_data_->coord_mapping.output_pre_translate,
                        recording_data_->coord_mapping.output_bounds);

  recording_data_ = base::nullopt;

  return cached_surface;
}

void SurfaceCacheDelegate::ReleaseSurface(
    common::SurfaceCache::CachedSurface* surface) {
  TRACE_EVENT0("cobalt::renderer", "SurfaceCacheDelegate::ReleaseSurface()");
  delete surface;
}

math::Size SurfaceCacheDelegate::GetRenderSize(const math::SizeF& local_size) {
  math::Size size(static_cast<int>(local_size.width() * scale_.x() + 0.5f),
                  static_cast<int>(local_size.height() * scale_.y() + 0.5f));
  return size;
}

math::Size SurfaceCacheDelegate::MaximumSurfaceSize() {
  return max_surface_size_;
}

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
