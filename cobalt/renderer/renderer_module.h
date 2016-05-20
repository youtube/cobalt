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
        backend::GraphicsContext*)> CreateRasterizerCallback;
    CreateRasterizerCallback create_rasterizer_function;

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

 private:
  scoped_ptr<renderer::backend::GraphicsSystem> graphics_system_;
  scoped_ptr<renderer::backend::Display> display_;
  scoped_ptr<renderer::backend::GraphicsContext> graphics_context_;
  scoped_ptr<renderer::Pipeline> pipeline_;
};

}  // namespace renderer
}  // namespace cobalt

#endif  // COBALT_RENDERER_RENDERER_MODULE_H_
