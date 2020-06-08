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

#include "cobalt/renderer/rasterizer/blitter/linear_gradient_cache.h"

#include <algorithm>
#include <cmath>
#include <iterator>

#include "cobalt/render_tree/brush.h"
#include "starboard/blitter.h"

#if SB_API_VERSION < 12 && SB_HAS(BLITTER)

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace blitter {

using cobalt::render_tree::LinearGradientBrush;

bool LinearGradientCache::Put(const LinearGradientBrush& brush,
                              SbBlitterSurface surface) {
  if (!SbBlitterIsSurfaceValid(surface)) {
    return false;
  }
  return cache_.put(brush.data(), surface);
}

SbBlitterSurface LinearGradientCache::Get(const LinearGradientBrush& brush) {
  CacheMap::iterator cache_result = cache_.find(brush.data());
  if (cache_result == cache_.end()) {
    return kSbBlitterInvalidSurface;
  }
  SbBlitterSurface surface = cache_result->value;
  DCHECK(SbBlitterIsSurfaceValid(surface));
  return surface;
}

}  // namespace blitter
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_API_VERSION < 12 && SB_HAS(BLITTER)
