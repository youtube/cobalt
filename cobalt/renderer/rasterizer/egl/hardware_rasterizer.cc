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
#include <vector>

#include "base/debug/trace_event.h"
#include "base/threading/thread_checker.h"
#include "cobalt/renderer/backend/egl/framebuffer.h"
#include "cobalt/renderer/backend/egl/graphics_context.h"
#include "cobalt/renderer/backend/egl/graphics_system.h"
#include "cobalt/renderer/backend/egl/texture.h"
#include "cobalt/renderer/backend/egl/utils.h"
#include "cobalt/renderer/frame_rate_throttler.h"
#include "cobalt/renderer/rasterizer/egl/draw_object_manager.h"
#include "cobalt/renderer/rasterizer/egl/graphics_state.h"
#include "cobalt/renderer/rasterizer/egl/render_tree_node_visitor.h"
#include "cobalt/renderer/rasterizer/egl/shader_program_manager.h"
#include "cobalt/renderer/rasterizer/skia/hardware_rasterizer.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/GrRenderTarget.h"
#include "third_party/skia/include/gpu/GrTypes.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

class HardwareRasterizer::Impl {
 public:
  explicit Impl(backend::GraphicsContext* graphics_context,
                int skia_cache_size_in_bytes,
                int scratch_surface_cache_size_in_bytes,
                int surface_cache_size_in_bytes);
  ~Impl();

  void Submit(const scoped_refptr<render_tree::Node>& render_tree,
              const scoped_refptr<backend::RenderTarget>& render_target,
              const Options& options);

  backend::TextureEGL* SubmitToFallbackRasterizer(
      const scoped_refptr<render_tree::Node>& render_tree,
      const math::Size& viewport_size);

  render_tree::ResourceProvider* GetResourceProvider() {
    return fallback_rasterizer_->GetResourceProvider();
  }

 private:
  GrContext* GetGrContext() {
    return fallback_rasterizer_->GetGrContext();
  }

  scoped_ptr<skia::HardwareRasterizer> fallback_rasterizer_;
  scoped_ptr<GraphicsState> graphics_state_;
  scoped_ptr<ShaderProgramManager> shader_program_manager_;

  backend::GraphicsContextEGL* graphics_context_;
  FrameRateThrottler frame_rate_throttler_;
  base::ThreadChecker thread_checker_;

  static const size_t kMinOffscreenTargets = 3;
  struct OffscreenTarget {
    scoped_ptr<backend::FramebufferEGL> framebuffer;
    SkAutoTUnref<SkSurface> skia_surface;
  };
  typedef std::vector<OffscreenTarget*> OffscreenTargetList;
  OffscreenTargetList used_offscreen_targets_;
  OffscreenTargetList unused_offscreen_targets_;
};

HardwareRasterizer::Impl::Impl(
    backend::GraphicsContext* graphics_context,
    int skia_cache_size_in_bytes,
    int scratch_surface_cache_size_in_bytes,
    int surface_cache_size_in_bytes)
    : fallback_rasterizer_(new skia::HardwareRasterizer(
          graphics_context,
          skia_cache_size_in_bytes,
          scratch_surface_cache_size_in_bytes,
          surface_cache_size_in_bytes)),
      graphics_context_(
          base::polymorphic_downcast<backend::GraphicsContextEGL*>(
              graphics_context)) {
  backend::GraphicsContextEGL::ScopedMakeCurrent scoped_make_current(
      graphics_context_);

  graphics_state_.reset(new GraphicsState());
  shader_program_manager_.reset(new ShaderProgramManager());
}

