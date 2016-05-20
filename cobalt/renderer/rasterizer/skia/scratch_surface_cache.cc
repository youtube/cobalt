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

#include "cobalt/renderer/rasterizer/skia/scratch_surface_cache.h"

#include <limits>

#include "base/debug/trace_event.h"

#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/gpu/GrTexture.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

namespace {

// We choose this cache size empirically, however exceeding the cache is allowed
// and won't have any detrimental effects, besides the fact that the cache
// won't be fully utilized.  It may eventually be nice to allow this to be
// set as a paramter.
const size_t kMaxCacheSizeInBytes = 7 * 1024 * 1024;

// Approximate the memory usage of a given surface size.
size_t ApproximateSurfaceMemory(const math::Size& size) {
  // Here we assume that we use 4 bytes per pixel.
  return size.width() * size.height() * 4;
}

}  // namespace

ScratchSurfaceCache::ScratchSurfaceCache(GrContext* gr_context,
                                         const SkSurfaceProps& surface_props)
    : gr_context_(gr_context),
      surface_props_(surface_props),
      surface_memory_(0) {}

ScratchSurfaceCache::~ScratchSurfaceCache() {
  DCHECK(surface_stack_.empty());
  for (std::vector<SkSurface*>::iterator iter = unused_surfaces_.begin();
       iter != unused_surfaces_.end(); ++iter) {
    (*iter)->unref();
  }
}

SkSurface* ScratchSurfaceCache::AcquireScratchSurface(const math::Size& size) {
  TRACE_EVENT2("cobalt::renderer",
               "ScratchSurfaceCache::AcquireScratchSurface()", "width",
               size.width(), "height", size.height());

  // First check if we can find a suitable surface in our cache that is at
  // least the size requested.
  SkSurface* surface = FindBestCachedSurface(size);

  // If we didn't have any suitable surfaces in our cache, create a new one.
  if (!surface) {
    // Increase our total memory used on surfaces, and then initiate a purge
    // to reduce memory to below our cache limit, if necessary.
    surface_memory_ += ApproximateSurfaceMemory(size);
    Purge();

    // Create the surface.
    surface = CreateScratchSurface(size);

    if (!surface) {
      // We were unable to allocate a scratch surface, either because we are
      // low on memory or because the requested surface has large dimensions.
      // Return null.
      surface_memory_ -= ApproximateSurfaceMemory(size);
      return NULL;
    }
  }

  DCHECK(surface);

  // Track that we have handed out this surface.
  surface_stack_.push_back(surface);

  // Reset the surface's canvas settings such as transform matrix and clip.
  SkCanvas* canvas = surface->getCanvas();
  canvas->restoreToCount(1);
  // Setup a save marker on the reset canvas so that we can restore this reset
  // state when re-using the surface.
  canvas->save();

  // Setup a clip rect on the surface to the requested size.  This can save
  // us from drawing to pixels outside of the requested size, since the actual
  // surface returned may be larger than the requested size.
  canvas->clipRect(SkRect::MakeWH(size.width(), size.height()),
                   SkRegion::kReplace_Op);

  // Clear the draw area to RGBA(0, 0, 0, 0), as expected for a fresh scratch
  // surface, before returning.
  canvas->drawARGB(0, 0, 0, 0, SkXfermode::kClear_Mode);

  return surface;
}

void ScratchSurfaceCache::ReleaseScratchSurface(SkSurface* surface) {
  TRACE_EVENT2("cobalt::renderer",
               "ScratchSurfaceCache::ReleaseScratchSurface()", "width",
               surface->width(), "height", surface->height());

  DCHECK_EQ(surface_stack_.back(), surface);
  surface_stack_.pop_back();

  // Add this surface to the end (where most recently used surfaces go) of the
  // unused surfaces list, so that it can be returned by later calls to acquire
  // a surface.
  unused_surfaces_.push_back(surface);
}

