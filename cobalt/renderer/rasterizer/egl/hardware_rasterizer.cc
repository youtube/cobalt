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

#include "cobalt/renderer/rasterizer/egl/hardware_rasterizer.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "base/debug/trace_event.h"
#include "base/memory/scoped_vector.h"
#include "base/threading/thread_checker.h"
#include "cobalt/renderer/backend/egl/graphics_context.h"
#include "cobalt/renderer/backend/egl/graphics_system.h"
#include "cobalt/renderer/backend/egl/texture.h"
#include "cobalt/renderer/backend/egl/utils.h"
#include "cobalt/renderer/frame_rate_throttler.h"
#include "cobalt/renderer/rasterizer/egl/draw_object_manager.h"
#include "cobalt/renderer/rasterizer/egl/graphics_state.h"
#include "cobalt/renderer/rasterizer/egl/offscreen_target_manager.h"
#include "cobalt/renderer/rasterizer/egl/render_tree_node_visitor.h"
#include "cobalt/renderer/rasterizer/egl/shader_program_manager.h"
#include "cobalt/renderer/rasterizer/skia/hardware_rasterizer.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

class HardwareRasterizer::Impl {
 public:
  explicit Impl(backend::GraphicsContext* graphics_context,
                int skia_atlas_width, int skia_atlas_height,
                int skia_cache_size_in_bytes,
                int scratch_surface_cache_size_in_bytes,
                int surface_cache_size_in_bytes);
  ~Impl();

  void Submit(const scoped_refptr<render_tree::Node>& render_tree,
              const scoped_refptr<backend::RenderTarget>& render_target,
              const Options& options);

  void SubmitToFallbackRasterizer(
      const scoped_refptr<render_tree::Node>& render_tree,
      const math::RectF& viewport_unscaled,
      const math::SizeF& viewport_scale,
      const backend::TextureEGL** out_texture,
      math::Matrix3F* out_texcoord_transform);

  render_tree::ResourceProvider* GetResourceProvider() {
    return fallback_rasterizer_->GetResourceProvider();
  }

 private:
  GrContext* GetFallbackContext() {
    return fallback_rasterizer_->GetGrContext();
  }

  void RasterizeTree(const scoped_refptr<render_tree::Node>& render_tree,
                     backend::RenderTargetEGL* render_target);

  scoped_ptr<skia::HardwareRasterizer> fallback_rasterizer_;
  scoped_ptr<GraphicsState> graphics_state_;
  scoped_ptr<ShaderProgramManager> shader_program_manager_;
  scoped_ptr<OffscreenTargetManager> offscreen_target_manager_;

  backend::GraphicsContextEGL* graphics_context_;
  FrameRateThrottler frame_rate_throttler_;
  base::ThreadChecker thread_checker_;
};

HardwareRasterizer::Impl::Impl(backend::GraphicsContext* graphics_context,
                               int skia_atlas_width, int skia_atlas_height,
                               int skia_cache_size_in_bytes,
                               int scratch_surface_cache_size_in_bytes,
                               int surface_cache_size_in_bytes)
    : fallback_rasterizer_(new skia::HardwareRasterizer(
          graphics_context, skia_atlas_width, skia_atlas_height,
          skia_cache_size_in_bytes, scratch_surface_cache_size_in_bytes,
          surface_cache_size_in_bytes)),
      graphics_context_(
          base::polymorphic_downcast<backend::GraphicsContextEGL*>(
              graphics_context)) {
  backend::GraphicsContextEGL::ScopedMakeCurrent scoped_make_current(
      graphics_context_);

  graphics_state_.reset(new GraphicsState());
  shader_program_manager_.reset(new ShaderProgramManager());
  offscreen_target_manager_.reset(
      new OffscreenTargetManager(graphics_context_, GetFallbackContext()));
}

HardwareRasterizer::Impl::~Impl() {
  backend::GraphicsContextEGL::ScopedMakeCurrent scoped_make_current(
      graphics_context_);

  GL_CALL(glFinish());
  offscreen_target_manager_.reset();
  shader_program_manager_.reset();
  graphics_state_.reset();
}