HardwareRasterizer::Impl::~Impl() {
  backend::GraphicsContextEGL::ScopedMakeCurrent scoped_make_current(
      graphics_context_);

  GL_CALL(glFinish());
  while (!unused_offscreen_targets_.empty()) {
    delete unused_offscreen_targets_.back();
    unused_offscreen_targets_.pop_back();
  }

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
  graphics_state_->Scissor(0, 0, target_size.width(), target_size.height());

  {
    DrawObjectManager draw_object_manager;
    RenderTreeNodeVisitor::FallbackRasterizeFunction fallback_rasterize =
        base::Bind(&HardwareRasterizer::Impl::SubmitToFallbackRasterizer,
                   base::Unretained(this));
    RenderTreeNodeVisitor visitor(graphics_state_.get(),
                                  &draw_object_manager,
                                  &fallback_rasterize);

    {
      TRACE_EVENT0("cobalt::renderer", "VisitRenderTree");
      render_tree->Accept(&visitor);
    }

    graphics_state_->BeginFrame();

    {
      TRACE_EVENT0("cobalt::renderer", "UpdateVertexBuffer");
      draw_object_manager.ExecuteUpdateVertexBuffer(graphics_state_.get(),
          shader_program_manager_.get());
      graphics_state_->UpdateVertexData();
    }

    {
      TRACE_EVENT0("cobalt::renderer", "RasterizeOffscreen");
      draw_object_manager.ExecuteRasterizeOffscreen(graphics_state_.get(),
          shader_program_manager_.get());
    }

    graphics_state_->Clear();

    {
      TRACE_EVENT0("cobalt::renderer", "RasterizeNormal");
      draw_object_manager.ExecuteRasterizeNormal(graphics_state_.get(),
          shader_program_manager_.get());
    }

    graphics_state_->EndFrame();
  }

  // Update the offscreen target cache. Keep an extra frame's worth of targets
  // around.
  while (unused_offscreen_targets_.size() > kMinOffscreenTargets &&
         unused_offscreen_targets_.size() > used_offscreen_targets_.size()) {
    delete unused_offscreen_targets_.back();
    unused_offscreen_targets_.pop_back();
  }

  // Add most recently used offscreen targets to the front.
  unused_offscreen_targets_.insert(unused_offscreen_targets_.begin(),
                                   used_offscreen_targets_.begin(),
                                   used_offscreen_targets_.end());
  used_offscreen_targets_.clear();

  frame_rate_throttler_.EndInterval();
  graphics_context_->SwapBuffers(render_target_egl);
  frame_rate_throttler_.BeginInterval();
}

backend::TextureEGL* HardwareRasterizer::Impl::SubmitToFallbackRasterizer(
    const scoped_refptr<render_tree::Node>& render_tree,
    const math::Size& viewport_size) {
  TRACE_EVENT0("cobalt::renderer", "SubmitToFallbackRasterizer");

  // Get an offscreen target for rendering.
  OffscreenTarget* offscreen_target = NULL;
  for (OffscreenTargetList::iterator iter = unused_offscreen_targets_.begin();
       iter != unused_offscreen_targets_.end(); ++iter) {
    if ((*iter)->framebuffer->GetSize() == viewport_size) {
      offscreen_target = *iter;
      unused_offscreen_targets_.erase(iter);
      break;
    }
  }

  if (offscreen_target == NULL) {
    offscreen_target = new OffscreenTarget;

    // Create a new framebuffer.
    offscreen_target->framebuffer.reset(new backend::FramebufferEGL(
        graphics_context_, viewport_size, GL_RGBA, GL_NONE));

    // Wrap the framebuffer as a skia surface.
    GrBackendRenderTargetDesc skia_desc;
    skia_desc.fWidth = viewport_size.width();
    skia_desc.fHeight = viewport_size.height();
    skia_desc.fConfig = kRGBA_8888_GrPixelConfig;
    skia_desc.fOrigin = kBottomLeft_GrSurfaceOrigin;
    skia_desc.fSampleCnt = 0;
    skia_desc.fStencilBits = 0;
    skia_desc.fRenderTargetHandle = offscreen_target->framebuffer->gl_handle();

    SkAutoTUnref<GrRenderTarget> skia_render_target(
        GetGrContext()->wrapBackendRenderTarget(skia_desc));
    SkSurfaceProps skia_surface_props(
        SkSurfaceProps::kUseDistanceFieldFonts_Flag,
        SkSurfaceProps::kLegacyFontHost_InitType);
    offscreen_target->skia_surface.reset(SkSurface::NewRenderTargetDirect(
        skia_render_target, &skia_surface_props));
  }
  used_offscreen_targets_.push_back(offscreen_target);

  backend::FramebufferEGL* framebuffer = offscreen_target->framebuffer.get();
  SkCanvas* canvas = offscreen_target->skia_surface->getCanvas();

  GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->gl_handle()));
  canvas->save();
  fallback_rasterizer_->SubmitOffscreen(render_tree, canvas);
  canvas->restore();
  GL_CALL(glBindFramebuffer(GL_FRAMEBUFFER, 0));

  return framebuffer->GetColorTexture();
}

HardwareRasterizer::HardwareRasterizer(
    backend::GraphicsContext* graphics_context,
    int skia_cache_size_in_bytes,
    int scratch_surface_cache_size_in_bytes,
    int surface_cache_size_in_bytes)
    : impl_(new Impl(graphics_context,
                     skia_cache_size_in_bytes,
                     scratch_surface_cache_size_in_bytes,
                     surface_cache_size_in_bytes)) {
}

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
