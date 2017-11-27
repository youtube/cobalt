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

#ifndef COBALT_RENDERER_RASTERIZER_BLITTER_SOFTWARE_RASTERIZER_H_
#define COBALT_RENDERER_RASTERIZER_BLITTER_SOFTWARE_RASTERIZER_H_

#include "starboard/configuration.h"

#if SB_HAS(BLITTER)

#include "base/memory/ref_counted.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/renderer/backend/blitter/graphics_context.h"
#include "cobalt/renderer/backend/graphics_context.h"
#include "cobalt/renderer/backend/render_target.h"
#include "cobalt/renderer/rasterizer/rasterizer.h"
#include "cobalt/renderer/rasterizer/skia/software_rasterizer.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace blitter {

// A software rasterizer which uses Skia to rasterize render trees on the CPU
// and then uses the Starboard Blitter API to transfer the result to the
// passed in given render target.
class SoftwareRasterizer : public Rasterizer {
 public:
  explicit SoftwareRasterizer(backend::GraphicsContext* context,
                              int surface_cache_size,
                              bool purge_skia_font_caches_on_destruction);

  void Submit(const scoped_refptr<render_tree::Node>& render_tree,
              const scoped_refptr<backend::RenderTarget>& render_target,
              const Options& options) override;

  render_tree::ResourceProvider* GetResourceProvider() override;

  void MakeCurrent() override {}

 private:
  backend::GraphicsContextBlitter* context_;
  skia::SoftwareRasterizer skia_rasterizer_;
};

}  // namespace blitter
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_HAS(BLITTER)

#endif  // COBALT_RENDERER_RASTERIZER_BLITTER_SOFTWARE_RASTERIZER_H_