void HardwareRasterizer::Impl::Submit(
    const scoped_refptr<render_tree::Node>& render_tree,
    const scoped_refptr<backend::RenderTarget>& render_target,
    const Options& options) {
  DCHECK(thread_checker_.CalledOnValidThread());

  backend::RenderTargetEGL* render_target_egl =
      base::polymorphic_downcast<backend::RenderTargetEGL*>(
          render_target.get());
  backend::GraphicsContextEGL::ScopedMakeCurrent scoped_make_current(
      graphics_context_, render_target_egl);

  fallback_rasterizer_->AdvanceFrame();

  const math::Size& target_size = render_target->GetSize();
  graphics_state_->SetClipAdjustment(target_size);
  graphics_state_->Viewport(0, 0, target_size.width(), target_size.height());

  // Update only the dirty pixels if the render target contents are preserved
  // between frames.
  if (options.dirty && render_target_egl->ContentWasPreservedAfterSwap()) {
    graphics_state_->Scissor(options.dirty->x(), options.dirty->y(),
        options.dirty->width(), options.dirty->height());
  } else {
    graphics_state_->Scissor(0, 0, target_size.width(), target_size.height());
  }

  offscreen_target_manager_->Update(target_size);

  RasterizeTree(render_tree, render_target_egl);

  frame_rate_throttler_.EndInterval();
  graphics_context_->SwapBuffers(render_target_egl);
  frame_rate_throttler_.BeginInterval();
}

void HardwareRasterizer::Impl::SubmitToFallbackRasterizer(
    const scoped_refptr<render_tree::Node>& render_tree,
    const math::RectF& viewport_unscaled,
    const math::SizeF& viewport_scale,
    const backend::TextureEGL** out_texture,
    math::Matrix3F* out_texcoord_transform) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("cobalt::renderer", "SubmitToFallbackRasterizer");

  // Get an offscreen target for rendering.
  math::RectF target_rect(0, 0, 0, 0);
  backend::FramebufferEGL* framebuffer = NULL;
  SkCanvas* canvas = NULL;

  // Handle scaling of contents as requested.
  math::RectF viewport(viewport_unscaled.x() * viewport_scale.width(),
                       viewport_unscaled.y() * viewport_scale.height(),
                       viewport_unscaled.width() * viewport_scale.width(),
                       viewport_unscaled.height() * viewport_scale.height());

  // It's possible for the desired viewport to be larger than can be allocated.
  // In this situation, rendering will need to be scaled down.
  math::RectF viewport_actual(viewport);

  // See if there is already a cache of the results. If not, then allocate
  // a target and render to it.
  if (!offscreen_target_manager_->GetCachedOffscreenTarget(
      render_tree.get(), viewport.size(),
      &framebuffer, &canvas, &target_rect)) {
    offscreen_target_manager_->AllocateOffscreenTarget(
        render_tree.get(), viewport.size(),
        &framebuffer, &canvas, &target_rect);

    // Use skia to rasterize to the allocated offscreen target.
    canvas->save();
    if (target_rect.width() < viewport.width() ||
        target_rect.height() < viewport.height()) {
      // The allocated offscreen target is smaller than requested. Scale
      // rendering so it can fit into the smaller target.
      math::SizeF scale(target_rect.width() / viewport.width(),
                        target_rect.height() / viewport.height());
      viewport_actual.SetRect(viewport_actual.x() * scale.width(),
                              viewport_actual.y() * scale.height(),
                              viewport_actual.width() * scale.width(),
                              viewport_actual.height() * scale.height());
      canvas->clipRect(SkRect::MakeXYWH(
          target_rect.x(), target_rect.y(),
          viewport_actual.width() + 0.5f, viewport_actual.height() + 0.5f));
      canvas->translate(viewport_actual.x() + target_rect.x(),
                        viewport_actual.y() + target_rect.y());
      canvas->scale(scale.width(), scale.height());
    } else {
      canvas->clipRect(SkRect::MakeXYWH(
          target_rect.x(), target_rect.y(),
          viewport_actual.width() + 0.5f, viewport_actual.height() + 0.5f));
      canvas->translate(viewport_actual.x() + target_rect.x(),
                        viewport_actual.y() + target_rect.y());
    }

    if (viewport_scale.width() != 1.0f || viewport_scale.height() != 1.0f) {
      canvas->scale(viewport_scale.width(), viewport_scale.height());
    }

    fallback_rasterizer_->SubmitOffscreen(render_tree, canvas);
    canvas->restore();
  } else if (target_rect.width() < viewport.width() ||
             target_rect.height() < viewport.height()) {
    // The cached offscreen target is smaller than originally requested.
    math::SizeF scale(target_rect.width() / viewport.width(),
                      target_rect.height() / viewport.height());
    viewport_actual.SetRect(viewport_actual.x() * scale.width(),
                            viewport_actual.y() * scale.height(),
                            viewport_actual.width() * scale.width(),
                            viewport_actual.height() * scale.height());
  }

  float scale_x = 1.0f / framebuffer->GetSize().width();
  float scale_y = 1.0f / framebuffer->GetSize().height();
  *out_texcoord_transform = math::Matrix3F::FromValues(
      viewport_actual.width() * scale_x, 0, target_rect.x() * scale_x,
      0, viewport_actual.height() * scale_y, target_rect.y() * scale_y,
      0, 0, 1);
  *out_texture = framebuffer->GetColorTexture();
}

