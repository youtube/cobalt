// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/configuration.h"
#if SB_API_VERSION >= 12 || SB_HAS(GLES2)

#include "cobalt/renderer/rasterizer/egl/hardware_rasterizer.h"

#include <memory>

#include "base/threading/thread_checker.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/render_tree/filter_node.h"
#include "cobalt/renderer/backend/egl/framebuffer_render_target.h"
#include "cobalt/renderer/backend/egl/graphics_context.h"
#include "cobalt/renderer/backend/egl/graphics_system.h"
#include "cobalt/renderer/backend/egl/texture.h"
#include "cobalt/renderer/backend/egl/utils.h"
#include "cobalt/renderer/egl_and_gles.h"
#include "cobalt/renderer/rasterizer/egl/draw_object_manager.h"
#include "cobalt/renderer/rasterizer/egl/graphics_state.h"
#include "cobalt/renderer/rasterizer/egl/offscreen_target_manager.h"
#include "cobalt/renderer/rasterizer/egl/render_tree_node_visitor.h"
#include "cobalt/renderer/rasterizer/egl/shader_program_manager.h"
#include "cobalt/renderer/rasterizer/skia/cobalt_skia_type_conversions.h"
#include "cobalt/renderer/rasterizer/skia/gl_format_conversions.h"
#include "cobalt/renderer/rasterizer/skia/hardware_rasterizer.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace egl {

class HardwareRasterizer::Impl {
 public:
  Impl(backend::GraphicsContext* graphics_context, int skia_atlas_width,
       int skia_atlas_height, int skia_cache_size_in_bytes,
       int scratch_surface_cache_size_in_bytes,
       int offscreen_target_cache_size_in_bytes,
       bool purge_skia_font_caches_on_destruction,
       bool force_deterministic_rendering);
  ~Impl();

  void Submit(const scoped_refptr<render_tree::Node>& render_tree,
              const scoped_refptr<backend::RenderTarget>& render_target,
              const Options& options);

  void SubmitToFallbackRasterizer(
      const scoped_refptr<render_tree::Node>& render_tree,
      SkCanvas* fallback_render_target, const math::Matrix3F& transform,
      const math::RectF& scissor, float opacity, uint32_t rasterize_flags);

  render_tree::ResourceProvider* GetResourceProvider() {
    return fallback_rasterizer_->GetResourceProvider();
  }

  int64_t GetFallbackRasterizeCount() { return fallback_rasterize_count_; }

  void ResetGraphicsStateCache() {
    graphics_state_->SetDirty();
    GetFallbackContext()->resetContext();
  }

  void MakeCurrent() { graphics_context_->MakeCurrent(); }
  void ReleaseContext() { graphics_context_->ReleaseCurrentContext(); }

 private:
  GrContext* GetFallbackContext() {
    return fallback_rasterizer_->GetGrContext();
  }

  void ResetFallbackContextDuringFrame();
  void FlushFallbackOffscreenDraws();

  void RasterizeTree(const scoped_refptr<render_tree::Node>& render_tree,
                     backend::RenderTargetEGL* render_target,
                     const math::Rect& content_rect, bool clear_first);

  sk_sp<SkSurface> CreateFallbackSurface(
      bool force_deterministic_rendering,
      const backend::RenderTarget* render_target);

  std::unique_ptr<skia::HardwareRasterizer> fallback_rasterizer_;
  std::unique_ptr<GraphicsState> graphics_state_;
  std::unique_ptr<ShaderProgramManager> shader_program_manager_;
  std::unique_ptr<OffscreenTargetManager> offscreen_target_manager_;

  int64_t fallback_rasterize_count_;

  backend::GraphicsContextEGL* graphics_context_;
  THREAD_CHECKER(thread_checker_);
};

