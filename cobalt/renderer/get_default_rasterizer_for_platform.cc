// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include <memory>

#include "cobalt/renderer/get_default_rasterizer_for_platform.h"

#include "cobalt/renderer/backend/graphics_context.h"
#include "cobalt/renderer/rasterizer/blitter/hardware_rasterizer.h"
#include "cobalt/renderer/rasterizer/blitter/software_rasterizer.h"
#include "cobalt/renderer/rasterizer/egl/hardware_rasterizer.h"
#include "cobalt/renderer/rasterizer/egl/software_rasterizer.h"
#include "cobalt/renderer/rasterizer/skia/hardware_rasterizer.h"
#include "cobalt/renderer/rasterizer/stub/rasterizer.h"
#include "cobalt/renderer/renderer_module.h"

namespace cobalt {
namespace renderer {

namespace {

#if COBALT_FORCE_STUB_RASTERIZER
std::unique_ptr<rasterizer::Rasterizer> CreateStubRasterizer(
    backend::GraphicsContext* graphics_context,
    const RendererModule::Options& options) {
  SB_UNREFERENCED_PARAMETER(graphics_context);
  return std::unique_ptr<rasterizer::Rasterizer>(
      new rasterizer::stub::Rasterizer());
}
#endif  // COBALT_FORCE_STUB_RASTERIZER

#if SB_HAS(GLES2)
std::unique_ptr<rasterizer::Rasterizer> CreateGLESSoftwareRasterizer(
    backend::GraphicsContext* graphics_context,
    const RendererModule::Options& options) {
  return std::unique_ptr<rasterizer::Rasterizer>(
      new rasterizer::egl::SoftwareRasterizer(
          graphics_context, options.purge_skia_font_caches_on_destruction));
}

std::unique_ptr<rasterizer::Rasterizer> CreateGLESHardwareRasterizer(
    backend::GraphicsContext* graphics_context,
    const RendererModule::Options& options) {
  return std::unique_ptr<rasterizer::Rasterizer>(
      new rasterizer::egl::HardwareRasterizer(
          graphics_context, options.skia_glyph_texture_atlas_dimensions.width(),
          options.skia_glyph_texture_atlas_dimensions.height(),
          options.skia_cache_size_in_bytes,
          options.scratch_surface_cache_size_in_bytes,
          options.offscreen_target_cache_size_in_bytes,
          options.purge_skia_font_caches_on_destruction,
          options.force_deterministic_rendering));
}

std::unique_ptr<rasterizer::Rasterizer> CreateSkiaHardwareRasterizer(
    backend::GraphicsContext* graphics_context,
    const RendererModule::Options& options) {
  return std::unique_ptr<rasterizer::Rasterizer>(
      new rasterizer::skia::HardwareRasterizer(
          graphics_context, options.skia_glyph_texture_atlas_dimensions.width(),
          options.skia_glyph_texture_atlas_dimensions.height(),
          options.skia_cache_size_in_bytes,
          options.scratch_surface_cache_size_in_bytes,
          options.purge_skia_font_caches_on_destruction,
          options.force_deterministic_rendering));
}
#endif  // #if SB_HAS(GLES2)

#if SB_HAS(BLITTER)
std::unique_ptr<rasterizer::Rasterizer> CreateBlitterSoftwareRasterizer(
    backend::GraphicsContext* graphics_context,
    const RendererModule::Options& options) {
  return std::unique_ptr<rasterizer::Rasterizer>(
      new rasterizer::blitter::SoftwareRasterizer(
          graphics_context, options.purge_skia_font_caches_on_destruction));
}

std::unique_ptr<rasterizer::Rasterizer> CreateBlitterHardwareRasterizer(
    backend::GraphicsContext* graphics_context,
    const RendererModule::Options& options) {
  return std::unique_ptr<rasterizer::Rasterizer>(
      new rasterizer::blitter::HardwareRasterizer(
          graphics_context, options.skia_glyph_texture_atlas_dimensions.width(),
          options.skia_glyph_texture_atlas_dimensions.height(),
          options.scratch_surface_cache_size_in_bytes,
          options.software_surface_cache_size_in_bytes,
          options.purge_skia_font_caches_on_destruction));
}
#endif  // SB_HAS(BLITTER)

}  // namespace

RasterizerInfo GetDefaultRasterizerForPlatform() {
#if COBALT_FORCE_STUB_RASTERIZER
  return {"stub", base::Bind(&CreateStubRasterizer)};
#else
#if SB_HAS(GLES2)
#if COBALT_FORCE_SOFTWARE_RASTERIZER
  return {"gles-software", base::Bind(&CreateGLESSoftwareRasterizer)};
#elif defined(COBALT_FORCE_DIRECT_GLES_RASTERIZER)
  return {"gles", base::Bind(&CreateGLESHardwareRasterizer)};
#else
  return {"skia", base::Bind(&CreateSkiaHardwareRasterizer)};
#endif  // COBALT_FORCE_SOFTWARE_RASTERIZER
#elif SB_HAS(BLITTER)
#if COBALT_FORCE_SOFTWARE_RASTERIZER
  return {"blitter-software", base::Bind(&CreateBlitterSoftwareRasterizer)};
#else
  return {"blitter", base::Bind(&CreateBlitterHardwareRasterizer)};
#endif  // COBALT_FORCE_SOFTWARE_RASTERIZER
#else
#error "Either GLES2 or the Starboard Blitter API must be available."
  return {"", NULL};
#endif
#endif  // #if COBALT_FORCE_STUB_RASTERIZER
}

}  // namespace renderer
}  // namespace cobalt
