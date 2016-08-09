/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#include "cobalt/renderer/rasterizer/skia/hardware_rasterizer.h"

#include "base/debug/trace_event.h"
#include "cobalt/renderer/backend/egl/graphics_context.h"
#include "cobalt/renderer/rasterizer/common/surface_cache.h"
#include "cobalt/renderer/rasterizer/skia/cobalt_skia_type_conversions.h"
#include "cobalt/renderer/rasterizer/skia/hardware_resource_provider.h"
#include "cobalt/renderer/rasterizer/skia/render_tree_node_visitor.h"
#include "cobalt/renderer/rasterizer/skia/scratch_surface_cache.h"
#include "cobalt/renderer/rasterizer/skia/surface_cache_delegate.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/GrTexture.h"
#include "third_party/skia/include/gpu/SkGrPixelRef.h"
#include "third_party/skia/src/gpu/SkGpuDevice.h"

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

class SkiaHardwareRasterizer::Impl {
 public:
  Impl(backend::GraphicsContext* graphics_context, int skia_cache_size_in_bytes,
       int scratch_surface_cache_size_in_bytes,
       int surface_cache_size_in_bytes);
  ~Impl();

  void Submit(const scoped_refptr<render_tree::Node>& render_tree,
              const scoped_refptr<backend::RenderTarget>& render_target,
              int options);

  render_tree::ResourceProvider* GetResourceProvider();

 private:
  class CachedScratchSurfaceHolder
      : public SkiaRenderTreeNodeVisitor::ScratchSurface {
   public:
    CachedScratchSurfaceHolder(ScratchSurfaceCache* cache,
                               const math::Size& size)
        : cached_scratch_surface_(cache, size) {}
    SkSurface* GetSurface() OVERRIDE {
      return cached_scratch_surface_.GetSurface();
    }

   private:
    CachedScratchSurface cached_scratch_surface_;
  };

  SkSurface* CreateSkSurface(const math::Size& size);
  scoped_ptr<SkiaRenderTreeNodeVisitor::ScratchSurface> CreateScratchSurface(
      const math::Size& size);

  base::ThreadChecker thread_checker_;

  backend::GraphicsContextEGL* graphics_context_;
  scoped_ptr<render_tree::ResourceProvider> resource_provider_;

  SkAutoTUnref<GrContext> gr_context_;

  SkAutoTUnref<SkSurface> sk_output_surface_;

  base::optional<ScratchSurfaceCache> scratch_surface_cache_;

  base::optional<SurfaceCacheDelegate> surface_cache_delegate_;
  base::optional<common::SurfaceCache> surface_cache_;
};

namespace {

SkSurfaceProps GetRenderTargetSurfaceProps() {
  return SkSurfaceProps(SkSurfaceProps::kUseDistanceFieldFonts_Flag,
                        SkSurfaceProps::kLegacyFontHost_InitType);
}

SkSurface* CreateSkiaRenderTargetSurface(GrRenderTarget* render_target) {
  SkSurfaceProps surface_props = GetRenderTargetSurfaceProps();
  return SkSurface::NewRenderTargetDirect(render_target, &surface_props);
}

// Takes meta-data from a Cobalt RenderTarget object and uses it to fill out
// a Skia backend render target descriptor.  Additionally, it also references
// the actual render target object as well so that Skia can then recover
// the Cobalt render target object.
GrBackendRenderTargetDesc CobaltRenderTargetToSkiaBackendRenderTargetDesc(
    cobalt::renderer::backend::RenderTarget* cobalt_render_target) {
  const math::Size& size = cobalt_render_target->GetSize();

  GrBackendRenderTargetDesc skia_desc;
  skia_desc.fWidth = size.width();
  skia_desc.fHeight = size.height();
  skia_desc.fConfig = kRGBA_8888_GrPixelConfig;
  skia_desc.fOrigin = kBottomLeft_GrSurfaceOrigin;
  skia_desc.fSampleCnt = 0;
  skia_desc.fStencilBits = 0;
  skia_desc.fRenderTargetHandle =
      static_cast<GrBackendObject>(cobalt_render_target->GetPlatformHandle());

  return skia_desc;
}

}  // namespace

