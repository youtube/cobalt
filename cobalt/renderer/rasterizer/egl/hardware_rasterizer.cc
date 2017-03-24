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
#include "cobalt/math/size.h"
#include "cobalt/renderer/backend/egl/framebuffer.h"
#include "cobalt/renderer/backend/egl/graphics_context.h"
#include "cobalt/renderer/backend/egl/graphics_system.h"
#include "cobalt/renderer/backend/egl/texture.h"
#include "cobalt/renderer/backend/egl/utils.h"
#include "cobalt/renderer/frame_rate_throttler.h"
#include "cobalt/renderer/rasterizer/egl/draw_object_manager.h"
#include "cobalt/renderer/rasterizer/egl/graphics_state.h"
#include "cobalt/renderer/rasterizer/egl/rect_allocator.h"
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

namespace {

int32_t NextPowerOf2(int32_t num) {
  // Return the smallest power of 2 that is greater than or equal to num.
  // This flips on all bits <= num, then num+1 will be the next power of 2.
  --num;
  num |= num >> 1;
  num |= num >> 2;
  num |= num >> 4;
  num |= num >> 8;
  num |= num >> 16;
  return num + 1;
}

bool IsPowerOf2(int32_t num) {
  return (num & (num - 1)) == 0;
}

}  // namespace

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

  void SubmitToFallbackRasterizer(
      const scoped_refptr<render_tree::Node>& render_tree,
      const math::RectF& viewport,
      const backend::TextureEGL** out_texture,
      math::Matrix3F* out_texcoord_transform);

  render_tree::ResourceProvider* GetResourceProvider() {
    return fallback_rasterizer_->GetResourceProvider();
  }

 private:
  // Use an atlas for offscreen targets.
  struct OffscreenAtlas {
    explicit OffscreenAtlas(const math::Size& size) : allocator(size) {}
    RectAllocator allocator;
    scoped_ptr<backend::FramebufferEGL> framebuffer;
    SkAutoTUnref<SkSurface> skia_surface;
  };

  GrContext* GetFallbackContext() {
    return fallback_rasterizer_->GetGrContext();
  }

  void RasterizeTree(const scoped_refptr<render_tree::Node>& render_tree);

  void AllocateOffscreenTarget(const math::SizeF& size,
      OffscreenAtlas** out_atlas, math::RectF* out_target_rect);

  OffscreenAtlas* AddOffscreenAtlas(const math::Size& size);

  scoped_ptr<skia::HardwareRasterizer> fallback_rasterizer_;
  scoped_ptr<GraphicsState> graphics_state_;
  scoped_ptr<ShaderProgramManager> shader_program_manager_;

  backend::GraphicsContextEGL* graphics_context_;
  FrameRateThrottler frame_rate_throttler_;
  base::ThreadChecker thread_checker_;

  typedef ScopedVector<OffscreenAtlas> OffscreenAtlasList;
  OffscreenAtlasList offscreen_atlases_;

  // Size of the smallest offscreen target atlas that can hold all offscreen
  // targets requested this frame.
  math::Size offscreen_atlas_size_;

  // Align offscreen targets to a particular size to more efficiently use the
  // offscreen target atlas. Use a power of 2 for the alignment so that a bit
  // mask can be used for the alignment calculation.
  math::Size offscreen_target_size_mask_;
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
              graphics_context)),
      offscreen_atlas_size_(0, 0),
      offscreen_target_size_mask_(0, 0) {
  backend::GraphicsContextEGL::ScopedMakeCurrent scoped_make_current(
      graphics_context_);

  graphics_state_.reset(new GraphicsState());
  shader_program_manager_.reset(new ShaderProgramManager());
}

HardwareRasterizer::Impl::~Impl() {
  backend::GraphicsContextEGL::ScopedMakeCurrent scoped_make_current(
      graphics_context_);

  GL_CALL(glFinish());
  offscreen_atlases_.clear();

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

  // Set initial characteristics for offscreen target handling.
  if (offscreen_atlas_size_.IsEmpty()) {
    if (target_size.width() >= 64 && target_size.height() >= 64) {
      offscreen_atlas_size_.SetSize(
          NextPowerOf2(target_size.width() / 16),
          NextPowerOf2(target_size.height() / 16));
      offscreen_target_size_mask_.SetSize(
          NextPowerOf2(target_size.width() / 64) - 1,
          NextPowerOf2(target_size.height() / 64) - 1);
    } else {
      offscreen_atlas_size_.SetSize(16, 16);
      offscreen_target_size_mask_.SetSize(0, 0);
    }
  }

  // Keep only the largest offscreen target atlas.
  while (offscreen_atlases_.size() > 1) {
    offscreen_atlases_.erase(offscreen_atlases_.begin());
  }

  if (!offscreen_atlases_.empty()) {
    offscreen_atlases_.back()->allocator.Reset();
    offscreen_atlases_.back()->skia_surface->getCanvas()->clear(
        SK_ColorTRANSPARENT);
  }

  RasterizeTree(render_tree);

  frame_rate_throttler_.EndInterval();
  graphics_context_->SwapBuffers(render_target_egl);
  frame_rate_throttler_.BeginInterval();
}

