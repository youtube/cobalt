/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_HARDWARE_RASTERIZER_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_HARDWARE_RASTERIZER_H_

#include "base/memory/scoped_ptr.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/renderer/backend/graphics_context.h"
#include "cobalt/renderer/backend/render_target.h"
#include "cobalt/renderer/rasterizer/rasterizer.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

// This SkiaHardwareRasterizer class represents a rasterizer that will setup
// a Skia hardware rendering context.  When Submit() is called, the passed in
// render tree will be rasterized using hardware-accelerated Skia.  The
// SkiaHardwareRasterizer must be constructed on the same thread that Submit()
// is to be called on.
class SkiaHardwareRasterizer : public Rasterizer {
 public:
  enum SurfaceCachingSwitch {
    kSurfaceCachingEnabled,
    kSurfaceCachingDisabled,
  };

  // The passed in render target will be used to determine the dimensions of
  // the output.  The graphics context will be used to issue commands to the GPU
  // to blit the final output to the render target.
  explicit SkiaHardwareRasterizer(backend::GraphicsContext* graphics_context,
                                  SurfaceCachingSwitch surface_caching_switch);
  virtual ~SkiaHardwareRasterizer();

  // Consume the render tree and output the results to the render target passed
  // into the constructor.
  void Submit(const scoped_refptr<render_tree::Node>& render_tree,
              const scoped_refptr<backend::RenderTarget>& render_target,
              int options) OVERRIDE;

  render_tree::ResourceProvider* GetResourceProvider() OVERRIDE;

 private:
  class Impl;
  scoped_ptr<Impl> impl_;
};

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_HARDWARE_RASTERIZER_H_
