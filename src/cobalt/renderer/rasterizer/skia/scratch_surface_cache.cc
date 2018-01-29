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

#include "cobalt/renderer/rasterizer/skia/scratch_surface_cache.h"

#include "cobalt/base/polymorphic_downcast.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkRegion.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

namespace {

class SkiaSurface : public common::ScratchSurfaceCache::Surface {
 public:
  SkiaSurface(sk_sp<SkSurface> surface, const math::Size& size)
      : surface_(surface), size_(size) {}

  math::Size GetSize() const override { return size_; }

  SkSurface* sk_surface() { return surface_.get(); }

 private:
  sk_sp<SkSurface> surface_;
  math::Size size_;
};
}  // namespace

ScratchSurfaceCache::ScratchSurfaceCache(
    CreateSkSurfaceFunction create_sk_surface_function,
    int cache_capacity_in_bytes)
    : delegate_(create_sk_surface_function),
      cache_(&delegate_, cache_capacity_in_bytes) {}

ScratchSurfaceCache::Delegate::Delegate(
    CreateSkSurfaceFunction create_sk_surface_function)
    : create_sk_surface_function_(create_sk_surface_function) {}

common::ScratchSurfaceCache::Surface*
ScratchSurfaceCache::Delegate::CreateSurface(const math::Size& size) {
  sk_sp<SkSurface> sk_surface = create_sk_surface_function_.Run(size);
  if (!sk_surface) {
    return nullptr;
  }
  return new SkiaSurface(sk_surface, size);
}

void ScratchSurfaceCache::Delegate::DestroySurface(
    common::ScratchSurfaceCache::Surface* surface) {
  delete surface;
}

void ScratchSurfaceCache::Delegate::PrepareForUse(
    common::ScratchSurfaceCache::Surface* surface, const math::Size& area) {
  SkSurface* sk_surface =
      base::polymorphic_downcast<SkiaSurface*>(surface)->sk_surface();

  // Reset the sk_surface's canvas settings such as transform matrix and clip.
  SkCanvas* canvas = sk_surface->getCanvas();
  canvas->restoreToCount(1);
  // Setup a save marker on the reset canvas so that we can restore this reset
  // state when re-using the sk_surface.
  canvas->save();

  // Indicate that we do not care about the existing state of the canvas, which
  // can allow for optimization on tile-based renderers.
  canvas->discard();

  // Setup a clip rect on the sk_surface to the requested area.  This can save
  // us from drawing to pixels outside of the requested area, since the actual
  // sk_surface returned may be larger than the requested area.
  canvas->clipRect(SkRect::MakeWH(area.width(), area.height()),
                   SkClipOp::kIntersect);

  // Clear the draw area to RGBA(0, 0, 0, 0), as expected for a fresh scratch
  // sk_surface, before returning.
  canvas->clear(SkColorSetARGB(0, 0, 0, 0));
}

SkSurface* CachedScratchSurface::GetSurface() {
  if (common_scratch_surface_.GetSurface()) {
    return base::polymorphic_downcast<SkiaSurface*>(
               common_scratch_surface_.GetSurface())
        ->sk_surface();
  } else {
    return NULL;
  }
}

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
