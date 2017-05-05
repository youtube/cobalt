// Copyright 2017 Google Inc. All Rights Reserved.
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
#include "cobalt/renderer/rasterizer/lib/external_rasterizer.h"

namespace cobalt {
namespace renderer {

namespace {
scoped_ptr<rasterizer::Rasterizer> CreateRasterizer(
    backend::GraphicsContext* graphics_context,
    const RendererModule::Options& options) {
  return scoped_ptr<rasterizer::Rasterizer>(
      new rasterizer::lib::ExternalRasterizer(
        graphics_context,
        options.skia_glyph_texture_atlas_dimensions.width(),
        options.skia_glyph_texture_atlas_dimensions.height(),
        options.skia_cache_size_in_bytes,
        options.scratch_surface_cache_size_in_bytes,
        options.surface_cache_size_in_bytes));
}
}  // namespace

void RendererModule::Options::SetPerPlatformDefaultOptions() {
  // Set default options from the current build's configuration.
  surface_cache_size_in_bytes = COBALT_SURFACE_CACHE_SIZE_IN_BYTES;

  // Default to 4MB, but this may be modified externally.
  skia_cache_size_in_bytes = 4 * 1024 * 1024;

  scratch_surface_cache_size_in_bytes =
      COBALT_SCRATCH_SURFACE_CACHE_SIZE_IN_BYTES;

  // Ensure the scene is re-rasterized even if the render tree is unchanged so
  // that headset look changes are properly rendered.
  submit_even_if_render_tree_is_unchanged = true;

  create_rasterizer_function = base::Bind(&CreateRasterizer);
}

}  // namespace renderer
}  // namespace cobalt