HardwareRasterizer::Impl::Impl(backend::GraphicsContext* graphics_context,
                               int skia_atlas_width, int skia_atlas_height,
                               int skia_cache_size_in_bytes,
                               int scratch_surface_cache_size_in_bytes,
                               int offscreen_target_cache_size_in_bytes,
                               bool purge_skia_font_caches_on_destruction,
                               bool force_deterministic_rendering)
    : fallback_rasterizer_(new skia::HardwareRasterizer(
          graphics_context, skia_atlas_width, skia_atlas_height,
          skia_cache_size_in_bytes, scratch_surface_cache_size_in_bytes,
          purge_skia_font_caches_on_destruction,
          force_deterministic_rendering)),
      fallback_rasterize_count_(0),
      graphics_context_(
          base::polymorphic_downcast<backend::GraphicsContextEGL*>(
              graphics_context)) {
  backend::GraphicsContextEGL::ScopedMakeCurrent scoped_make_current(
      graphics_context_);
  graphics_state_.reset(new GraphicsState());
  shader_program_manager_.reset(new ShaderProgramManager());
  offscreen_target_manager_.reset(new OffscreenTargetManager(
      graphics_context_,
      base::Bind(&HardwareRasterizer::Impl::CreateFallbackSurface,
                 base::Unretained(this), force_deterministic_rendering),
      offscreen_target_cache_size_in_bytes));
  if (force_deterministic_rendering) {
    offscreen_target_manager_->SetCacheErrorThreshold(0.0f);
  }
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
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  backend::RenderTargetEGL* render_target_egl =
      base::polymorphic_downcast<backend::RenderTargetEGL*>(
          render_target.get());

  // Skip rendering if we lost the surface. This can happen just before suspend
  // on Android, so now we're just waiting for the suspend to clean up.
  if (render_target_egl->is_surface_bad()) {
    return;
  }

  backend::GraphicsContextEGL::ScopedMakeCurrent scoped_make_current(
      graphics_context_, render_target_egl);

  // Update only the dirty pixels if the render target contents are preserved
  // between frames.
  bool clear_first = options.flags & Rasterizer::kSubmitFlags_Clear;
  math::Rect content_rect(render_target->GetSize());
  if (!clear_first && options.dirty &&
      render_target_egl->ContentWasPreservedAfterSwap()) {
    content_rect = *options.dirty;
  }

  offscreen_target_manager_->Update(render_target->GetSize());

  RasterizeTree(render_tree, render_target_egl, content_rect, clear_first);

  graphics_context_->SwapBuffers(render_target_egl);

  // Reset the fallback context in case it is used between frames (e.g.
  // to initialize images).
  GetFallbackContext()->resetContext();
}

void HardwareRasterizer::Impl::SubmitToFallbackRasterizer(
    const scoped_refptr<render_tree::Node>& render_tree,
    SkCanvas* fallback_render_target, const math::Matrix3F& transform,
    const math::RectF& scissor, float opacity, uint32_t rasterize_flags) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  TRACE_EVENT0("cobalt::renderer", "SubmitToFallbackRasterizer");

  if (!scissor.IsExpressibleAsRect()) {
    DLOG(WARNING) << "Invalid scissor of " << scissor.ToString()
                  << " passed into SubmitToFallbackRasterizer.";
    return;
  }

  // Use skia to rasterize to the allocated offscreen target.
  fallback_render_target->save();
  fallback_render_target->clipRect(SkRect::MakeXYWH(
      scissor.x(), scissor.y(), scissor.width(), scissor.height()));
  fallback_render_target->concat(skia::CobaltMatrixToSkia(transform));

  if ((rasterize_flags & RenderTreeNodeVisitor::kFallbackShouldClear) != 0) {
    fallback_render_target->clear(SK_ColorTRANSPARENT);
  }

  if (opacity < 1.0f) {
    scoped_refptr<render_tree::Node> opacity_node = new render_tree::FilterNode(
        render_tree::OpacityFilter(opacity), render_tree);
    fallback_rasterizer_->SubmitOffscreen(opacity_node, fallback_render_target);
  } else {
    fallback_rasterizer_->SubmitOffscreen(render_tree, fallback_render_target);
  }

  if ((rasterize_flags & RenderTreeNodeVisitor::kFallbackShouldFlush) != 0) {
    fallback_render_target->flush();
  }

  fallback_render_target->restore();
}

void HardwareRasterizer::Impl::FlushFallbackOffscreenDraws() {
  TRACE_EVENT0("cobalt::renderer", "Skia Flush");
  offscreen_target_manager_->Flush();
}

void HardwareRasterizer::Impl::ResetFallbackContextDuringFrame() {
  // Perform a minimal reset of the fallback context. Only need to invalidate
  // states that this rasterizer pollutes.
  uint32_t untouched_states =
      kMSAAEnable_GrGLBackendState | kStencil_GrGLBackendState |
      kPixelStore_GrGLBackendState | kFixedFunction_GrGLBackendState |
      kPathRendering_GrGLBackendState;

  GetFallbackContext()->resetContext(~untouched_states & kAll_GrBackendState);
}

