// Copyright 2014 Google Inc. All Rights Reserved.
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

#include "cobalt/renderer/rasterizer/skia/hardware_rasterizer.h"

#include <algorithm>

#include "base/containers/linked_hash_map.h"
#include "base/debug/trace_event.h"
#include "cobalt/renderer/backend/egl/graphics_context.h"
#include "cobalt/renderer/backend/egl/graphics_system.h"
#include "cobalt/renderer/backend/egl/texture.h"
#include "cobalt/renderer/backend/egl/utils.h"
#include "cobalt/renderer/frame_rate_throttler.h"
#include "cobalt/renderer/rasterizer/common/surface_cache.h"
#include "cobalt/renderer/rasterizer/egl/textured_mesh_renderer.h"
#include "cobalt/renderer/rasterizer/skia/cobalt_skia_type_conversions.h"
#include "cobalt/renderer/rasterizer/skia/hardware_mesh.h"
#include "cobalt/renderer/rasterizer/skia/hardware_resource_provider.h"
#include "cobalt/renderer/rasterizer/skia/render_tree_node_visitor.h"
#include "cobalt/renderer/rasterizer/skia/scratch_surface_cache.h"
#include "cobalt/renderer/rasterizer/skia/surface_cache_delegate.h"
#include "cobalt/renderer/rasterizer/skia/vertex_buffer_object.h"
#include "third_party/glm/glm/gtc/matrix_inverse.hpp"
#include "third_party/glm/glm/mat3x3.hpp"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/GrTexture.h"
#include "third_party/skia/include/gpu/SkGrPixelRef.h"
#include "third_party/skia/src/gpu/SkGpuDevice.h"

namespace {
  // Some clients call Submit() multiple times with up to 2 different render
  // targets each frame, so the max must be at least 2 to avoid constantly
  // generating new surfaces.
  static const size_t kMaxSkSurfaceCount = 2;
}

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

class HardwareRasterizer::Impl {
 public:
  Impl(backend::GraphicsContext* graphics_context, int skia_cache_size_in_bytes,
       int scratch_surface_cache_size_in_bytes,
       int surface_cache_size_in_bytes);
  ~Impl();

  void AdvanceFrame();

  void Submit(const scoped_refptr<render_tree::Node>& render_tree,
              const scoped_refptr<backend::RenderTarget>& render_target,
              const Options& options);

  void SubmitOffscreen(const scoped_refptr<render_tree::Node>& render_tree,
                       SkCanvas* canvas);

  render_tree::ResourceProvider* GetResourceProvider();
  GrContext* GetGrContext();

  void MakeCurrent();

