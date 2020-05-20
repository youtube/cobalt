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

#ifndef COBALT_RENDERER_RASTERIZER_BLITTER_LINEAR_GRADIENT_CACHE_H_
#define COBALT_RENDERER_RASTERIZER_BLITTER_LINEAR_GRADIENT_CACHE_H_

#include <algorithm>
#include <vector>

#include "cobalt/base/fixed_size_lru_cache.h"
#include "cobalt/render_tree/brush.h"
#include "starboard/blitter.h"

#if SB_API_VERSION < 12 && SB_HAS(BLITTER)

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace blitter {

// This class uses a circular buffer to cache surfaces created by linear
// gradients.
// This class has two public methods: Put, and Get.

class LinearGradientCache {
  // This functor is used to determine if two brushes are close to each other
  // enough to share the same surface.
  struct IsGradientBrushSameSizeAndDirection {
    bool operator()(const render_tree::LinearGradientBrush::Data& brush1,
                    const render_tree::LinearGradientBrush::Data& brush2) {
      math::PointF delta_brush1(brush1.dest_.x() - brush1.source_.x(),
                                brush1.dest_.y() - brush1.source_.y());
      math::PointF delta_brush2(brush2.dest_.x() - brush2.source_.x(),
                                brush2.dest_.y() - brush2.source_.y());
      if (delta_brush1 != delta_brush2) return false;
      if (brush1.color_stops_.size() != brush2.color_stops_.size())
        return false;
      return IsNear(brush1.color_stops_, brush2.color_stops_, 1E-9);
    }
  };

  class SurfaceDeleter {
   public:
    void operator()(SbBlitterSurface surface) {
      DCHECK(SbBlitterIsSurfaceValid(surface));
      if (SbBlitterIsSurfaceValid(surface)) {
        SbBlitterDestroySurface(surface);
      }
    }
  };

 public:
  static const int kDefaultCacheSizeItems = 8;

  typedef base::FixedSizeLRUCache<
      cobalt::render_tree::LinearGradientBrush::Data, SbBlitterSurface,
      kDefaultCacheSizeItems, SurfaceDeleter,
      IsGradientBrushSameSizeAndDirection>
      CacheMap;

  // Returns true if item was inserted into the cache.
  bool Put(const render_tree::LinearGradientBrush& brush,
           SbBlitterSurface surface);

  // Returns kSbBlitterInvalidSurface if no match for a brush is found.
  SbBlitterSurface Get(const cobalt::render_tree::LinearGradientBrush& brush);

 private:
  CacheMap cache_;
};

}  // namespace blitter
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_API_VERSION < 12 && SB_HAS(BLITTER)

#endif  // COBALT_RENDERER_RASTERIZER_BLITTER_LINEAR_GRADIENT_CACHE_H_
