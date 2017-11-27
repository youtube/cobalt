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

#ifndef COBALT_RENDERER_RASTERIZER_BLITTER_SCRATCH_SURFACE_CACHE_H_
#define COBALT_RENDERER_RASTERIZER_BLITTER_SCRATCH_SURFACE_CACHE_H_

#include "cobalt/math/size.h"
#include "cobalt/renderer/rasterizer/common/scratch_surface_cache.h"
#include "starboard/blitter.h"

#if SB_HAS(BLITTER)

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace blitter {

// We define a blitter::ScratchSurfaceCache class here that decorates
// common::ScratchSurfaceCache by customizing it to be Blitter API specific.
// A Blitter API renderer can use this class to request temporary scratch
// SbBlitterSurfaces that can be automatically re-used over the course of a
// frame.  CachedScratchSurface objects must be created in order to actually
// interact with ScratchSurfaceCache.
class ScratchSurfaceCache {
 public:
  // |device| specifies the |device| from which surfaces should be allocated
  // from and |context| specifies which context to use when draw commands need
  // to be issued (e.g. to clear a surface so it is ready for use).
  ScratchSurfaceCache(SbBlitterDevice device, SbBlitterContext context,
                      int cache_capacity_in_bytes);

 private:
  class Delegate : public common::ScratchSurfaceCache::Delegate {
   public:
    Delegate(SbBlitterDevice device, SbBlitterContext context);

    common::ScratchSurfaceCache::Surface* CreateSurface(
        const math::Size& size) override;
    void DestroySurface(common::ScratchSurfaceCache::Surface* surface) override;
    void PrepareForUse(common::ScratchSurfaceCache::Surface* surface,
                       const math::Size& area) override;

   private:
    SbBlitterDevice device_;
    SbBlitterContext context_;
  };

  Delegate delegate_;
  common::ScratchSurfaceCache cache_;

  friend class CachedScratchSurface;
};

// This class acts as the interface to ScratchSurfaceCache.  It does so by
// decorating common::CachedScratchSurface and adding methods to obtain
// a reference to the Blitter API-specific SbBlitterSurface object.
class CachedScratchSurface {
 public:
  CachedScratchSurface(ScratchSurfaceCache* cache, const math::Size& size)
      : common_scratch_surface_(&cache->cache_, size) {}

  // Returns a pointer to the Blitter surface.
  SbBlitterSurface GetSurface();

 private:
  common::CachedScratchSurface common_scratch_surface_;
};

}  // namespace blitter
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_HAS(BLITTER)

#endif  // COBALT_RENDERER_RASTERIZER_BLITTER_SCRATCH_SURFACE_CACHE_H_