void HardwareRasterizer::Impl::SubmitToFallbackRasterizer(
    const scoped_refptr<render_tree::Node>& render_tree,
    const math::RectF& viewport,
    const backend::TextureEGL** out_texture,
    math::Matrix3F* out_texcoord_transform) {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("cobalt::renderer", "SubmitToFallbackRasterizer");

  math::RectF target_rect(0, 0, 0, 0);
  OffscreenAtlas* atlas = NULL;
  AllocateOffscreenTarget(viewport.size(), &atlas, &target_rect);

  backend::FramebufferEGL* framebuffer = atlas->framebuffer.get();
  SkCanvas* canvas = atlas->skia_surface->getCanvas();

  // Use skia to rasterize to the allocated offscreen target.
  canvas->save();
  canvas->clipRect(SkRect::MakeXYWH(
      target_rect.x(), target_rect.y(),
      viewport.width() + 0.5f, viewport.height() + 0.5f));
  canvas->translate(viewport.x() + target_rect.x(),
                    viewport.y() + target_rect.y());
  fallback_rasterizer_->SubmitOffscreen(render_tree, canvas);
  canvas->restore();

  float scale_x = 1.0f / framebuffer->GetSize().width();
  float scale_y = 1.0f / framebuffer->GetSize().height();
  *out_texcoord_transform = math::Matrix3F::FromValues(
      viewport.width() * scale_x, 0, target_rect.x() * scale_x,
      0, viewport.height() * scale_y, target_rect.y() * scale_y,
      0, 0, 1);
  *out_texture = framebuffer->GetColorTexture();
}

void HardwareRasterizer::Impl::RasterizeTree(
    const scoped_refptr<render_tree::Node>& render_tree) {
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

    // Reset the skia graphics context since the egl rasterizer dirtied it.
    GetFallbackContext()->resetContext();
    draw_object_manager.ExecuteOffscreenRasterize(graphics_state_.get(),
        shader_program_manager_.get());

    {
      TRACE_EVENT0("cobalt::renderer", "Skia Flush");
      for (OffscreenAtlasList::iterator iter = offscreen_atlases_.begin();
           iter != offscreen_atlases_.end(); ++iter) {
        (*iter)->skia_surface->getCanvas()->flush();
      }
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

void HardwareRasterizer::Impl::AllocateOffscreenTarget(
    const math::SizeF& size,
    OffscreenAtlas** out_atlas, math::RectF* out_target_rect) {
  // Get an offscreen target for rendering. Align up the requested target size
  // to improve usage of the atlas (since more requests will have the same
  // aligned width or height).
  DCHECK(IsPowerOf2(offscreen_target_size_mask_.width() + 1));
  DCHECK(IsPowerOf2(offscreen_target_size_mask_.height() + 1));
  math::Size target_size(
      (static_cast<int>(size.width() + 0.5f) +
          offscreen_target_size_mask_.width()) &
          ~offscreen_target_size_mask_.width(),
      (static_cast<int>(size.height() + 0.5f) +
          offscreen_target_size_mask_.height()) &
          ~offscreen_target_size_mask_.height());
  math::Rect target_rect(0, 0, 0, 0);
  OffscreenAtlas* atlas = NULL;

  // See if there's room in the most recently created offscreen target atlas.
  // Don't search any other atlases since we want to quickly find an offscreen
  // atlas size large enough to hold all offscreen targets needed in a frame.
  if (!offscreen_atlases_.empty()) {
    atlas = offscreen_atlases_.back();
    target_rect = atlas->allocator.Allocate(target_size);
  }

  if (target_rect.IsEmpty()) {
    // Create a new offscreen atlas, bigger than the previous, so that
    // eventually only one offscreen atlas is needed per frame.
    bool grew = false;
    if (offscreen_atlas_size_.width() < target_size.width()) {
      offscreen_atlas_size_.set_width(NextPowerOf2(target_size.width()));
      grew = true;
    }
    if (offscreen_atlas_size_.height() < target_size.height()) {
      offscreen_atlas_size_.set_height(NextPowerOf2(target_size.height()));
      grew = true;
    }
    if (!grew) {
      // Grow the offscreen atlas while keeping it square-ish.
      if (offscreen_atlas_size_.width() <= offscreen_atlas_size_.height()) {
        offscreen_atlas_size_.set_width(offscreen_atlas_size_.width() * 2);
      } else {
        offscreen_atlas_size_.set_height(offscreen_atlas_size_.height() * 2);
      }
    }

    atlas = AddOffscreenAtlas(offscreen_atlas_size_);
    target_rect = atlas->allocator.Allocate(target_size);
  }
  DCHECK(!target_rect.IsEmpty());

  *out_atlas = atlas;
  *out_target_rect = target_rect;
}

HardwareRasterizer::Impl::OffscreenAtlas*
    HardwareRasterizer::Impl::AddOffscreenAtlas(const math::Size& size) {
  OffscreenAtlas* atlas = new OffscreenAtlas(offscreen_atlas_size_);
  offscreen_atlases_.push_back(atlas);

  // Create a new framebuffer.
  atlas->framebuffer.reset(new backend::FramebufferEGL(
      graphics_context_, offscreen_atlas_size_, GL_RGBA, GL_NONE));

  // Wrap the framebuffer as a skia surface.
  GrBackendRenderTargetDesc skia_desc;
  skia_desc.fWidth = offscreen_atlas_size_.width();
  skia_desc.fHeight = offscreen_atlas_size_.height();
  skia_desc.fConfig = kRGBA_8888_GrPixelConfig;
  skia_desc.fOrigin = kTopLeft_GrSurfaceOrigin;
  skia_desc.fSampleCnt = 0;
  skia_desc.fStencilBits = 0;
  skia_desc.fRenderTargetHandle = atlas->framebuffer->gl_handle();

  SkAutoTUnref<GrRenderTarget> skia_render_target(
      GetFallbackContext()->wrapBackendRenderTarget(skia_desc));
  SkSurfaceProps skia_surface_props(
      SkSurfaceProps::kUseDistanceFieldFonts_Flag,
      SkSurfaceProps::kLegacyFontHost_InitType);
  atlas->skia_surface.reset(SkSurface::NewRenderTargetDirect(
      skia_render_target, &skia_surface_props));

  atlas->skia_surface->getCanvas()->clear(SK_ColorTRANSPARENT);

  return atlas;
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
