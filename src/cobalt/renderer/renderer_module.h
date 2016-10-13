/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_RENDERER_RENDERER_MODULE_H_
#define COBALT_RENDERER_RENDERER_MODULE_H_

#include "cobalt/render_tree/node.h"
#include "cobalt/renderer/backend/display.h"
#include "cobalt/renderer/backend/graphics_context.h"
#include "cobalt/renderer/backend/graphics_system.h"
#include "cobalt/renderer/pipeline.h"
#include "cobalt/renderer/rasterizer/rasterizer.h"
#include "cobalt/system_window/system_window.h"

namespace cobalt {
namespace renderer {

class RendererModule {
 public:
  struct Options {
    Options();

    typedef base::Callback<scoped_ptr<rasterizer::Rasterizer>(
        backend::GraphicsContext*, const Options& options)>
        CreateRasterizerCallback;

    // The rasterizer must be created, accessed, and destroyed within the same
    // thread, and so to facilitate that a rasterizer factory function must
    // be provided here instead of the rasterizer itself.
    CreateRasterizerCallback create_rasterizer_function;

    // Determines the capacity of the scratch surface cache.  The scratch
    // surface cache facilitates the reuse of temporary offscreen surfaces
    // within a single frame.  This setting is only relevant when using the
    // hardware-accelerated Skia rasterizer.
    int scratch_surface_cache_size_in_bytes;

    // Determines the capacity of the skia cache.  The Skia cache is maintained
    // within Skia and is used to cache the results of complicated effects such
    // as shadows, so that Skia draw calls that are used repeatedly across
    // frames can be cached into surfaces.  This setting is only relevant when
    // using the hardware-accelerated Skia rasterizer.
    int skia_cache_size_in_bytes;

    // Determines the capacity of the surface cache.  The surface cache tracks
    // which render tree nodes are being re-used across frames and stores the
    // nodes that are most CPU-expensive to render into surfaces.
    int surface_cache_size_in_bytes;

    // If this flag is set to true, the pipeline will not re-submit a render
    // tree if it has not changed from the previous submission.  This can save
    // CPU time so long as there's no problem with the fact that the display
    // buffer will not be frequently swapped.
    bool submit_even_if_render_tree_is_unchanged;

   private:
    // Implemented per-platform, and allows each platform to customize
    // the renderer options.
    void SetPerPlatformDefaultOptions();
  };

  explicit RendererModule(system_window::SystemWindow* system_window,
                          const Options& options);
  ~RendererModule();

  void Suspend();
  void Resume();

  renderer::Pipeline* pipeline() { return pipeline_.get(); }
  const scoped_refptr<renderer::backend::RenderTarget> render_target() {
    return display_->GetRenderTarget();
  }

 private:
  system_window::SystemWindow* system_window_;
  Options options_;

  scoped_ptr<renderer::backend::GraphicsSystem> graphics_system_;
  scoped_ptr<renderer::backend::Display> display_;
  scoped_ptr<renderer::backend::GraphicsContext> graphics_context_;
  scoped_ptr<renderer::Pipeline> pipeline_;
};

}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RENDERER_MODULE_H_
