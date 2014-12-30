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

#include "cobalt/renderer/backend/default_graphics_system.h"
#include "cobalt/renderer/rasterizer_skia/hardware_rasterizer.h"
#include "cobalt/renderer/rasterizer_skia/software_rasterizer.h"
#include "cobalt/renderer/renderer_module.h"

namespace cobalt {
namespace renderer {

RendererModule::Options::Options() : use_hardware_skia_rasterizer(false) {}

RendererModule::RendererModule(const Options& options) {
  // Load up the platform's default graphics system.
  graphics_system_ = renderer::backend::CreateDefaultGraphicsSystem();

  // Create/initialize the default display
  display_ = graphics_system_->CreateDefaultDisplay();

  // Create a graphics context associated with the default display's render
  // target so that we have a channel to write to the display.
  scoped_ptr<renderer::backend::GraphicsContext> primary_graphics_context(
      graphics_system_->CreateGraphicsContext(display_->GetRenderTarget()));

  // Create a Skia rasterizer to rasterize our render trees and
  // send output directly to the display.
  scoped_ptr<renderer::Rasterizer> rasterizer;
  if (options.use_hardware_skia_rasterizer) {
    rasterizer.reset(new renderer::rasterizer_skia::SkiaHardwareRasterizer(
        display_->GetRenderTarget(), primary_graphics_context.Pass()));
  } else {
    rasterizer.reset(new renderer::rasterizer_skia::SkiaSoftwareRasterizer(
        display_->GetRenderTarget(), primary_graphics_context.Pass()));
  }

  // Setup the threaded rendering pipeline and fit our newly created rasterizer
  // into it.
  pipeline_ = make_scoped_ptr(new renderer::Pipeline(rasterizer.Pass()));
}

RendererModule::~RendererModule() {
  pipeline_.reset();
  display_.reset();
  graphics_system_.reset();
}

}  // namespace renderer
}  // namespace cobalt
