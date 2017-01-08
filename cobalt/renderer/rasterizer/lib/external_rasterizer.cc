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

#include "cobalt/renderer/rasterizer/lib/external_rasterizer.h"

#include <cmath>
#include <vector>

#include "base/threading/thread_checker.h"
#include "cobalt/renderer/backend/egl/graphics_context.h"
#include "cobalt/renderer/backend/egl/render_target.h"
#include "cobalt/renderer/rasterizer/lib/imported/graphics.h"
#include "cobalt/renderer/rasterizer/skia/hardware_rasterizer.h"
#include "starboard/shared/gles/gl_call.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace lib {

class ExternalRasterizer::Impl {
 public:
  Impl(backend::GraphicsContext* graphics_context,
       int skia_atlas_width, int skia_atlas_height,
       int skia_cache_size_in_bytes,
       int scratch_surface_cache_size_in_bytes,
       int surface_cache_size_in_bytes);
  ~Impl();

  void Submit(const scoped_refptr<render_tree::Node>& render_tree,
              const scoped_refptr<backend::RenderTarget>& render_target);

  render_tree::ResourceProvider* GetResourceProvider();

 private:
  base::ThreadChecker thread_checker_;

  backend::GraphicsContextEGL* graphics_context_;

  skia::HardwareRasterizer hardware_rasterizer_;
  skia::HardwareRasterizer::Options options_;

  // The main offscreen render target to use when rendering UI or rectangular
  // video.
  scoped_refptr<backend::RenderTarget> main_offscreen_render_target_;
  scoped_ptr<backend::TextureEGL> main_texture_;
  // TODO: Add in a RenderTarget for the video texture.
};

ExternalRasterizer::Impl::Impl(backend::GraphicsContext* graphics_context,
                               int skia_atlas_width, int skia_atlas_height,
                               int skia_cache_size_in_bytes,
                               int scratch_surface_cache_size_in_bytes,
                               int surface_cache_size_in_bytes)
    : graphics_context_(
          base::polymorphic_downcast<backend::GraphicsContextEGL*>(
              graphics_context)),
      hardware_rasterizer_(graphics_context, skia_atlas_width,
                           skia_atlas_height, skia_cache_size_in_bytes,
                           scratch_surface_cache_size_in_bytes,
                           surface_cache_size_in_bytes) {
  options_.flags = skia::HardwareRasterizer::kSubmitFlags_Clear;
  graphics_context_->MakeCurrent();

  // TODO: Import the correct size for this and any other textures from the lib
  // client and re-generate the size as appropriate.
  main_offscreen_render_target_ =
      graphics_context_->CreateOffscreenRenderTarget(math::Size(1920, 1080));
  main_texture_.reset(new backend::TextureEGL(
      graphics_context_,
      make_scoped_refptr(
          base::polymorphic_downcast<backend::PBufferRenderTargetEGL*>(
              main_offscreen_render_target_.get()))));

  CbLibOnGraphicsContextCreated();
}

ExternalRasterizer::Impl::~Impl() { graphics_context_->MakeCurrent(); }

void ExternalRasterizer::Impl::Submit(
    const scoped_refptr<render_tree::Node>& render_tree,
    const scoped_refptr<backend::RenderTarget>& render_target) {
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(0);
  backend::RenderTargetEGL* render_target_egl =
      base::polymorphic_downcast<backend::RenderTargetEGL*>(
          render_target.get());
  graphics_context_->MakeCurrentWithSurface(render_target_egl);

  backend::RenderTargetEGL* main_texture_render_target_egl =
      base::polymorphic_downcast<backend::RenderTargetEGL*>(
          main_offscreen_render_target_.get());
  hardware_rasterizer_.Submit(render_tree, main_offscreen_render_target_,
                              options_);

  const intptr_t texture_handle = main_texture_->GetPlatformHandle();
  // TODO: Provide mesh data to clients for map-to-mesh playbacks and a separate
  // video texture handle.
  // TODO: Allow clients to specify arbitrary subtrees to render into different
  // textures?
  CbLibRenderFrame(texture_handle);

  graphics_context_->SwapBuffers(render_target_egl);
}

render_tree::ResourceProvider* ExternalRasterizer::Impl::GetResourceProvider() {
  return hardware_rasterizer_.GetResourceProvider();
}

ExternalRasterizer::ExternalRasterizer(
    backend::GraphicsContext* graphics_context, int skia_atlas_width,
    int skia_atlas_height, int skia_cache_size_in_bytes,
    int scratch_surface_cache_size_in_bytes, int surface_cache_size_in_bytes)
    : impl_(new Impl(graphics_context, skia_atlas_width, skia_atlas_height,
                     skia_cache_size_in_bytes,
                     scratch_surface_cache_size_in_bytes,
                     surface_cache_size_in_bytes)) {}

ExternalRasterizer::~ExternalRasterizer() {}

void ExternalRasterizer::Submit(
    const scoped_refptr<render_tree::Node>& render_tree,
    const scoped_refptr<backend::RenderTarget>& render_target,
    const Options& options) {
  impl_->Submit(render_tree, render_target);
}

render_tree::ResourceProvider* ExternalRasterizer::GetResourceProvider() {
  return impl_->GetResourceProvider();
}

}  // namespace lib
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