 private:
  // Note: We cannot store a SkAutoTUnref<SkSurface> in the map because it is
  // not copyable; so we must manually manage our references when adding /
  // removing SkSurfaces from it.
  typedef base::linked_hash_map<
    backend::RenderTarget*, SkSurface*> SkSurfaceMap;
  class CachedScratchSurfaceHolder
      : public RenderTreeNodeVisitor::ScratchSurface {
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
  scoped_ptr<RenderTreeNodeVisitor::ScratchSurface> CreateScratchSurface(
      const math::Size& size);

  void ResetSkiaState();

  void RenderTextureEGL(const render_tree::ImageNode* image_node,
                        RenderTreeNodeVisitorDrawState* draw_state);
  void RenderTextureWithMeshFilterEGL(
      const render_tree::ImageNode* image_node,
      const render_tree::MapToMeshFilter& mesh_filter,
      RenderTreeNodeVisitorDrawState* draw_state);

  base::ThreadChecker thread_checker_;

  backend::GraphicsContextEGL* graphics_context_;
  scoped_ptr<render_tree::ResourceProvider> resource_provider_;

  SkAutoTUnref<GrContext> gr_context_;

  SkSurfaceMap sk_output_surface_map_;

  base::optional<ScratchSurfaceCache> scratch_surface_cache_;

  base::optional<SurfaceCacheDelegate> surface_cache_delegate_;
  base::optional<common::SurfaceCache> surface_cache_;

  base::optional<egl::TexturedMeshRenderer> textured_mesh_renderer_;

  FrameRateThrottler frame_rate_throttler_;
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

glm::mat4 GetFallbackTextureModelViewProjectionMatrix(
    const SkISize& canvas_size, const SkMatrix& total_matrix,
    const math::RectF& destination_rect) {
  // We define a transformation from GLES normalized device coordinates (e.g.
  // [-1.0, 1.0]) into Skia coordinates (e.g. [0, canvas_size.width()]).  This
  // lets us apply Skia's transform inside of Skia's coordinate space.
  glm::mat3 gl_norm_coords_to_skia_canvas_coords(
      canvas_size.width() * 0.5f, 0, 0, 0, -canvas_size.height() * 0.5f, 0,
      canvas_size.width() * 0.5f, canvas_size.height() * 0.5f, 1);

  // Convert Skia's current transform from the 3x3 row-major Skia matrix to a
  // 4x4 column-major GLSL matrix.  This is in Skia's coordinate system.
  glm::mat3 skia_transform_matrix(
      total_matrix[0], total_matrix[3], total_matrix[6], total_matrix[1],
      total_matrix[4], total_matrix[7], total_matrix[2],
      total_matrix[5], total_matrix[8]);

  // Finally construct a matrix to map from full screen coordinates into the
  // destination rectangle.  This is in Skia's coordinate system.
  glm::mat3 dest_rect_matrix(
      destination_rect.width() / canvas_size.width(), 0, 0, 0,
      destination_rect.height() / canvas_size.height(), 0,
      destination_rect.x(), destination_rect.y(), 1);

  // Since these matrices are applied in LIFO order, read the followin inlined
  // comments in reverse order.
  return glm::mat4(
      // Finally transform back into normalized device coordinates so that
      // GL can digest the results.
      glm::affineInverse(gl_norm_coords_to_skia_canvas_coords) *
      // Apply Skia's transformation matrix to the resulting coordinates.
      skia_transform_matrix *
      // Apply a matrix to transform from a quad that maps to the entire screen
      // into a quad that maps to the destination rectangle.
      dest_rect_matrix *
      // First transform from normalized device coordinates which the VBO
      // referenced by the RenderQuad() function will have its positions defined
      // within (e.g. [-1, 1]).
      gl_norm_coords_to_skia_canvas_coords);
}

}  // namespace

void HardwareRasterizer::Impl::RenderTextureEGL(
    const render_tree::ImageNode* image_node,
    RenderTreeNodeVisitorDrawState* draw_state) {
  HardwareFrontendImage* image =
      base::polymorphic_downcast<HardwareFrontendImage*>(
          image_node->data().source.get());

  const backend::TextureEGL* texture = image->GetTextureEGL();

  // Flush the Skia draw state to ensure that all previously issued Skia calls
  // are rendered so that the following draw command will appear in the correct
  // order.
  draw_state->render_target->flush();

  SkISize canvas_size = draw_state->render_target->getBaseLayerSize();
  GL_CALL(glViewport(0, 0, canvas_size.width(), canvas_size.height()));

  SkIRect canvas_boundsi;
  draw_state->render_target->getClipDeviceBounds(&canvas_boundsi);
  GL_CALL(glScissor(canvas_boundsi.x(), canvas_boundsi.y(),
                    canvas_boundsi.width(), canvas_boundsi.height()));

  if (image->IsOpaque()) {
    GL_CALL(glDisable(GL_BLEND));
  } else {
    GL_CALL(glEnable(GL_BLEND));
  }
  GL_CALL(glDisable(GL_DEPTH_TEST));
  GL_CALL(glDisable(GL_STENCIL_TEST));
  GL_CALL(glEnable(GL_SCISSOR_TEST));

  if (!textured_mesh_renderer_) {
    textured_mesh_renderer_.emplace(graphics_context_);
  }
  math::Rect content_region(image->GetSize());
  if (image->GetContentRegion()) {
    content_region = *image->GetContentRegion();
  }

  // Invoke our TexturedMeshRenderer to actually perform the draw call.
  textured_mesh_renderer_->RenderQuad(
      texture, content_region,
      GetFallbackTextureModelViewProjectionMatrix(
          canvas_size, draw_state->render_target->getTotalMatrix(),
          image_node->data().destination_rect));

  // Let Skia know that we've modified GL state.
  gr_context_->resetContext();
}

void HardwareRasterizer::Impl::RenderTextureWithMeshFilterEGL(
    const render_tree::ImageNode* image_node,
    const render_tree::MapToMeshFilter& mesh_filter,
    RenderTreeNodeVisitorDrawState* draw_state) {
  if (!image_node->data().source) {
    return;
  }

  HardwareFrontendImage* image =
      base::polymorphic_downcast<HardwareFrontendImage*>(
          image_node->data().source.get());

  const backend::TextureEGL* texture = image->GetTextureEGL();

  SkISize canvas_size = draw_state->render_target->getBaseLayerSize();

  // Flush the Skia draw state to ensure that all previously issued Skia calls
  // are rendered so that the following draw command will appear in the correct
  // order.
  draw_state->render_target->flush();

  // We setup our viewport to fill the entire canvas.
  GL_CALL(glViewport(0, 0, canvas_size.width(), canvas_size.height()));
  GL_CALL(glScissor(0, 0, canvas_size.width(), canvas_size.height()));

  if (image->IsOpaque()) {
    GL_CALL(glDisable(GL_BLEND));
  } else {
    GL_CALL(glEnable(GL_BLEND));
  }
  GL_CALL(glDisable(GL_DEPTH_TEST));
  GL_CALL(glDisable(GL_STENCIL_TEST));
  GL_CALL(glEnable(GL_SCISSOR_TEST));
  GL_CALL(glEnable(GL_CULL_FACE));
  GL_CALL(glCullFace(GL_BACK));

  if (!textured_mesh_renderer_) {
    textured_mesh_renderer_.emplace(graphics_context_);
  }
  math::Rect content_region(image->GetSize());
  if (image->GetContentRegion()) {
    content_region = *image->GetContentRegion();
  }

  const VertexBufferObject* left_vbo =
      base::polymorphic_downcast<HardwareMesh*>(mesh_filter.mono_mesh().get())
          ->GetVBO();
  // Invoke out TexturedMeshRenderer to actually perform the draw call.
  textured_mesh_renderer_->RenderVBO(left_vbo->GetHandle(),
                                     left_vbo->GetVertexCount(),
                                     left_vbo->GetDrawMode(), texture,
                                     content_region, draw_state->transform_3d);

  // Let Skia know that we've modified GL state.
  gr_context_->resetContext();
}

HardwareRasterizer::Impl::Impl(backend::GraphicsContext* graphics_context,
                               int skia_cache_size_in_bytes,
                               int scratch_surface_cache_size_in_bytes,
                               int surface_cache_size_in_bytes)
    : graphics_context_(
          base::polymorphic_downcast<backend::GraphicsContextEGL*>(
              graphics_context)) {
  TRACE_EVENT0("cobalt::renderer", "HardwareRasterizer::Impl::Impl()");

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
      base::Bind(&HardwareRasterizer::Impl::CreateSkSurface,
                 base::Unretained(this));

  scratch_surface_cache_.emplace(create_sk_surface_function,
                                 scratch_surface_cache_size_in_bytes);

  // Setup a resource provider for resources to be used with a hardware
  // accelerated Skia rasterizer.
  resource_provider_.reset(
      new HardwareResourceProvider(graphics_context_, gr_context_));

  graphics_context_->ReleaseCurrentContext();

  int max_surface_size = std::max(gr_context_->getMaxRenderTargetSize(),
                                  gr_context_->getMaxTextureSize());
  DLOG(INFO) << "Max renderer surface size: " << max_surface_size;

  if (surface_cache_size_in_bytes > 0) {
    surface_cache_delegate_.emplace(
        create_sk_surface_function,
        math::Size(max_surface_size, max_surface_size));

    surface_cache_.emplace(&surface_cache_delegate_.value(),
                           surface_cache_size_in_bytes);
  }
}

HardwareRasterizer::Impl::~Impl() {
  graphics_context_->MakeCurrent();
  textured_mesh_renderer_ = base::nullopt;

  for (SkSurfaceMap::iterator iter = sk_output_surface_map_.begin();
    iter != sk_output_surface_map_.end(); iter++) {
    iter->second->unref();
  }
  surface_cache_ = base::nullopt;
  surface_cache_delegate_ = base::nullopt;
  scratch_surface_cache_ = base::nullopt;
  gr_context_.reset(NULL);
  graphics_context_->ReleaseCurrentContext();
}

void HardwareRasterizer::Impl::AdvanceFrame() {
  // Update our surface cache to do per-frame calculations such as deciding
  // which render tree nodes are candidates for caching in this upcoming
  // frame.
  if (surface_cache_) {
    surface_cache_->Frame();
  }
}

void HardwareRasterizer::Impl::Submit(
    const scoped_refptr<render_tree::Node>& render_tree,
    const scoped_refptr<backend::RenderTarget>& render_target,
    const Options& options) {
  DCHECK(thread_checker_.CalledOnValidThread());

  scoped_refptr<backend::RenderTargetEGL> render_target_egl(
      base::polymorphic_downcast<backend::RenderTargetEGL*>(
          render_target.get()));

  // Skip rendering if we lost the surface. This can happen just before suspend
  // on Android, so now we're just waiting for the suspend to clean up.
  if (render_target_egl->is_surface_bad()) {
    return;
  }

  backend::GraphicsContextEGL::ScopedMakeCurrent scoped_make_current(
      graphics_context_, render_target_egl);

  // First reset the graphics context state for the pending render tree
  // draw calls, in case we have modified state in between.
  gr_context_->resetContext();

  AdvanceFrame();

  SkSurface* sk_output_surface;
  SkSurfaceMap::iterator iter = sk_output_surface_map_.find(render_target);
  if (iter == sk_output_surface_map_.end()) {
    // Remove the least recently used SkSurface from the map if we exceed the
    // max allowed saved surfaces.
    if (sk_output_surface_map_.size() > kMaxSkSurfaceCount) {
      SkSurfaceMap::iterator iter = sk_output_surface_map_.begin();
      DLOG(WARNING) << "Erasing the SkSurface for RenderTarget " << iter->first
        << " this should not happen often or else it means the surface map is"
        << " probably thrashing.";
      iter->second->unref();
      sk_output_surface_map_.erase(iter);
    }
    // Setup a Skia render target that wraps the passed in Cobalt render target.
    SkAutoTUnref<GrRenderTarget> skia_render_target(
        gr_context_->wrapBackendRenderTarget(
            CobaltRenderTargetToSkiaBackendRenderTargetDesc(
                render_target.get())));

    // Create an SkSurface from the render target so that we can acquire a
    // SkCanvas object from it in Submit().
    sk_output_surface = CreateSkiaRenderTargetSurface(skia_render_target);
    sk_output_surface_map_[render_target] = sk_output_surface;
  } else {
    sk_output_surface = sk_output_surface_map_[render_target];
    // Mark this RenderTarget/SkCanvas pair as the most recently used by
    // popping it and re-adding it.
    sk_output_surface_map_.erase(iter);
    sk_output_surface_map_[render_target] = sk_output_surface;
  }

  // Get a SkCanvas that outputs to our hardware render target.
  SkCanvas* canvas = sk_output_surface->getCanvas();
  canvas->save();

  if (options.flags & Rasterizer::kSubmitFlags_Clear) {
    canvas->clear(SkColorSetARGB(0, 0, 0, 0));
  } else if (options.dirty) {
    // Only a portion of the display is dirty. Reuse the previous frame
    // if possible.
    if (render_target_egl->ContentWasPreservedAfterSwap()) {
      canvas->clipRect(CobaltRectFToSkiaRect(*options.dirty));
    }
  }

  {
    TRACE_EVENT0("cobalt::renderer", "VisitRenderTree");
    // Rasterize the passed in render tree to our hardware render target.
    RenderTreeNodeVisitor::CreateScratchSurfaceFunction
        create_scratch_surface_function =
            base::Bind(&HardwareRasterizer::Impl::CreateScratchSurface,
                       base::Unretained(this));
    RenderTreeNodeVisitor visitor(
        canvas, &create_scratch_surface_function,
        base::Bind(&HardwareRasterizer::Impl::ResetSkiaState,
                   base::Unretained(this)),
        base::Bind(&HardwareRasterizer::Impl::RenderTextureEGL,
                   base::Unretained(this)),
        base::Bind(&HardwareRasterizer::Impl::RenderTextureWithMeshFilterEGL,
                   base::Unretained(this)),
        surface_cache_delegate_ ? &surface_cache_delegate_.value() : NULL,
        surface_cache_ ? &surface_cache_.value() : NULL);
    render_tree->Accept(&visitor);
  }

  {
    TRACE_EVENT0("cobalt::renderer", "Skia Flush");
    canvas->flush();
  }

  frame_rate_throttler_.EndInterval();
  graphics_context_->SwapBuffers(render_target_egl);
  frame_rate_throttler_.BeginInterval();
  canvas->restore();
}

void HardwareRasterizer::Impl::SubmitOffscreen(
    const scoped_refptr<render_tree::Node>& render_tree,
    SkCanvas* canvas) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Caller is expected to reset gr_context as needed.

