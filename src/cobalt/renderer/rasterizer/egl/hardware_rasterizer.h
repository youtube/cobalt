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

#ifndef COBALT_RENDERER_RASTERIZER_EGL_HARDWARE_RASTERIZER_H_
#define COBALT_RENDERER_RASTERIZER_EGL_HARDWARE_RASTERIZER_H_

#include "base/memory/scoped_ptr.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/renderer/backend/graphics_context.h"
#include "cobalt/renderer/backend/render_target.h"
#include "cobalt/renderer/rasterizer/rasterizer.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

// A hardware rasterizer which directly uses EGL/GLES2 to rasterize render
// trees onto the given render target.  For certain effects, it will fall back
// to the Skia hardware rasterizer.
class HardwareRasterizer : public Rasterizer {
 public:
  // The passed in render target will be used to determine the dimensions of
  // the output.  The graphics context will be used to issue commands to the GPU
  // to blit the final output to the render target.
  // The value of |skia_cache_size_in_bytes| dictates the maximum amount of
  // memory that Skia will use to cache the results of certain effects that take
  // a long time to render, such as shadows.  The results will be reused across
  // submissions.
  // The value of |scratch_surface_cache_size_in_bytes| sets an upper limit on
  // the number of bytes that can be consumed by the scratch surface cache,
  // a facility that allows temporary surfaces to be reused within the
  // rasterization of a single frame/submission.
  // The value of |offscreen_target_cache_size_in_bytes| sets an upper limit on
  // how much GPU memory is used to save the results of render tree nodes to
  // improve performance. This should be non-zero, otherwise performance
  // degrades drastically.
  // |disable_rasterizer_caching| disables caching of render tree node outputs.
  // This significantly degrades performance but provides sub-pixel correctness.
  // This should only be done for testing purposes.
  explicit HardwareRasterizer(backend::GraphicsContext* graphics_context,
                              int skia_atlas_width, int skia_atlas_height,
                              int skia_cache_size_in_bytes,
                              int scratch_surface_cache_size_in_bytes,
                              int offscreen_target_cache_size_in_bytes,
                              bool purge_skia_font_caches_on_destruction,
                              bool disable_rasterizer_caching);
  virtual ~HardwareRasterizer();

  void Submit(const scoped_refptr<render_tree::Node>& render_tree,
              const scoped_refptr<backend::RenderTarget>& render_target,
              const Options& options) override;

  render_tree::ResourceProvider* GetResourceProvider() override;

  void MakeCurrent() override;

 private:
  class Impl;
  scoped_ptr<Impl> impl_;
};

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_EGL_HARDWARE_RASTERIZER_H_