void HardwareRasterizer::Impl::RasterizeTree(
    const scoped_refptr<render_tree::Node>& render_tree,
    backend::RenderTargetEGL* render_target, const math::Rect& content_rect,
    bool clear_first) {
  DrawObjectManager draw_object_manager(
      base::Bind(&HardwareRasterizer::Impl::ResetFallbackContextDuringFrame,
                 base::Unretained(this)),
      base::Bind(&HardwareRasterizer::Impl::FlushFallbackOffscreenDraws,
                 base::Unretained(this)));
  RenderTreeNodeVisitor visitor(
      graphics_state_.get(), &draw_object_manager,
      offscreen_target_manager_.get(),
      base::Bind(&HardwareRasterizer::Impl::SubmitToFallbackRasterizer,
                 base::Unretained(this)),
      fallback_rasterizer_->GetCachedCanvas(render_target), render_target,
      content_rect);

  // Traverse the render tree to populate the draw object manager.
  {
    TRACE_EVENT0("cobalt::renderer", "VisitRenderTree");
    render_tree->Accept(&visitor);
  }

  fallback_rasterize_count_ += visitor.GetFallbackRasterizeCount();

  graphics_state_->BeginFrame();

  // Rasterize to offscreen targets using skia.
  {
    TRACE_EVENT0("cobalt::renderer", "OffscreenRasterize");

    // Ensure the skia context is fully reset.
    GetFallbackContext()->resetContext();
    draw_object_manager.ExecuteOffscreenRasterize(
        graphics_state_.get(), shader_program_manager_.get());
  }

  // Clear the dirty region of the render target.
  graphics_state_->BindFramebuffer(render_target);
  graphics_state_->Viewport(0, 0, render_target->GetSize().width(),
                            render_target->GetSize().height());
  graphics_state_->Scissor(content_rect.x(), content_rect.y(),
                           content_rect.width(), content_rect.height());
  if (clear_first) {
    graphics_state_->Clear();
  }

  {
    TRACE_EVENT0("cobalt::renderer", "OnscreenRasterize");
    draw_object_manager.ExecuteOnscreenRasterize(graphics_state_.get(),
                                                 shader_program_manager_.get());
  }

  graphics_context_->ResetCurrentSurface();
  graphics_state_->EndFrame();
}

sk_sp<SkSurface> HardwareRasterizer::Impl::CreateFallbackSurface(
    bool force_deterministic_rendering,
    const backend::RenderTarget* render_target) {
  // Wrap the given render target in a new skia surface.
  GrGLFramebufferInfo info;
  info.fFBOID = render_target->GetPlatformHandle();
  info.fFormat = skia::ConvertBaseGLFormatToSizedInternalFormat(GL_RGBA);
  GrBackendRenderTarget skia_render_target(render_target->GetSize().width(),
                                           render_target->GetSize().height(), 0,
                                           0, info);

  uint32_t flags = 0;
  SkSurfaceProps skia_surface_props(flags,
                                    SkSurfaceProps::kLegacyFontHost_InitType);
  return SkSurface::MakeFromBackendRenderTarget(
      GetFallbackContext(), skia_render_target, kBottomLeft_GrSurfaceOrigin,
      kRGBA_8888_SkColorType, nullptr, &skia_surface_props);
}

HardwareRasterizer::HardwareRasterizer(
    backend::GraphicsContext* graphics_context, int skia_atlas_width,
    int skia_atlas_height, int skia_cache_size_in_bytes,
    int scratch_surface_cache_size_in_bytes,
    int offscreen_target_cache_size_in_bytes,
    bool purge_skia_font_caches_on_destruction,
    bool force_deterministic_rendering)
    : impl_(new Impl(graphics_context, skia_atlas_width, skia_atlas_height,
                     skia_cache_size_in_bytes,
                     scratch_surface_cache_size_in_bytes,
                     offscreen_target_cache_size_in_bytes,
                     purge_skia_font_caches_on_destruction,
                     force_deterministic_rendering)) {}

HardwareRasterizer::~HardwareRasterizer() {}

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

int64_t HardwareRasterizer::GetFallbackRasterizeCount() {
  return impl_->GetFallbackRasterizeCount();
}

void HardwareRasterizer::ResetGraphicsStateCache() {
  impl_->ResetGraphicsStateCache();
}

void HardwareRasterizer::MakeCurrent() { return impl_->MakeCurrent(); }

void HardwareRasterizer::ReleaseContext() { return impl_->ReleaseContext(); }

}  // namespace egl
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // SB_API_VERSION >= 12 || SB_HAS(GLES2)
