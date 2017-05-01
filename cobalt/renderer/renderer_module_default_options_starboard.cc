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
#include "cobalt/renderer/rasterizer/blitter/hardware_rasterizer.h"
#include "cobalt/renderer/rasterizer/blitter/software_rasterizer.h"
#include "cobalt/renderer/rasterizer/egl/hardware_rasterizer.h"
#include "cobalt/renderer/rasterizer/egl/software_rasterizer.h"
#include "cobalt/renderer/rasterizer/skia/hardware_rasterizer.h"
#include "cobalt/renderer/rasterizer/stub/rasterizer.h"

namespace cobalt {
namespace renderer {

namespace {

scoped_ptr<rasterizer::Rasterizer> CreateRasterizer(
    backend::GraphicsContext* graphics_context,
    const RendererModule::Options& options) {
  TRACE_EVENT0("cobalt::renderer", "CreateRasterizer");
#if COBALT_FORCE_STUB_RASTERIZER
  return scoped_ptr<rasterizer::Rasterizer>(new rasterizer::stub::Rasterizer());
#else
#if SB_HAS(GLES2)
#if COBALT_FORCE_SOFTWARE_RASTERIZER
  return scoped_ptr<rasterizer::Rasterizer>(
      new rasterizer::egl::SoftwareRasterizer(
          graphics_context, options.surface_cache_size_in_bytes));
#elif defined(COBALT_FORCE_DIRECT_GLES_RASTERIZER)
  return scoped_ptr<rasterizer::Rasterizer>(
      new rasterizer::egl::HardwareRasterizer(
          graphics_context, options.skia_glyph_texture_atlas_dimensions.width(),
          options.skia_glyph_texture_atlas_dimensions.height(),
          options.skia_cache_size_in_bytes,
          options.scratch_surface_cache_size_in_bytes,
          options.surface_cache_size_in_bytes));
#else
  return scoped_ptr<rasterizer::Rasterizer>(
      new rasterizer::skia::HardwareRasterizer(
          graphics_context, options.skia_glyph_texture_atlas_dimensions.width(),
          options.skia_glyph_texture_atlas_dimensions.height(),
          options.skia_cache_size_in_bytes,
          options.scratch_surface_cache_size_in_bytes,
          options.surface_cache_size_in_bytes));
#endif  // COBALT_FORCE_SOFTWARE_RASTERIZER
#elif SB_HAS(BLITTER)
#if COBALT_FORCE_SOFTWARE_RASTERIZER
  return scoped_ptr<rasterizer::Rasterizer>(
      new rasterizer::blitter::SoftwareRasterizer(
          graphics_context, options.surface_cache_size_in_bytes));
#else
  return scoped_ptr<rasterizer::Rasterizer>(
      new rasterizer::blitter::HardwareRasterizer(
          graphics_context, options.skia_glyph_texture_atlas_dimensions.width(),
          options.skia_glyph_texture_atlas_dimensions.height(),
          options.scratch_surface_cache_size_in_bytes,
          options.surface_cache_size_in_bytes,
          options.software_surface_cache_size_in_bytes));
#endif  // COBALT_FORCE_SOFTWARE_RASTERIZER
#else
#error "Either GLES2 or the Starboard Blitter API must be available."
  return scoped_ptr<rasterizer::Rasterizer>();
#endif
#endif  // #if COBALT_FORCE_STUB_RASTERIZER
}
}  // namespace

void RendererModule::Options::SetPerPlatformDefaultOptions() {
  // Set default options from the current build's Starboard configuration.
  surface_cache_size_in_bytes = COBALT_SURFACE_CACHE_SIZE_IN_BYTES;
  // Default to 4MB, but this may be modified externally.
  skia_cache_size_in_bytes = 4 * 1024 * 1024;
  scratch_surface_cache_size_in_bytes =
      COBALT_SCRATCH_SURFACE_CACHE_SIZE_IN_BYTES;
  // 8MB default for software_surface_cache.
  software_surface_cache_size_in_bytes = 8 * 1024 * 1024;

  // If there is no need to frequently flip the display buffer, then enable
  // support for an optimization where the scene is not re-rasterized each frame
  // if it has not changed from the last frame.
  submit_even_if_render_tree_is_unchanged =
      SB_MUST_FREQUENTLY_FLIP_DISPLAY_BUFFER;

  create_rasterizer_function = base::Bind(&CreateRasterizer);
}

}  // namespace renderer
}  // namespace cobalt
