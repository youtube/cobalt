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

#ifndef COBALT_RENDERER_RASTERIZER_SKIA_SOFTWARE_RASTERIZER_H_
#define COBALT_RENDERER_RASTERIZER_SKIA_SOFTWARE_RASTERIZER_H_

#include "base/memory/scoped_ptr.h"
#include "cobalt/render_tree/resource_provider.h"
#include "cobalt/renderer/backend/graphics_context.h"
#include "cobalt/renderer/backend/render_target.h"
#include "cobalt/renderer/rasterizer/rasterizer.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

// This class provides a Rasterizer implementation where each render tree
// submission results in a software Skia SkBitmap being setup as a render
// target and rendered to.  Ultimately, the resulting SkBitmap will be
// uploaded as a texture and blitted to the render target through the
// use of the passed in graphics context.
class SkiaSoftwareRasterizer : public Rasterizer {
 public:
  // The graphics context will be used to issue commands to the GPU
  // to blit the final output to the render target.
  explicit SkiaSoftwareRasterizer(
      backend::GraphicsContext* graphics_context);

  // Consume the render tree and output the results to the render target passed
  // into the constructor.
  void Submit(
      const scoped_refptr<render_tree::Node>& render_tree,
      const scoped_refptr<backend::RenderTarget>& render_target) OVERRIDE;

  render_tree::ResourceProvider* GetResourceProvider() OVERRIDE;

 private:
  scoped_refptr<backend::RenderTarget> render_target_;
  backend::GraphicsContext* graphics_context_;
  scoped_ptr<render_tree::ResourceProvider> resource_provider_;
};

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RASTERIZER_SKIA_SOFTWARE_RASTERIZER_H_