  {
    TRACE_EVENT0("cobalt::renderer", "VisitRenderTree");
    // Rasterize the passed in render tree to our hardware render target.
    RenderTreeNodeVisitor::CreateScratchSurfaceFunction
        create_scratch_surface_function =
            base::Bind(&HardwareRasterizer::Impl::CreateScratchSurface,
                       base::Unretained(this));
    RenderTreeNodeVisitor visitor(
        canvas, &create_scratch_surface_function,
        base::Bind(&HardwareRasterizer::Impl::ResetSkiaState,
                   base::Unretained(this)),
        base::Bind(&HardwareRasterizer::Impl::RenderTextureEGL,
                   base::Unretained(this)),
        base::Bind(&HardwareRasterizer::Impl::RenderTextureWithMeshFilterEGL,
                   base::Unretained(this)),
        surface_cache_delegate_ ? &surface_cache_delegate_.value() : NULL,
        surface_cache_ ? &surface_cache_.value() : NULL);
    render_tree->Accept(&visitor);
  }
}

render_tree::ResourceProvider* HardwareRasterizer::Impl::GetResourceProvider() {
  return resource_provider_.get();
}

GrContext* HardwareRasterizer::Impl::GetGrContext() {
  return gr_context_.get();
}

void HardwareRasterizer::Impl::MakeCurrent() {
  graphics_context_->MakeCurrent();
}