SkiaHardwareRasterizer::Impl::Impl(backend::GraphicsContext* graphics_context,
                                   int skia_cache_size_in_bytes,
                                   int scratch_surface_cache_size_in_bytes,
                                   int surface_cache_size_in_bytes)
    : graphics_context_(
          base::polymorphic_downcast<backend::GraphicsContextEGL*>(
              graphics_context)) {
  TRACE_EVENT0("cobalt::renderer", "SkiaHardwareRasterizer::Impl::Impl()");

  DLOG(INFO) << "skia_cache_size_in_bytes: " << skia_cache_size_in_bytes;
  DLOG(INFO) << "scratch_surface_cache_size_in_bytes: "
             << scratch_surface_cache_size_in_bytes;
  DLOG(INFO) << "surface_cache_size_in_bytes: " << surface_cache_size_in_bytes;

  graphics_context_->MakeCurrent();
  // Create a GrContext object that wraps the passed in Cobalt GraphicsContext
  // object.
  gr_context_.reset(GrContext::Create(
      kCobalt_GrBackend,
      reinterpret_cast<GrBackendContext>(graphics_context_)));
  DCHECK(gr_context_);
  // The GrContext manages a resource cache internally using GrResourceCache
  // which by default caches 96MB of resources.  This is used for helping with
  // rendering shadow effects, gradient effects, and software rendered paths.
  // As we have our own cache for most resources, set it to a much smaller value
  // so Skia doesn't use too much GPU memory.
  const int kSkiaCacheMaxResources = 128;
  gr_context_->setResourceCacheLimits(kSkiaCacheMaxResources,
                                      skia_cache_size_in_bytes);

  base::Callback<SkSurface*(const math::Size&)> create_sk_surface_function =
      base::Bind(&SkiaHardwareRasterizer::Impl::CreateSkSurface,
                 base::Unretained(this));

  scratch_surface_cache_.emplace(create_sk_surface_function,
                                 scratch_surface_cache_size_in_bytes);

  // Setup a resource provider for resources to be used with a hardware
  // accelerated Skia rasterizer.
  resource_provider_.reset(
      new SkiaHardwareResourceProvider(graphics_context_, gr_context_));
  graphics_context_->ReleaseCurrentContext();

  if (surface_cache_size_in_bytes > 0) {
    surface_cache_delegate_.emplace(
        create_sk_surface_function,
        math::Size(gr_context_->getMaxTextureSize(),
                   gr_context_->getMaxTextureSize()));

    surface_cache_.emplace(&surface_cache_delegate_.value(),
                           surface_cache_size_in_bytes);
  }
}

SkiaHardwareRasterizer::Impl::~Impl() {
  graphics_context_->MakeCurrent();
  sk_output_surface_.reset(NULL);
  surface_cache_ = base::nullopt;
  surface_cache_delegate_ = base::nullopt;
  scratch_surface_cache_ = base::nullopt;
  gr_context_.reset(NULL);
  graphics_context_->ReleaseCurrentContext();
}

void SkiaHardwareRasterizer::Impl::Submit(
    const scoped_refptr<render_tree::Node>& render_tree,
    const scoped_refptr<backend::RenderTarget>& render_target, int options) {
  DCHECK(thread_checker_.CalledOnValidThread());

  backend::RenderTargetEGL* render_target_egl =
      base::polymorphic_downcast<backend::RenderTargetEGL*>(
          render_target.get());

  backend::GraphicsContextEGL::ScopedMakeCurrent scoped_make_current(
      graphics_context_, render_target_egl);

  // First reset the graphics context state for the pending render tree
  // draw calls, in case we have modified state in between.
  gr_context_->resetContext();

  // Update our surface cache to do per-frame calculations such as deciding
  // which render tree nodes are candidates for caching in this upcoming
  // frame.
  if (surface_cache_) {
    surface_cache_->Frame();
  }

  if (!sk_output_surface_) {
    // Setup a Skia render target that wraps the passed in Cobalt render target.
    SkAutoTUnref<GrRenderTarget> skia_render_target(
        gr_context_->wrapBackendRenderTarget(
            CobaltRenderTargetToSkiaBackendRenderTargetDesc(
                render_target.get())));

    // Create an SkSurface from the render target so that we can acquire a
    // SkCanvas object from it in Submit().
    sk_output_surface_.reset(CreateSkiaRenderTargetSurface(skia_render_target));
  }

  // Get a SkCanvas that outputs to our hardware render target.
  SkCanvas* canvas = sk_output_surface_->getCanvas();

  if (options & Rasterizer::kSubmitOptions_Clear) {
    canvas->clear(SkColorSetARGB(0, 0, 0, 0));
  }

  {
    TRACE_EVENT0("cobalt::renderer", "VisitRenderTree");
    // Rasterize the passed in render tree to our hardware render target.
    SkiaRenderTreeNodeVisitor::CreateScratchSurfaceFunction
        create_scratch_surface_function =
            base::Bind(&SkiaHardwareRasterizer::Impl::CreateScratchSurface,
                       base::Unretained(this));
    SkiaRenderTreeNodeVisitor visitor(
        canvas, &create_scratch_surface_function,
        surface_cache_delegate_ ? &surface_cache_delegate_.value() : NULL,
        surface_cache_ ? &surface_cache_.value() : NULL);
    render_tree->Accept(&visitor);
  }

  {
    TRACE_EVENT0("cobalt::renderer", "Skia Flush");
    canvas->flush();
  }

  graphics_context_->SwapBuffers(render_target_egl);
}

