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

#include "cobalt/renderer/rasterizer/blitter/surface_cache_delegate.h"

#include <string>

#include "base/debug/trace_event.h"
#include "cobalt/base/polymorphic_downcast.h"
#include "cobalt/math/transform_2d.h"
#include "cobalt/math/vector2d_f.h"
#include "cobalt/math/vector3d_f.h"
#include "cobalt/renderer/rasterizer/blitter/cobalt_blitter_conversions.h"
#include "cobalt/renderer/rasterizer/skia/cobalt_skia_type_conversions.h"
#include "starboard/blitter.h"

#if SB_HAS(BLITTER)

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace blitter {

SurfaceCacheDelegate::SurfaceCacheDelegate(SbBlitterDevice device,
                                           SbBlitterContext context)
    : device_(device),
      context_(context)
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

#if defined(ENABLE_DEBUG_CONSOLE)
void SurfaceCacheDelegate::OnToggleCacheHighlights(const std::string& message) {
  toggle_cache_highlights_ = !toggle_cache_highlights_;
}
#endif

void SurfaceCacheDelegate::ApplySurface(
    common::SurfaceCache::CachedSurface* surface) {
  TRACE_EVENT0("cobalt::renderer", "SurfaceCacheDelegate::ApplySurface()");
  CachedSurface* blitter_surface =
      base::polymorphic_downcast<CachedSurface*>(surface);

  if (!SbBlitterIsSurfaceValid(blitter_surface->surface())) {
    return;
  }

  // First, post-apply our scale to our current transform.
  Transform apply_transform(render_state_->transform);
  apply_transform.ApplyScale(blitter_surface->output_post_scale());
  math::RectF output_rectf =
      apply_transform.TransformRect(blitter_surface->output_bounds());
  // We can simulate a "pre-multiply" by translation by offsetting the final
  // output rectangle by the pre-translate, effectively resulting in the
  // translation being applied last, as intended.
  output_rectf.Offset(blitter_surface->output_pre_translate());
  SbBlitterRect output_blitter_rect = RectFToBlitterRect(output_rectf);

#if defined(ENABLE_DEBUG_CONSOLE)
  if (toggle_cache_highlights_) {
    SbBlitterSetColor(context_, SbBlitterColorFromRGBA(255, 128, 128, 128));
    SbBlitterSetBlending(context_, true);
    SbBlitterFillRect(context_, output_blitter_rect);
  } else  // NOLINT(readability/braces)
#endif
  {
    // Render the cached surface to the output render target.
    SbBlitterSetBlending(context_, true);
    SbBlitterSetModulateBlitsWithColor(context_, false);
    SbBlitterBlitRectToRect(
        context_, blitter_surface->surface(),
        SbBlitterMakeRect(0, 0, blitter_surface->output_bounds().width(),
                          blitter_surface->output_bounds().height()),
        output_blitter_rect);
  }
}

void SurfaceCacheDelegate::StartRecording(const math::RectF& local_bounds) {
  TRACE_EVENT0("cobalt::renderer", "SurfaceCacheDelegate::StartRecording()");
  DCHECK(!recording_data_);

  // If the scale is less than one, render to the offscreen surface as if the
  // scale is 1, since the scale may be animating and this way we do not need
  // to rerender the subtree each frame of the animation.  If the scale is
  // larger than 1, we cache it as is and if it is animating, performance will
  // suffer.
  math::Vector2dF scale = render_state_->transform.scale();
  math::Vector2dF inv_scale(scale.x() < 1.0f ? 1.0f / scale.x() : 1.0f,
                            scale.y() < 1.0f ? 1.0f / scale.y() : 1.0f);
  Transform inv_scale_transform(render_state_->transform);
  inv_scale_transform.ApplyScale(inv_scale);

  // Figure where we should render to the offscreen surface and then how to map
  // that later to the onscreen surface when applying the cached surface.  We
  // ignore the viewport so that we cache all of partially on-screen surfaces.
  common::OffscreenRenderCoordinateMapping coord_mapping =
      common::GetOffscreenRenderCoordinateMapping(
          local_bounds, inv_scale_transform.ToMatrix(),
          base::optional<math::Rect>());

  // If the output has an area of 0 then there is nothing to cache.  This should
  // not be common.
  if (coord_mapping.output_bounds.size().GetArea() == 0) {
    recording_data_.emplace(*render_state_, coord_mapping,
                            kSbBlitterInvalidSurface);
    return;
  }

  // Now create a surface and set it up as the new render target for our client
  // to target.
  SbBlitterSurface surface = SbBlitterCreateRenderTargetSurface(
      device_, coord_mapping.output_bounds.width(),
      coord_mapping.output_bounds.height(), kSbBlitterSurfaceFormatRGBA8);
  CHECK(SbBlitterIsSurfaceValid(surface));

  // Setup our offscreen surface as the render target for use while recording.
  SbBlitterRenderTarget offscreen_render_target =
      SbBlitterGetRenderTargetFromSurface(surface);
  CHECK(SbBlitterIsRenderTargetValid(offscreen_render_target));

  recording_data_.emplace(*render_state_, coord_mapping, surface);

  // Set our new render state to the recording render state.
  render_state_ = set_render_state_function_.Run(RenderState(
      offscreen_render_target, Transform(coord_mapping.sub_render_transform),
      BoundsStack(context_, math::Rect(coord_mapping.output_bounds.size()))));
  SbBlitterSetRenderTarget(context_, render_state_->render_target);
  render_state_->bounds_stack.UpdateContext();

  // Clear the draw area to RGBA(0, 0, 0, 0) for a fresh scratch surface before
  // returning.
  SbBlitterSetColor(context_, SbBlitterColorFromRGBA(0, 0, 0, 0));
  SbBlitterSetBlending(context_, false);
  SbBlitterFillRect(context_,
                    SbBlitterMakeRect(0, 0, coord_mapping.output_bounds.width(),
                                      coord_mapping.output_bounds.height()));
}

common::SurfaceCache::CachedSurface* SurfaceCacheDelegate::EndRecording() {
  TRACE_EVENT0("cobalt::renderer", "SurfaceCacheDelegate::EndRecording()");
  DCHECK(recording_data_);

  // Restore our original render state from before we started recording.
  render_state_ =
      set_render_state_function_.Run(recording_data_->original_render_state);
  SbBlitterSetRenderTarget(context_, render_state_->render_target);
  render_state_->bounds_stack.UpdateContext();

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
  math::Size size(
      static_cast<int>(
          local_size.width() * render_state_->transform.scale().x() + 0.5f),
      static_cast<int>(
          local_size.height() * render_state_->transform.scale().y() + 0.5f));
  return size;
}

math::Size SurfaceCacheDelegate::MaximumSurfaceSize() {
  // The Blitter API does not have methods for determining the maximum surface
  // size.
  return math::Size(2048, 2048);
}

}  // namespace blitter
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // #if SB_HAS(BLITTER)