namespace {

float GetMatchScoreForSurfaceAndSize(SkSurface* surface,
                                     const math::Size& size) {
  // We use the negated sum of the squared differences between the requested
  // width/height and the surface width/height as the score.
  // This promotes returning the smallest surface possible that has a similar
  // ratio.
  int width_diff = surface->width() - size.width();
  int height_diff = surface->height() - size.height();

  return -width_diff * width_diff - height_diff * height_diff;
}

}  // namespace

SkSurface* ScratchSurfaceCache::FindBestCachedSurface(const math::Size& size) {
  // Iterate through all cached surfaces to find the one that is the best match
  // for the given size.  The function GetMatchScoreForSurfaceAndSize() is
  // responsible for assigning a score to a SkSurface/math::Size pair.
  float max_surface_score = -std::numeric_limits<float>::infinity();
  std::vector<SkSurface*>::iterator max_iter = unused_surfaces_.end();
  for (std::vector<SkSurface*>::iterator iter = unused_surfaces_.begin();
       iter != unused_surfaces_.end(); ++iter) {
    SkSurface* current_surface = *iter;
    if (current_surface->width() >= size.width() &&
        current_surface->height() >= size.height()) {
      float surface_score =
          GetMatchScoreForSurfaceAndSize(current_surface, size);

      if (surface_score > max_surface_score) {
        max_surface_score = surface_score;
        max_iter = iter;
      }
    }
  }

  // If any of the cached unused surfaces had at least the specified
  // width/height, return the one that had the highest score.
  if (max_iter != unused_surfaces_.end()) {
    SkSurface* surface = *max_iter;
    // Remove this surface from the list of unused surfaces.
    unused_surfaces_.erase(max_iter);
    // Return the cached surface.
    return surface;
  } else {
    // Otherwise return NULL to indicate that we do not have any suitable cached
    // surfaces.
    return NULL;
  }
}

void ScratchSurfaceCache::Purge() {
  // Delete surfaces from the front (least recently used) of |unused_surfaces_|
  // until we have deleted all surfaces or lowered our memory usage to under
  // kMaxCacheSizeInBytes.
  while (!unused_surfaces_.empty() && surface_memory_ > kMaxCacheSizeInBytes) {
    SkSurface* to_free = unused_surfaces_.front();
    surface_memory_ -= ApproximateSurfaceMemory(
        math::Size(to_free->width(), to_free->height()));
    to_free->unref();
    unused_surfaces_.erase(unused_surfaces_.begin());
  }
}

SkSurface* ScratchSurfaceCache::CreateScratchSurface(const math::Size& size) {
  TRACE_EVENT2("cobalt::renderer",
               "ScratchSurfaceCache::CreateScratchSurface()", "width",
               size.width(), "height", size.height());

  // Create a texture of the specified size.  Then convert it to a render
  // target and then convert that to a SkSurface which we return.
  GrTextureDesc target_surface_desc;
  target_surface_desc.fFlags =
      kRenderTarget_GrTextureFlagBit | kNoStencil_GrTextureFlagBit;
  target_surface_desc.fOrigin = kBottomLeft_GrSurfaceOrigin;
  target_surface_desc.fWidth = size.width();
  target_surface_desc.fHeight = size.height();
  target_surface_desc.fConfig = kRGBA_8888_GrPixelConfig;
  target_surface_desc.fSampleCnt = 0;

  SkAutoTUnref<GrTexture> skia_texture(
      gr_context_->createUncachedTexture(target_surface_desc, NULL, 0));
  if (!skia_texture) {
    // If we failed at creating a texture, try again using a different texture
    // format.
    target_surface_desc.fConfig = kBGRA_8888_GrPixelConfig;
    skia_texture.reset(
        gr_context_->createUncachedTexture(target_surface_desc, NULL, 0));
  }
  if (!skia_texture) {
    return NULL;
  }

  GrRenderTarget* skia_render_target = skia_texture->asRenderTarget();
  DCHECK(skia_render_target);

  return SkSurface::NewRenderTargetDirect(skia_render_target, &surface_props_);
}

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