render_tree::ResourceProvider*
SkiaHardwareRasterizer::Impl::GetResourceProvider() {
  return resource_provider_.get();
}

SkSurface* SkiaHardwareRasterizer::Impl::CreateSkSurface(
    const math::Size& size) {
  TRACE_EVENT2("cobalt::renderer", "SkiaHardwareRasterizer::CreateSkSurface()",
               "width", size.width(), "height", size.height());

  // Create a texture of the specified size.  Then convert it to a render
  // target and then convert that to a SkSurface which we return.
  GrTextureDesc target_surface_desc;
  target_surface_desc.fFlags =
      kRenderTarget_GrTextureFlagBit | kNoStencil_GrTextureFlagBit;
  target_surface_desc.fOrigin = kBottomLeft_GrSurfaceOrigin;
  target_surface_desc.fWidth = size.width();
  target_surface_desc.fHeight = size.height();
  target_surface_desc.fConfig = kRGBA_8888_GrPixelConfig;
  target_surface_desc.fSampleCnt = 0;

  SkAutoTUnref<GrTexture> skia_texture(
      gr_context_->createUncachedTexture(target_surface_desc, NULL, 0));
  if (!skia_texture) {
    // If we failed at creating a texture, try again using a different texture
    // format.
    target_surface_desc.fConfig = kBGRA_8888_GrPixelConfig;
    skia_texture.reset(
        gr_context_->createUncachedTexture(target_surface_desc, NULL, 0));
  }
  if (!skia_texture) {
    return NULL;
  }

  GrRenderTarget* skia_render_target = skia_texture->asRenderTarget();
  DCHECK(skia_render_target);

  SkSurfaceProps surface_props = GetRenderTargetSurfaceProps();
  return SkSurface::NewRenderTargetDirect(skia_render_target, &surface_props);
}

scoped_ptr<SkiaRenderTreeNodeVisitor::ScratchSurface>
SkiaHardwareRasterizer::Impl::CreateScratchSurface(const math::Size& size) {
  TRACE_EVENT2("cobalt::renderer",
               "SkiaHardwareRasterizer::CreateScratchImage()", "width",
               size.width(), "height", size.height());

  scoped_ptr<CachedScratchSurfaceHolder> scratch_surface(
      new CachedScratchSurfaceHolder(&scratch_surface_cache_.value(), size));
  if (scratch_surface->GetSurface()) {
    return scratch_surface.PassAs<SkiaRenderTreeNodeVisitor::ScratchSurface>();
  } else {
    return scoped_ptr<SkiaRenderTreeNodeVisitor::ScratchSurface>();
  }
}

SkiaHardwareRasterizer::SkiaHardwareRasterizer(
    backend::GraphicsContext* graphics_context, int skia_cache_size_in_bytes,
    int scratch_surface_cache_size_in_bytes, int surface_cache_size_in_bytes)
    : impl_(new Impl(graphics_context, skia_cache_size_in_bytes,
                     scratch_surface_cache_size_in_bytes,
                     surface_cache_size_in_bytes)) {}

SkiaHardwareRasterizer::~SkiaHardwareRasterizer() {}

void SkiaHardwareRasterizer::Submit(
    const scoped_refptr<render_tree::Node>& render_tree,
    const scoped_refptr<backend::RenderTarget>& render_target, int options) {
  TRACE_EVENT0("cobalt::renderer", "Rasterizer::Submit()");
  TRACE_EVENT0("cobalt::renderer", "SkiaHardwareRasterizer::Submit()");
  impl_->Submit(render_tree, render_target, options);
}

render_tree::ResourceProvider* SkiaHardwareRasterizer::GetResourceProvider() {
  return impl_->GetResourceProvider();
}

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
