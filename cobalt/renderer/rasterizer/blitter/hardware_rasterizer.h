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

#ifndef COBALT_RENDERER_RASTERIZER_BLITTER_HARDWARE_RASTERIZER_H_
#define COBALT_RENDERER_RASTERIZER_BLITTER_HARDWARE_RASTERIZER_H_

#include "base/memory/scoped_ptr.h"
#include "cobalt/render_tree/node.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/renderer/backend/graphics_context.h"
#include "cobalt/renderer/backend/render_target.h"
#include "cobalt/renderer/rasterizer/blitter/scratch_surface_cache.h"
#include "cobalt/renderer/rasterizer/rasterizer.h"

#if SB_HAS(BLITTER)

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace blitter {

// The Blitter API hardware rasterizer will render all render tree node elements
// via the Blitter API, if possible.  If it is not possible, it will fallback
// on using software Skia to render portions of the tree that the Blitter API
// cannot render.
class HardwareRasterizer : public Rasterizer {
 public:
  explicit HardwareRasterizer(backend::GraphicsContext* graphics_context,
                              int skia_atlas_width, int skia_atlas_height,
                              int scratch_surface_size_in_bytes,
                              int surface_cache_size_in_bytes,
                              int software_surface_cache_size_in_bytes,
                              bool purge_skia_font_caches_on_destruction);
  virtual ~HardwareRasterizer();

  // Consume the render tree and output the results to the render target passed
  // into the constructor.
  void Submit(const scoped_refptr<render_tree::Node>& render_tree,
              const scoped_refptr<backend::RenderTarget>& render_target,
              const Options& options) override;

  render_tree::ResourceProvider* GetResourceProvider() override;

  void MakeCurrent() override {}

 private:
  class Impl;
  scoped_ptr<Impl> impl_;
};

}  // namespace blitter
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // #if SB_HAS(BLITTER)

#endif  // COBALT_RENDERER_RASTERIZER_BLITTER_HARDWARE_RASTERIZER_H_
