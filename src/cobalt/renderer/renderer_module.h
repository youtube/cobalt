// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDERER_RENDERER_MODULE_H_
#define COBALT_RENDERER_RENDERER_MODULE_H_

#include "cobalt/base/camera_transform.h"
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

    typedef base::Callback<base::CameraTransform()> GetCameraTransformCallback;

    // The rasterizer must be created, accessed, and destroyed within the same
    // thread, and so to facilitate that a rasterizer factory function must
    // be provided here instead of the rasterizer itself.
    CreateRasterizerCallback create_rasterizer_function;

    // Determines the capacity of the scratch surface cache.  The scratch
    // surface cache facilitates the reuse of temporary offscreen surfaces
    // within a single frame.  This setting is only relevant when using the
    // hardware-accelerated Skia rasterizer.
    int scratch_surface_cache_size_in_bytes;

    // Represents the dimensions of the skia texture atlas which holds all of
    // the glyph textures used during rendering.
    math::Size skia_glyph_texture_atlas_dimensions;

    // Determines the capacity of the skia cache.  The Skia cache is maintained
    // within Skia and is used to cache the results of complicated effects such
    // as shadows, so that Skia draw calls that are used repeatedly across
    // frames can be cached into surfaces.  This setting is only relevant when
    // using the hardware-accelerated Skia rasterizer.
    int skia_cache_size_in_bytes;

    // Only relevant if you are using the Blitter API.
    // Determines the capacity of the software surface cache, which is used to
    // cache all surfaces that are rendered via a software rasterizer to avoid
    // re-rendering them.
    int software_surface_cache_size_in_bytes;

    // Determines the amount of GPU memory the offscreen target atlases will
    // use. This is specific to the direct-GLES rasterizer and caches any render
    // tree nodes which require skia for rendering. Two atlases will be
    // allocated from this memory or multiple atlases of the frame size if the
    // limit allows. It is recommended that enough memory be reserved for two
    // RGBA atlases about a quarter of the frame size.
    int offscreen_target_cache_size_in_bytes;

    // By default, some rasterizers may cache the output of certain render
    // tree nodes to improve render performance. However, this may result in
    // pixel differences if the cached output is rendered to the screen using
    // a sub-pixel offset that is different from when the cache was created.
    // This caching mechanism should only be disabled for testing purposes
    // (e.g. screenshot diff tools).
    bool disable_rasterizer_caching;

    // If this flag is set to true, the pipeline will not re-submit a render
    // tree if it has not changed from the previous submission.  This can save
    // CPU time so long as there's no problem with the fact that the display
    // buffer will not be frequently swapped.
    bool submit_even_if_render_tree_is_unchanged;

    // If this flag is set to true, which is the default value, then all of
    // Skia's font caches are purged during destruction. These caches have
    // lifespans that are uncoupled from the resource provider's, and will not
    // be purged on destruction when this is set to false.
    bool purge_skia_font_caches_on_destruction;

    // On modes with a 3D camera controllable by user input, fetch the affine
    // transforms needed to render the scene from the camera's view.
    GetCameraTransformCallback get_camera_transform;

    bool enable_fps_stdout;
    bool enable_fps_overlay;

   private:
    // Implemented per-platform, and allows each platform to customize
    // the renderer options.
    void SetPerPlatformDefaultOptions();
  };

  explicit RendererModule(system_window::SystemWindow* system_window,
                          const Options& options);
  ~RendererModule();

  renderer::Pipeline* pipeline() { return pipeline_.get(); }
  const scoped_refptr<renderer::backend::RenderTarget> render_target() {
    return display_->GetRenderTarget();
  }

  render_tree::ResourceProvider* resource_provider() {
    if (!pipeline_) {
      return NULL;
    }

    return pipeline_->GetResourceProvider();
  }

  math::Size render_target_size() { return render_target()->GetSize(); }

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