SkSurface* HardwareRasterizer::Impl::CreateSkSurface(const math::Size& size) {
  TRACE_EVENT2("cobalt::renderer", "HardwareRasterizer::CreateSkSurface()",
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

scoped_ptr<RenderTreeNodeVisitor::ScratchSurface>
HardwareRasterizer::Impl::CreateScratchSurface(const math::Size& size) {
  TRACE_EVENT2("cobalt::renderer", "HardwareRasterizer::CreateScratchImage()",
               "width", size.width(), "height", size.height());

  scoped_ptr<CachedScratchSurfaceHolder> scratch_surface(
      new CachedScratchSurfaceHolder(&scratch_surface_cache_.value(), size));
  if (scratch_surface->GetSurface()) {
    return scratch_surface.PassAs<RenderTreeNodeVisitor::ScratchSurface>();
  } else {
    return scoped_ptr<RenderTreeNodeVisitor::ScratchSurface>();
  }
}

void HardwareRasterizer::Impl::ResetSkiaState() { gr_context_->resetContext(); }

HardwareRasterizer::HardwareRasterizer(
    backend::GraphicsContext* graphics_context, int skia_cache_size_in_bytes,
    int scratch_surface_cache_size_in_bytes, int surface_cache_size_in_bytes)
    : impl_(new Impl(graphics_context, skia_cache_size_in_bytes,
                     scratch_surface_cache_size_in_bytes,
                     surface_cache_size_in_bytes)) {}

HardwareRasterizer::~HardwareRasterizer() {}

void HardwareRasterizer::Submit(
    const scoped_refptr<render_tree::Node>& render_tree,
    const scoped_refptr<backend::RenderTarget>& render_target,
    const Options& options) {
  TRACE_EVENT0("cobalt::renderer", "Rasterizer::Submit()");
  TRACE_EVENT0("cobalt::renderer", "HardwareRasterizer::Submit()");
  impl_->Submit(render_tree, render_target, options);
}

void HardwareRasterizer::SubmitOffscreen(
    const scoped_refptr<render_tree::Node>& render_tree,
    SkCanvas* canvas) {
  TRACE_EVENT0("cobalt::renderer", "HardwareRasterizer::SubmitOffscreen()");
  impl_->SubmitOffscreen(render_tree, canvas);
}

void HardwareRasterizer::AdvanceFrame() {
  impl_->AdvanceFrame();
}

render_tree::ResourceProvider* HardwareRasterizer::GetResourceProvider() {
  return impl_->GetResourceProvider();
}

GrContext* HardwareRasterizer::GetGrContext() {
  return impl_->GetGrContext();
}

void HardwareRasterizer::MakeCurrent() { return impl_->MakeCurrent(); }

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
