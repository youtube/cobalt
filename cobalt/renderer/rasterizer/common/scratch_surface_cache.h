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

#ifndef COBALT_RENDERER_RASTERIZER_COMMON_SCRATCH_SURFACE_CACHE_H_
#define COBALT_RENDERER_RASTERIZER_COMMON_SCRATCH_SURFACE_CACHE_H_

#include <vector>

#include "base/callback.h"
#include "base/logging.h"
#include "cobalt/math/size.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace common {

// The ScratchSurfaceCache manages a set of platform-specific surface objects
// that can be used for offscreen rendering (e.g. by a RenderTreeNodeVisitor).
// In order to acquire surfaces from ScratchSurfaceCache, one should construct
// a CachedScratchSurface object, from which they can get a pointer to the
// Surface object.
// It is unique because it caters to RenderTreeNodeVisitor's scratch
// surface allocation pattern.  In particular, we don't mind handing out very
// large scratch surfaces when very small surfaces are requested, because
// typically only one surface is ever needed at a time.  When multiple surfaces
// are necessary, the subsequent ones tend to be smaller than the previous
// (because rendering is now happening within the previous ones), and so handing
// out textures in large to small order makes sense.
class ScratchSurfaceCache {
 public:
  class Surface {
   public:
    virtual ~Surface() {}
    virtual math::Size GetSize() const = 0;
  };
  class Delegate {
   public:
    virtual Surface* CreateSurface(const math::Size& size) = 0;
    virtual void DestroySurface(Surface* surface) = 0;
    virtual void PrepareForUse(Surface* surface, const math::Size& area) = 0;
  };

  ScratchSurfaceCache(Delegate* delegate, int cache_capacity_in_bytes);
  ~ScratchSurfaceCache();

 private:
  // Acquire (either via cache lookup or by creation if that fails) a scratch
  // surface.
  Surface* AcquireScratchSurface(const math::Size& size);
  void ReleaseScratchSurface(Surface* surface);

  // Searches through |unused_surfaces_| for one the one that best matches the
  // requested size, among all those cached surfaces that have at least the
  // requested size.
  Surface* FindBestCachedSurface(const math::Size& size);

  // Removes elements from the cache until we are below the cache limit.
  void Purge();

  // Allocate a new Surface and return it.
  Surface* CreateScratchSurface(const math::Size& size);

  // Implementation-specific details about how to create/destroy surfaces.
  Delegate* delegate_;

  // The maximum number of surface bytes that can be stored in the cache.
  int cache_capacity_in_bytes_;

  // We keep track of all surfaces we've handed out using |surface_stack_|.
  // This is mostly for debug checks to verify that surfaces returned to us
  // actually did come from us, and that they are returned in the expected
  // order.
  std::vector<Surface*> surface_stack_;

  // |unused_surfaces_| is the heart of the cache, as it stores the surfaces
  // that are unused but allocated and available to be handed out upon request.
  // Most recently used surfaces are found at the end of this vector.
  std::vector<Surface*> unused_surfaces_;

  // Tracks how much total memory we've allocated towards surfaces.
  size_t surface_memory_;

  friend class CachedScratchSurface;
};

// The primary interface to ScratchSurfaceCache is through CachedScratchSurface
// objects.  These effectively scope the acquisition of a cached surface, RAII
// style.
class CachedScratchSurface {
 public:
  CachedScratchSurface(ScratchSurfaceCache* cache, const math::Size& size)
      : cache_(cache) {
    surface_ = cache_->AcquireScratchSurface(size);
  }

  ~CachedScratchSurface() {
    if (surface_) {
      cache_->ReleaseScratchSurface(surface_);
    }
  }

  // Returns a pointer to the platform-specific surface.  The platform can then
  // downcast to the expected surface type.
  ScratchSurfaceCache::Surface* GetSurface() { return surface_; }

 private:
  ScratchSurfaceCache* cache_;
  ScratchSurfaceCache::Surface* surface_;
};

}  // namespace common
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_COMMON_SCRATCH_SURFACE_CACHE_H_
