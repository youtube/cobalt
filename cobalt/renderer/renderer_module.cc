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
#include "cobalt/browser/memory_settings/calculations.h"
#include "cobalt/renderer/backend/default_graphics_system.h"
#include "cobalt/renderer/rasterizer/skia/hardware_rasterizer.h"
#include "cobalt/renderer/rasterizer/skia/software_rasterizer.h"

namespace cobalt {
namespace renderer {

RendererModule::Options::Options()
    : disable_rasterizer_caching(false),
      purge_skia_font_caches_on_destruction(true),
      enable_fps_stdout(false),
      enable_fps_overlay(false) {
  // These default values may ultimately be overridden by AutoMem.
  // These settings are suited for a 1080p frame.
  scratch_surface_cache_size_in_bytes = 0;
  skia_glyph_texture_atlas_dimensions.SetSize(2048, 2048);
  skia_cache_size_in_bytes = 4 * 1024 * 1024;

#if SB_HAS(GLES2)
#if defined(COBALT_FORCE_DIRECT_GLES_RASTERIZER)
  software_surface_cache_size_in_bytes = 0;
  offscreen_target_cache_size_in_bytes = 4 * 1024 * 1024;
#else
  software_surface_cache_size_in_bytes = 0;
  offscreen_target_cache_size_in_bytes = 0;
#endif
#else
  software_surface_cache_size_in_bytes = 8 * 1024 * 1024;
  offscreen_target_cache_size_in_bytes = 0;
#endif

  // Call into platform-specific code for setting up render module options.
  SetPerPlatformDefaultOptions();
}

RendererModule::RendererModule(system_window::SystemWindow* system_window,
                               const Options& options)
    : system_window_(system_window), options_(options) {
  TRACE_EVENT0("cobalt::renderer", "RendererModule::RendererModule()");
  // Load up the platform's default graphics system.
  {
    TRACE_EVENT0("cobalt::renderer", "backend::CreateDefaultGraphicsSystem()");
    graphics_system_ = backend::CreateDefaultGraphicsSystem(system_window_);
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
    renderer::Pipeline::Options pipeline_options;
    pipeline_options.enable_fps_stdout = options_.enable_fps_stdout;
    pipeline_options.enable_fps_overlay = options_.enable_fps_overlay;

    pipeline_ = make_scoped_ptr(new renderer::Pipeline(
        base::Bind(options_.create_rasterizer_function, graphics_context_.get(),
                   options_),
        display_->GetRenderTarget(), graphics_context_.get(),
        options_.submit_even_if_render_tree_is_unchanged,
        renderer::Pipeline::kClearToBlack, pipeline_options));
  }
}

RendererModule::~RendererModule() {
  TRACE_EVENT0("cobalt::renderer", "RendererModule::~RendererModule");
}

}  // namespace renderer
}  // namespace cobalt
