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

#include "cobalt/renderer/rasterizer/blitter/cached_software_rasterizer.h"

#include <utility>

#include "cobalt/math/vector2d_f.h"
#include "cobalt/renderer/rasterizer/blitter/skia_blitter_conversions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageInfo.h"

#if SB_HAS(BLITTER)

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace blitter {

size_t CachedSoftwareRasterizer::Surface::GetEstimatedMemoryUsage() const {
  // We assume 4-bytes-per-pixel.
  return coord_mapping.output_bounds.size().GetArea() * 4;
}

CachedSoftwareRasterizer::CachedSoftwareRasterizer(
    SbBlitterDevice device, SbBlitterContext context, int cache_capacity,
    bool purge_skia_font_caches_on_destruction)
    : cache_capacity_(cache_capacity),
      device_(device),
      context_(context),
      software_rasterizer_(0, purge_skia_font_caches_on_destruction),
      cache_memory_usage_(
          "Memory.CachedSoftwareRasterizer.CacheUsage", 0,
          "Total memory occupied by cached software-rasterized surfaces."),
      cache_frame_usage_(
          "Memory.CachedSoftwareRasterizer.FrameCacheUsage", 0,
          "Total memory occupied by cache software-rasterizer surfaces that "
          "were referenced this frame.") {}

CachedSoftwareRasterizer::~CachedSoftwareRasterizer() {
  // Clean up any leftover surfaces.
  for (CacheMap::iterator iter = surface_map_.begin();
       iter != surface_map_.end(); ++iter) {
    DCHECK(iter->second.cached);
    SbBlitterDestroySurface(iter->second.surface);
  }
  surface_map_.clear();
}

void CachedSoftwareRasterizer::OnStartNewFrame() {
  for (CacheMap::iterator iter = surface_map_.begin();
       iter != surface_map_.end();) {
    CacheMap::iterator current = iter;
    ++iter;

    // If the surface was referenced mark it as being unreferenced for the next
    // frame.
    if (current->second.referenced) {
#if defined(ENABLE_DEBUG_CONSOLE)
      current->second.created = false;
#endif  // defined(ENABLE_DEBUG_CONSOLE)
      current->second.referenced = false;
    }
  }

  // Reset our current frame cache usage to 0 since this is the start of a new
  // frame.
  cache_frame_usage_ = 0;
}

void CachedSoftwareRasterizer::PurgeUntilSpaceAvailable(int space_needed) {
  while (space_needed + cache_memory_usage_.value() > cache_capacity_) {
    CacheMap::iterator next_item = surface_map_.begin();

    // We shouldn't call this function if it means we would have to purge
    // elements that were referenced this frame.  This is to avoid thrashing
    // the cache, we would prefer to cache as much as we can in a frame, and
    // then just not cache whatever we can't without cycling what's in the cache
    // already.
    DCHECK(!next_item->second.referenced);
    cache_memory_usage_ -= next_item->second.GetEstimatedMemoryUsage();
    SbBlitterDestroySurface(next_item->second.surface);
    surface_map_.erase(next_item);
  }
}

CachedSoftwareRasterizer::Surface CachedSoftwareRasterizer::GetSurface(
    render_tree::Node* node, const Transform& transform) {
  CacheMap::iterator found = surface_map_.find(node);
  if (found != surface_map_.end()) {
#if SB_HAS(BILINEAR_FILTERING_SUPPORT)
    if (found->second.scale.x() >= transform.scale().x() &&
        found->second.scale.y() >= transform.scale().y()) {
#else  // SB_HAS(BILINEAR_FILTERING_SUPPORT)
    if (found->second.scale.x() == transform.scale().x() &&
        found->second.scale.y() == transform.scale().y()) {
#endif
      std::pair<render_tree::Node*, Surface> to_insert =
          std::make_pair(node, found->second);

      // Move this surface's position in the queue to the front, since it was
      // referenced.
      to_insert.second.referenced = true;
      surface_map_.erase(found);
      surface_map_.insert(to_insert);

      cache_frame_usage_ += to_insert.second.GetEstimatedMemoryUsage();

      return to_insert.second;
    }
  }

  Surface software_surface;
  software_surface.referenced = true;
#if defined(ENABLE_DEBUG_CONSOLE)
  software_surface.created = true;
#endif  // defined(ENABLE_DEBUG_CONSOLE)
  software_surface.node = node;
  software_surface.surface = kSbBlitterInvalidSurface;
  software_surface.cached = false;

  // We ensure that scale is at least 1. Since it is common to have animating
  // scale between 0 and 1 (e.g. an item animating from 0 to 1 to "grow" into
  // the scene), this ensures that rendered items will not look pixellated as
  // they grow (e.g. if they were cached at scale 0.2, they would be stretched
  // x5 if the scale were to animate to 1.0).
  if (transform.scale().x() == 0.0f || transform.scale().y() == 0.0f) {
    // There won't be anything to render if the transform squishes one dimension
    // to 0.
    return software_surface;
  }
  DCHECK_LT(0, transform.scale().x());
  DCHECK_LT(0, transform.scale().y());
  Transform scaled_transform(transform);
// If bilinear filtering is not supported, do not rely on the rasterizer to
// scale down for us, it results in too many artifacts.
#if SB_HAS(BILINEAR_FILTERING_SUPPORT)
  math::Vector2dF apply_scale(1.0f, 1.0f);
  if (transform.scale().x() < 1.0f) {
    apply_scale.set_x(1.0f / transform.scale().x());
  }
  if (transform.scale().y() < 1.0f) {
    apply_scale.set_y(1.0f / transform.scale().y());
  }
  scaled_transform.ApplyScale(apply_scale);
#endif  // SB_HAS(BILINEAR_FILTERING_SUPPORT)

  software_surface.scale = scaled_transform.scale();

  common::OffscreenRenderCoordinateMapping coord_mapping =
      common::GetOffscreenRenderCoordinateMapping(node->GetBounds(),
                                                  scaled_transform.ToMatrix(),
                                                  base::optional<math::Rect>());

  software_surface.coord_mapping = coord_mapping;

  if (coord_mapping.output_bounds.IsEmpty()) {
    // There's nothing to render if the bounds are 0.
    return software_surface;
  }

  DCHECK_GE(0.001f, std::abs(1.0f -
                             scaled_transform.scale().x() *
                                 coord_mapping.output_post_scale.x()));
  DCHECK_GE(0.001f, std::abs(1.0f -
                             scaled_transform.scale().y() *
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
  if (!SbBlitterIsPixelDataValid(pixel_data)) {
    // We failed to allocate the pixel data, just return with a null surface
    // in this case.
    LOG(ERROR) << "Error allocating pixel data for an offscreen software "
                  "rasterization.";
    return software_surface;
  }

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
  // |transform|.
  canvas.scale(sub_render_transform.scale().x(),
               sub_render_transform.scale().y());

  // Use the Skia software rasterizer to render our subtree.
  software_rasterizer_.Submit(node, &canvas);

  // Create a surface out of the now populated pixel data.
  software_surface.surface =
      SbBlitterCreateSurfaceFromPixelData(device_, pixel_data);

  if (software_surface.GetEstimatedMemoryUsage() + cache_frame_usage_.value() <=
      cache_capacity_) {
    software_surface.cached = true;
    cache_frame_usage_ += software_surface.GetEstimatedMemoryUsage();

    if (found != surface_map_.end()) {
      // This surface may have already been in the cache if it was in there
      // with a different scale.  In that case, replace the old one.
      cache_memory_usage_ -= found->second.GetEstimatedMemoryUsage();
      surface_map_.erase(found);
    }

    PurgeUntilSpaceAvailable(software_surface.GetEstimatedMemoryUsage());

    std::pair<CacheMap::iterator, bool> inserted =
        surface_map_.insert(std::make_pair(node, software_surface));
    cache_memory_usage_ += software_surface.GetEstimatedMemoryUsage();
  }

  return software_surface;
}

}  // namespace blitter
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // #if SB_HAS(BLITTER)
