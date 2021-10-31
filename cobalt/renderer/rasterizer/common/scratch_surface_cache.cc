// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/renderer/rasterizer/common/scratch_surface_cache.h"

#include <limits>

#include "base/trace_event/trace_event.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace common {

namespace {

// Approximate the memory usage of a given surface size.
size_t ApproximateSurfaceMemory(const math::Size& size) {
  // Here we assume that we use 4 bytes per pixel.
  return size.width() * size.height() * 4;
}

}  // namespace

ScratchSurfaceCache::ScratchSurfaceCache(Delegate* delegate,
                                         int cache_capacity_in_bytes)
    : delegate_(delegate),
      cache_capacity_in_bytes_(cache_capacity_in_bytes),
      surface_memory_(0) {}

ScratchSurfaceCache::~ScratchSurfaceCache() {
  DCHECK(surface_stack_.empty());
  for (std::vector<Surface*>::iterator iter = unused_surfaces_.begin();
       iter != unused_surfaces_.end(); ++iter) {
    delegate_->DestroySurface(*iter);
  }
}

ScratchSurfaceCache::Surface* ScratchSurfaceCache::AcquireScratchSurface(
    const math::Size& size) {
  TRACE_EVENT2("cobalt::renderer",
               "ScratchSurfaceCache::AcquireScratchSurface()", "width",
               size.width(), "height", size.height());
  if (cache_capacity_in_bytes_ == 0) {
    // If scratch surface caching is disabled, just create and immediately
    // return a surface.
    Surface* surface = delegate_->CreateSurface(size);
    if (surface) {
      delegate_->PrepareForUse(surface, size);
      return surface;
    } else {
      return NULL;
    }
  }

  // First check if we can find a suitable surface in our cache that is at
  // least the size requested.
  Surface* surface = FindBestCachedSurface(size);

  // If we didn't have any suitable surfaces in our cache, create a new one.
  if (!surface) {
    // Increase our total memory used on surfaces, and then initiate a purge
    // to reduce memory to below our cache limit, if necessary.
    surface_memory_ += ApproximateSurfaceMemory(size);
    Purge();

    // Create the surface.
    surface = delegate_->CreateSurface(size);

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

  delegate_->PrepareForUse(surface, size);

  return surface;
}

void ScratchSurfaceCache::ReleaseScratchSurface(Surface* surface) {
  TRACE_EVENT2("cobalt::renderer",
               "ScratchSurfaceCache::ReleaseScratchSurface()", "width",
               surface->GetSize().width(), "height",
               surface->GetSize().height());
  if (cache_capacity_in_bytes_ == 0) {
    // If scratch surface caching is disabled, immediately destroy the surface
    // and return.
    delegate_->DestroySurface(surface);
    return;
  }

  DCHECK_EQ(surface_stack_.back(), surface);
  surface_stack_.pop_back();

  // Add this surface to the end (where most recently used surfaces go) of the
  // unused surfaces list, so that it can be returned by later calls to acquire
  // a surface.
  unused_surfaces_.push_back(surface);
}

namespace {

float GetMatchScoreForSurfaceAndSize(ScratchSurfaceCache::Surface* surface,
                                     const math::Size& size) {
  math::Size surface_size = surface->GetSize();

  // We use the negated sum of the squared differences between the requested
  // width/height and the surface width/height as the score.
  // This promotes returning the smallest surface possible that has a similar
  // ratio.
  int width_diff = surface_size.width() - size.width();
  int height_diff = surface_size.height() - size.height();

  return -width_diff * width_diff - height_diff * height_diff;
}

}  // namespace

ScratchSurfaceCache::Surface* ScratchSurfaceCache::FindBestCachedSurface(
    const math::Size& size) {
  // Iterate through all cached surfaces to find the one that is the best match
  // for the given size.  The function GetMatchScoreForSurfaceAndSize() is
  // responsible for assigning a score to a Surface/math::Size pair.
  float max_surface_score = -std::numeric_limits<float>::infinity();
  std::vector<Surface*>::iterator max_iter = unused_surfaces_.end();
  for (std::vector<Surface*>::iterator iter = unused_surfaces_.begin();
       iter != unused_surfaces_.end(); ++iter) {
    Surface* current_surface = *iter;
    math::Size surface_size = current_surface->GetSize();
    if (surface_size.width() >= size.width() &&
        surface_size.height() >= size.height()) {
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
    Surface* surface = *max_iter;
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
  // |cache_capacity_in_bytes_|.
  while (!unused_surfaces_.empty() &&
         surface_memory_ > cache_capacity_in_bytes_) {
    Surface* to_free = unused_surfaces_.front();
    math::Size surface_size = to_free->GetSize();
    surface_memory_ -= ApproximateSurfaceMemory(
        math::Size(surface_size.width(), surface_size.height()));
    delegate_->DestroySurface(to_free);
    unused_surfaces_.erase(unused_surfaces_.begin());
  }
}

}  // namespace common
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
