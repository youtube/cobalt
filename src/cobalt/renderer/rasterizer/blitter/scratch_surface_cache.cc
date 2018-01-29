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

#include "cobalt/renderer/rasterizer/blitter/scratch_surface_cache.h"

#include "cobalt/base/polymorphic_downcast.h"
#include "starboard/blitter.h"

#if SB_HAS(BLITTER)

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace blitter {

namespace {

class BlitterSurface : public common::ScratchSurfaceCache::Surface {
 public:
  BlitterSurface(SbBlitterSurface surface, const math::Size& size)
      : surface_(surface), size_(size) {}
  ~BlitterSurface() override { SbBlitterDestroySurface(surface_); }

  math::Size GetSize() const override { return size_; }

  SbBlitterSurface blitter_surface() { return surface_; }

 private:
  SbBlitterSurface surface_;
  math::Size size_;
};
}  // namespace

ScratchSurfaceCache::ScratchSurfaceCache(SbBlitterDevice device,
                                         SbBlitterContext context,
                                         int cache_capacity_in_bytes)
    : delegate_(device, context), cache_(&delegate_, cache_capacity_in_bytes) {}

ScratchSurfaceCache::Delegate::Delegate(SbBlitterDevice device,
                                        SbBlitterContext context)
    : device_(device), context_(context) {}

common::ScratchSurfaceCache::Surface*
ScratchSurfaceCache::Delegate::CreateSurface(const math::Size& size) {
  SbBlitterSurface blitter_surface = SbBlitterCreateRenderTargetSurface(
      device_, size.width(), size.height(), kSbBlitterSurfaceFormatRGBA8);
  if (SbBlitterIsSurfaceValid(blitter_surface)) {
    return new BlitterSurface(blitter_surface, size);
  } else {
    return NULL;
  }
}

void ScratchSurfaceCache::Delegate::DestroySurface(
    common::ScratchSurfaceCache::Surface* surface) {
  delete surface;
}

void ScratchSurfaceCache::Delegate::PrepareForUse(
    common::ScratchSurfaceCache::Surface* surface, const math::Size& area) {
  SbBlitterSurface blitter_surface =
      base::polymorphic_downcast<BlitterSurface*>(surface)->blitter_surface();

  SbBlitterSetRenderTarget(
      context_, SbBlitterGetRenderTargetFromSurface(blitter_surface));
  SbBlitterSetBlending(context_, false);
  SbBlitterSetColor(context_, SbBlitterColorFromRGBA(0, 0, 0, 0));
  SbBlitterFillRect(context_,
                    SbBlitterMakeRect(0, 0, area.width(), area.height()));
}

SbBlitterSurface CachedScratchSurface::GetSurface() {
  if (common_scratch_surface_.GetSurface()) {
    return base::polymorphic_downcast<BlitterSurface*>(
               common_scratch_surface_.GetSurface())
        ->blitter_surface();
  } else {
    return kSbBlitterInvalidSurface;
  }
}

}  // namespace blitter
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_HAS(BLITTER)
