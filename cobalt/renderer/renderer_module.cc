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

#include "cobalt/renderer/renderer_module.h"

#include "base/debug/trace_event.h"
#include "cobalt/browser/memory_settings/memory_settings.h"
#include "cobalt/renderer/backend/default_graphics_system.h"
#include "cobalt/renderer/rasterizer/skia/hardware_rasterizer.h"
#include "cobalt/renderer/rasterizer/skia/software_rasterizer.h"

namespace cobalt {
namespace renderer {

RendererModule::Options::Options()
    : skia_texture_atlas_dimensions(2048, 2048) {
  // Call into platform-specific code for setting up render module options.
  SetPerPlatformDefaultOptions();
}

RendererModule::RendererModule(system_window::SystemWindow* system_window,
                               const Options& options)
    : system_window_(system_window), options_(options) {
  TRACE_EVENT0("cobalt::renderer", "RendererModule::RendererModule()");
  Resume();
}

RendererModule::~RendererModule() {}

void RendererModule::Suspend() {
  TRACE_EVENT0("cobalt::renderer", "RendererModule::Suspend()");
  pipeline_.reset();
  graphics_context_.reset();
  display_.reset();
  graphics_system_.reset();
}

void RendererModule::Resume() {
  TRACE_EVENT0("cobalt::renderer", "RendererModule::Resume()");

  // Load up the platform's default graphics system.
  {
    TRACE_EVENT0("cobalt::renderer", "backend::CreateDefaultGraphicsSystem()");
    graphics_system_ = backend::CreateDefaultGraphicsSystem();
  }

  // Create/initialize the display using the system window
  // specified in the constructor args to get the render target.
  {
    TRACE_EVENT0("cobalt::renderer", "GraphicsSystem::CreateDisplay()");
    DCHECK(system_window_);
    display_ = graphics_system_->CreateDisplay(system_window_);
  }

  // Create a graphics context associated with the default display's render
  // target so that we have a channel to write to the display.
  {
    TRACE_EVENT0("cobalt::renderer", "GraphicsSystem::CreateGraphicsContext()");
    graphics_context_ = graphics_system_->CreateGraphicsContext();
  }

  // Setup the threaded rendering pipeline and pass it the rasterizer creator
  // function so that it can create a rasterizer on its own thread.
  // Direct it to render directly to the display.
  {
    TRACE_EVENT0("cobalt::renderer", "new renderer::Pipeline()");
    pipeline_ = make_scoped_ptr(new renderer::Pipeline(
        base::Bind(options_.create_rasterizer_function, graphics_context_.get(),
                   options_),
        display_->GetRenderTarget(), graphics_context_.get(),
        options_.submit_even_if_render_tree_is_unchanged,
        renderer::Pipeline::kClearToBlack));
  }
}

}  // namespace renderer
}  // namespace cobalt
