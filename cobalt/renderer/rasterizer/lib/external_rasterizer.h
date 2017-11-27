// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDERER_RASTERIZER_LIB_EXTERNAL_RASTERIZER_H_
#define COBALT_RENDERER_RASTERIZER_LIB_EXTERNAL_RASTERIZER_H_

#include "base/memory/ref_counted.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/renderer/backend/graphics_context.h"
#include "cobalt/renderer/backend/render_target.h"
#include "cobalt/renderer/rasterizer/rasterizer.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace lib {

// The ExternalRasterizer is the Rasterizer used by Cobalt client libs to enable
// them to rasterize into Cobalt's system window.
// The primary responsibilities of the ExternalRasterizer are to provide clients
// with a window and GL context as well as rasterizing RenderTrees into
// textures that are provided to clients to render into their own GL scene.
class ExternalRasterizer : public Rasterizer {
 public:
  ExternalRasterizer(backend::GraphicsContext* graphics_context,
                     int skia_atlas_width, int skia_atlas_height,
                     int skia_cache_size_in_bytes,
                     int scratch_surface_cache_size_in_bytes,
                     int rasterizer_gpu_cache_size_in_bytes,
                     bool purge_skia_font_caches_on_destruction);
  virtual ~ExternalRasterizer();

  void Submit(const scoped_refptr<render_tree::Node>& render_tree,
              const scoped_refptr<backend::RenderTarget>& render_target,
              const Options& options) override;

  // Note, this will simply pipe through the HardwareRasterizer's
  // ResourceProvider.
  render_tree::ResourceProvider* GetResourceProvider() override;

  void MakeCurrent() override;

  class Impl;

 private:
  scoped_ptr<Impl> impl_;
};

}  // namespace lib
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_LIB_EXTERNAL_RASTERIZER_H_