void HardwareRasterizer::Impl::RasterizeTree(
    const scoped_refptr<render_tree::Node>& render_tree,
    backend::RenderTargetEGL* render_target) {
  DrawObjectManager draw_object_manager;
  RenderTreeNodeVisitor::FallbackRasterizeFunction fallback_rasterize =
      base::Bind(&HardwareRasterizer::Impl::SubmitToFallbackRasterizer,
                 base::Unretained(this));
  RenderTreeNodeVisitor visitor(graphics_state_.get(),
                                &draw_object_manager,
                                &fallback_rasterize);

  // Traverse the render tree to populate the draw object manager.
  {
    TRACE_EVENT0("cobalt::renderer", "VisitRenderTree");
    render_tree->Accept(&visitor);
  }

  graphics_state_->BeginFrame();

  // Rasterize to offscreen targets using skia.
  {
    TRACE_EVENT0("cobalt::renderer", "OffscreenRasterize");
    backend::GraphicsContextEGL::ScopedMakeCurrent scoped_make_current(
        graphics_context_, render_target);

    // Reset the skia graphics context since the egl rasterizer dirtied it.
    GetFallbackContext()->resetContext();
    draw_object_manager.ExecuteOffscreenRasterize(graphics_state_.get(),
        shader_program_manager_.get());

    {
      TRACE_EVENT0("cobalt::renderer", "Skia Flush");
      offscreen_target_manager_->Flush();
    }

    // Reset the egl graphics state since skia dirtied it.
    graphics_state_->SetDirty();
    GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));
  }

  graphics_state_->Clear();

  {
    TRACE_EVENT0("cobalt::renderer", "OnscreenUpdateVertexBuffer");
    draw_object_manager.ExecuteOnscreenUpdateVertexBuffer(graphics_state_.get(),
        shader_program_manager_.get());
    graphics_state_->UpdateVertexData();
  }

  {
    TRACE_EVENT0("cobalt::renderer", "OnscreenRasterize");
    draw_object_manager.ExecuteOnscreenRasterize(graphics_state_.get(),
        shader_program_manager_.get());
  }

  graphics_state_->EndFrame();
}

HardwareRasterizer::HardwareRasterizer(
    backend::GraphicsContext* graphics_context, int skia_atlas_width,
    int skia_atlas_height, int skia_cache_size_in_bytes,
    int scratch_surface_cache_size_in_bytes, int surface_cache_size_in_bytes)
    : impl_(new Impl(graphics_context, skia_atlas_width, skia_atlas_height,
                     skia_cache_size_in_bytes,
                     scratch_surface_cache_size_in_bytes,
                     surface_cache_size_in_bytes)) {}

void HardwareRasterizer::Submit(
    const scoped_refptr<render_tree::Node>& render_tree,
    const scoped_refptr<backend::RenderTarget>& render_target,
    const Options& options) {
  TRACE_EVENT0("cobalt::renderer", "HardwareRasterizer::Submit");
  impl_->Submit(render_tree, render_target, options);
}

render_tree::ResourceProvider* HardwareRasterizer::GetResourceProvider() {
  return impl_->GetResourceProvider();
}

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
