// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/renderer/rasterizer/blitter/hardware_rasterizer.h"

#include <string>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/threading/thread_checker.h"
#if defined(ENABLE_DEBUG_CONSOLE)
#include "cobalt/base/console_commands.h"
#endif
#include "cobalt/render_tree/resource_provider_stub.h"
#include "cobalt/renderer/backend/blitter/graphics_context.h"
#include "cobalt/renderer/backend/blitter/render_target.h"
#include "cobalt/renderer/rasterizer/blitter/cached_software_rasterizer.h"
#include "cobalt/renderer/rasterizer/blitter/render_state.h"
#include "cobalt/renderer/rasterizer/blitter/render_tree_node_visitor.h"
#include "cobalt/renderer/rasterizer/blitter/resource_provider.h"
#include "cobalt/renderer/rasterizer/skia/software_rasterizer.h"

#if SB_HAS(BLITTER)

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace blitter {

class HardwareRasterizer::Impl {
 public:
  explicit Impl(backend::GraphicsContext* graphics_context,
                int skia_atlas_width, int skia_atlas_height,
                int scratch_surface_size_in_bytes,
                int software_surface_cache_size_in_bytes,
                bool purge_skia_font_caches_on_destruction);
  ~Impl();

  void Submit(const scoped_refptr<render_tree::Node>& render_tree,
              const scoped_refptr<backend::RenderTarget>& render_target,
              const Options& options);

  void SubmitOffscreenToRenderTarget(
      const scoped_refptr<render_tree::Node>& render_tree,
      const scoped_refptr<backend::RenderTarget>& render_target);

  render_tree::ResourceProvider* GetResourceProvider();

 private:
#if defined(ENABLE_DEBUG_CONSOLE)
  void OnToggleHighlightSoftwareDraws(const std::string& message);
#endif
#if defined(COBALT_RENDER_DIRTY_REGION_ONLY)
  void SetupLastFrameSurface(int width, int height);
#endif  // #if defined(COBALT_RENDER_DIRTY_REGION_ONLY)
  base::ThreadChecker thread_checker_;

  backend::GraphicsContextBlitter* context_;

  scoped_ptr<render_tree::ResourceProvider> resource_provider_;

  int64 submit_count_;

  // We maintain a "final results" surface that mirrors the display buffer.
  // This way, we can rerender only the dirty parts of the screen to this
  // |current_frame_| buffer and then blit that to the display.
  SbBlitterSurface current_frame_;

  ScratchSurfaceCache scratch_surface_cache_;

  CachedSoftwareRasterizer software_surface_cache_;
  LinearGradientCache linear_gradient_cache_;

#if defined(ENABLE_DEBUG_CONSOLE)
  // Debug command to toggle cache highlights to help visualize which nodes
  // are being cached.
  bool toggle_highlight_software_draws_;
  base::ConsoleCommandManager::CommandHandler
      toggle_highlight_software_draws_command_handler_;
#endif
};

HardwareRasterizer::Impl::Impl(backend::GraphicsContext* graphics_context,
                               int skia_atlas_width, int skia_atlas_height,
                               int scratch_surface_size_in_bytes,
                               int software_surface_cache_size_in_bytes,
                               bool purge_skia_font_caches_on_destruction)
    : context_(base::polymorphic_downcast<backend::GraphicsContextBlitter*>(
          graphics_context)),
      submit_count_(0),
      current_frame_(kSbBlitterInvalidSurface),
      scratch_surface_cache_(context_->GetSbBlitterDevice(),
                             context_->GetSbBlitterContext(),
                             scratch_surface_size_in_bytes),
      software_surface_cache_(context_->GetSbBlitterDevice(),
                              context_->GetSbBlitterContext(),
                              software_surface_cache_size_in_bytes,
                              purge_skia_font_caches_on_destruction)
#if defined(ENABLE_DEBUG_CONSOLE)
      ,
      toggle_highlight_software_draws_(false),
      toggle_highlight_software_draws_command_handler_(
          "toggle_highlight_software_draws",
          base::Bind(&HardwareRasterizer::Impl::OnToggleHighlightSoftwareDraws,
                     base::Unretained(this)),
          "Highlights regions where software rasterization is occurring.",
          "Toggles whether all software rasterized elements will appear as a "
          "green rectangle or not.  This can be used to identify where in a "
          "scene software rasterization is occurring.")
#endif  // defined(ENABLE_DEBUG_CONSOLE)
{
  resource_provider_ =
      scoped_ptr<render_tree::ResourceProvider>(new ResourceProvider(
          context_->GetSbBlitterDevice(),
          software_surface_cache_.GetResourceProvider(),
          base::Bind(&HardwareRasterizer::Impl::SubmitOffscreenToRenderTarget,
                     base::Unretained(this))));
}

HardwareRasterizer::Impl::~Impl() {
  if (SbBlitterIsSurfaceValid(current_frame_)) {
    SbBlitterDestroySurface(current_frame_);
  }
}

#if defined(ENABLE_DEBUG_CONSOLE)
void HardwareRasterizer::Impl::OnToggleHighlightSoftwareDraws(
    const std::string& message) {
  UNREFERENCED_PARAMETER(message);
  toggle_highlight_software_draws_ = !toggle_highlight_software_draws_;
}
#endif

void HardwareRasterizer::Impl::Submit(
    const scoped_refptr<render_tree::Node>& render_tree,
    const scoped_refptr<backend::RenderTarget>& render_target,
    const Options& options) {
  TRACE_EVENT0("cobalt::renderer", "Rasterizer::Submit()");
  DCHECK(thread_checker_.CalledOnValidThread());

  int width = render_target->GetSize().width();
  int height = render_target->GetSize().height();

  SbBlitterContext context = context_->GetSbBlitterContext();

  backend::RenderTargetBlitter* render_target_blitter =
      base::polymorphic_downcast<backend::RenderTargetBlitter*>(
          render_target.get());

#if defined(COBALT_RENDER_DIRTY_REGION_ONLY)
  if (!SbBlitterIsSurfaceValid(current_frame_)) {
    SetupLastFrameSurface(width, height);
  }
#endif  // #if defined(COBALT_RENDER_DIRTY_REGION_ONLY)

  SbBlitterRenderTarget visitor_render_target = kSbBlitterInvalidRenderTarget;
  if (SbBlitterIsSurfaceValid(current_frame_)) {
    visitor_render_target = SbBlitterGetRenderTargetFromSurface(current_frame_);
  } else {
    visitor_render_target = render_target_blitter->GetSbRenderTarget();
  }
  CHECK(SbBlitterSetRenderTarget(context, visitor_render_target));

  software_surface_cache_.OnStartNewFrame();

  // Clear the background before proceeding if the clear option is set.
  // We also clear if this is one of the first 3 submits.  This is for security
  // purposes, so that despite the Blitter API implementation, we ensure that
  // if the output buffer is not completely rendered to, data from a previous
  // process cannot leak in.
  bool cleared = false;
  if (options.flags & Rasterizer::kSubmitFlags_Clear || submit_count_ < 3) {
    cleared = true;
    CHECK(SbBlitterSetBlending(context, false));
    CHECK(SbBlitterSetColor(context, SbBlitterColorFromRGBA(0, 0, 0, 0)));
    CHECK(SbBlitterFillRect(context, SbBlitterMakeRect(0, 0, width, height)));
  }

  {
    TRACE_EVENT0("cobalt::renderer", "VisitRenderTree");

    // Visit the render tree with our Blitter API visitor.
    BoundsStack start_bounds(context_->GetSbBlitterContext(),
                             math::Rect(render_target_blitter->GetSize()));

    if (SbBlitterIsSurfaceValid(current_frame_) && options.dirty && !cleared) {
      // If a dirty rectangle was specified, limit our redrawing to within it.
      start_bounds.Push(*options.dirty);
    }

    RenderState initial_render_state(visitor_render_target, Transform(),
                                     start_bounds);

#if defined(ENABLE_DEBUG_CONSOLE)
    initial_render_state.highlight_software_draws =
        toggle_highlight_software_draws_;
#endif  // defined(ENABLE_DEBUG_CONSOLE)
    RenderTreeNodeVisitor visitor(
        context_->GetSbBlitterDevice(), context_->GetSbBlitterContext(),
        initial_render_state, &scratch_surface_cache_,
        &software_surface_cache_, &linear_gradient_cache_);
    render_tree->Accept(&visitor);
  }

  if (SbBlitterIsSurfaceValid(current_frame_)) {
    // Finally flip the surface to make visible the rendered results.
    CHECK(SbBlitterSetRenderTarget(context,
                                   render_target_blitter->GetSbRenderTarget()));
    CHECK(SbBlitterSetBlending(context, false));
    CHECK(SbBlitterSetModulateBlitsWithColor(context, false));
    CHECK(SbBlitterBlitRectToRect(context, current_frame_,
                                  SbBlitterMakeRect(0, 0, width, height),
                                  SbBlitterMakeRect(0, 0, width, height)));
  }

  CHECK(SbBlitterFlushContext(context));
  render_target_blitter->Flip();

  ++submit_count_;
}

void HardwareRasterizer::Impl::SubmitOffscreenToRenderTarget(
    const scoped_refptr<render_tree::Node>& render_tree,
    const scoped_refptr<backend::RenderTarget>& render_target) {
  TRACE_EVENT0("cobalt::renderer",
               "Rasterizer::SubmitOffscreenToRenderTarget()");
  DCHECK(thread_checker_.CalledOnValidThread());

  SbBlitterContext context = context_->GetSbBlitterContext();

  backend::RenderTargetBlitter* render_target_blitter =
      base::polymorphic_downcast<backend::RenderTargetBlitter*>(
          render_target.get());

  SbBlitterRenderTarget visitor_render_target =
      render_target_blitter->GetSbRenderTarget();
  CHECK(SbBlitterSetRenderTarget(context, visitor_render_target));

  math::Size size(render_target_blitter->GetSize());
  BoundsStack start_bounds(context, math::Rect(size));

  RenderState initial_render_state(visitor_render_target, Transform(),
                                   start_bounds);

  CHECK(SbBlitterSetBlending(context, false));
  CHECK(SbBlitterSetColor(context, SbBlitterColorFromRGBA(0, 0, 0, 0)));
  CHECK(SbBlitterFillRect(
      context, SbBlitterMakeRect(0, 0, size.width(), size.height())));

  RenderTreeNodeVisitor visitor(context_->GetSbBlitterDevice(), context,
                                initial_render_state, &scratch_surface_cache_,
                                &software_surface_cache_,
                                &linear_gradient_cache_);
  render_tree->Accept(&visitor);

  CHECK(SbBlitterFlushContext(context));
}

render_tree::ResourceProvider* HardwareRasterizer::Impl::GetResourceProvider() {
  return resource_provider_.get();
}

#if defined(COBALT_RENDER_DIRTY_REGION_ONLY)
void HardwareRasterizer::Impl::SetupLastFrameSurface(int width, int height) {
  current_frame_ =
      SbBlitterCreateRenderTargetSurface(context_->GetSbBlitterDevice(), width,
                                         height, kSbBlitterSurfaceFormatRGBA8);
}
#endif  // #if defined(COBALT_RENDER_DIRTY_REGION_ONLY)

HardwareRasterizer::HardwareRasterizer(
    backend::GraphicsContext* graphics_context, int skia_atlas_width,
    int skia_atlas_height, int scratch_surface_size_in_bytes,
    int software_surface_cache_size_in_bytes,
    bool purge_skia_font_caches_on_destruction)
    : impl_(new Impl(graphics_context, skia_atlas_width, skia_atlas_height,
                     scratch_surface_size_in_bytes,
                     software_surface_cache_size_in_bytes,
                     purge_skia_font_caches_on_destruction)) {}

HardwareRasterizer::~HardwareRasterizer() {}

void HardwareRasterizer::Submit(
    const scoped_refptr<render_tree::Node>& render_tree,
    const scoped_refptr<backend::RenderTarget>& render_target,
    const Options& options) {
  return impl_->Submit(render_tree, render_target, options);
}

render_tree::ResourceProvider* HardwareRasterizer::GetResourceProvider() {
  return impl_->GetResourceProvider();
}

}  // namespace blitter
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

#endif  // #if SB_HAS(BLITTER)
