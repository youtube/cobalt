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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_SCRATCH_SURFACE_CACHE_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_SCRATCH_SURFACE_CACHE_H_

#include <vector>

#include "base/callback.h"
#include "base/logging.h"
#include "cobalt/math/size.h"
#include "cobalt/renderer/rasterizer/common/scratch_surface_cache.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

class ScratchSurfaceCache {
 public:
  typedef base::Callback<SkSurface*(const math::Size&)> CreateSkSurfaceFunction;

  ScratchSurfaceCache(CreateSkSurfaceFunction create_sk_surface_function,
                      int cache_capacity_in_bytes);

 private:
  class Delegate : public common::ScratchSurfaceCache::Delegate {
   public:
    explicit Delegate(CreateSkSurfaceFunction create_sk_surface_function);

    common::ScratchSurfaceCache::Surface* CreateSurface(
        const math::Size& size) override;
    void DestroySurface(common::ScratchSurfaceCache::Surface* surface) override;
    void PrepareForUse(common::ScratchSurfaceCache::Surface* surface,
                       const math::Size& area) override;

   private:
    CreateSkSurfaceFunction create_sk_surface_function_;
  };

  Delegate delegate_;
  common::ScratchSurfaceCache cache_;

  friend class CachedScratchSurface;
};

class CachedScratchSurface {
 public:
  CachedScratchSurface(ScratchSurfaceCache* cache, const math::Size& size)
      : common_scratch_surface_(&cache->cache_, size) {}

  // Returns a pointer to the Skia SkSurface.
  SkSurface* GetSurface();

 private:
  common::CachedScratchSurface common_scratch_surface_;
};

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_SCRATCH_SURFACE_CACHE_H_
