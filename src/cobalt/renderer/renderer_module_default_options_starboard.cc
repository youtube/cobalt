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

#include "cobalt/renderer/renderer_module.h"

#include "cobalt/renderer/rasterizer/blitter/hardware_rasterizer.h"
#include "cobalt/renderer/rasterizer/blitter/software_rasterizer.h"
#include "cobalt/renderer/rasterizer/egl/software_rasterizer.h"
#include "cobalt/renderer/rasterizer/skia/hardware_rasterizer.h"
#include "cobalt/renderer/rasterizer/stub/rasterizer.h"

namespace cobalt {
namespace renderer {

namespace {

scoped_ptr<rasterizer::Rasterizer> CreateRasterizer(
    backend::GraphicsContext* graphics_context,
    const RendererModule::Options& options) {
#if COBALT_FORCE_STUB_RASTERIZER
  return scoped_ptr<rasterizer::Rasterizer>(new rasterizer::stub::Rasterizer());
#else
#if SB_HAS(GLES2)
#if COBALT_FORCE_SOFTWARE_RASTERIZER
  return scoped_ptr<rasterizer::Rasterizer>(
      new rasterizer::egl::SoftwareRasterizer(
          graphics_context, options.surface_cache_size_in_bytes));
#else
  return scoped_ptr<rasterizer::Rasterizer>(
      new rasterizer::skia::HardwareRasterizer(
          graphics_context, options.skia_cache_size_in_bytes,
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
          graphics_context, options.surface_cache_size_in_bytes));
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
  skia_cache_size_in_bytes = COBALT_SKIA_CACHE_SIZE_IN_BYTES;
  scratch_surface_cache_size_in_bytes =
      COBALT_SCRATCH_SURFACE_CACHE_SIZE_IN_BYTES;

  create_rasterizer_function = base::Bind(&CreateRasterizer);
}

}  // namespace renderer
}  // namespace cobalt
