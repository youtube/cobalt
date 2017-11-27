// Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_HARDWARE_RASTERIZER_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_HARDWARE_RASTERIZER_H_

#include "base/memory/scoped_ptr.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/renderer/backend/graphics_context.h"
#include "cobalt/renderer/backend/render_target.h"
#include "cobalt/renderer/rasterizer/rasterizer.h"

class GrContext;
class SkCanvas;

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

// This HardwareRasterizer class represents a rasterizer that will setup
// a Skia hardware rendering context.  When Submit() is called, the passed in
// render tree will be rasterized using hardware-accelerated Skia.  The
// HardwareRasterizer must be constructed on the same thread that Submit()
// is to be called on.
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
  // If |surface_cache_size| is non-zero, the rasterizer will set itself up with
  // a surface cache such that expensive render tree nodes seen multiple times
  // will get saved to offscreen surfaces.
  explicit HardwareRasterizer(backend::GraphicsContext* graphics_context,
                              int skia_atlas_width, int skia_atlas_height,
                              int skia_cache_size_in_bytes,
                              int scratch_surface_cache_size_in_bytes,
                              int surface_cache_size_in_bytes,
                              bool purge_skia_font_caches_on_destruction);
  virtual ~HardwareRasterizer();

  // Consume the render tree and output the results to the render target passed
  // into the constructor.
  void Submit(const scoped_refptr<render_tree::Node>& render_tree,
              const scoped_refptr<backend::RenderTarget>& render_target,
              const Options& options) override;

  // Consume the render tree and output the results to the specified canvas.
  void SubmitOffscreen(const scoped_refptr<render_tree::Node>& render_tree,
                       SkCanvas* canvas);

  // Get the cached canvas for a render target that would normally go through
  // Submit(). The cache size is limited, so this should not be used for
  // generic offscreen render targets.
  SkCanvas* GetCachedCanvas(
      const scoped_refptr<backend::RenderTarget>& render_target);

  // If Submit() is not called, then use this function to tell rasterizer that
  // a frame has been submitted.
  void AdvanceFrame();

  render_tree::ResourceProvider* GetResourceProvider() override;
  GrContext* GetGrContext();

  void MakeCurrent() override;

 private:
  class Impl;
  scoped_ptr<Impl> impl_;
};

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_HARDWARE_RASTERIZER_H_
