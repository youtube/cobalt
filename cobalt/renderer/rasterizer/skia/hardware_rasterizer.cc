// Copyright 2014 The Cobalt Authors. All Rights Reserved.
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
#include <memory>
#include <unordered_map>

#include "base/trace_event/trace_event.h"
#include "cobalt/renderer/backend/egl/framebuffer_render_target.h"
#include "cobalt/renderer/backend/egl/graphics_context.h"
#include "cobalt/renderer/backend/egl/graphics_system.h"
#include "cobalt/renderer/backend/egl/texture.h"
#include "cobalt/renderer/backend/egl/utils.h"
#include "cobalt/renderer/rasterizer/egl/textured_mesh_renderer.h"  // nogncheck
#include "cobalt/renderer/rasterizer/skia/cobalt_skia_type_conversions.h"
#include "cobalt/renderer/rasterizer/skia/gl_format_conversions.h"
#include "cobalt/renderer/rasterizer/skia/hardware_mesh.h"
#include "cobalt/renderer/rasterizer/skia/hardware_resource_provider.h"
#include "cobalt/renderer/rasterizer/skia/render_tree_node_visitor.h"
#include "cobalt/renderer/rasterizer/skia/scratch_surface_cache.h"
#include "cobalt/renderer/rasterizer/skia/vertex_buffer_object.h"
#include "starboard/configuration.h"
#include "third_party/glm/glm/gtc/matrix_inverse.hpp"
#include "third_party/glm/glm/gtx/transform.hpp"
#include "third_party/glm/glm/mat3x3.hpp"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#ifdef USE_SKIA_NEXT
#include "third_party/skia/include/gpu/GrDirectContext.h"
#else
#include "third_party/skia/include/gpu/GrContext.h"
#endif
#include "third_party/skia/include/gpu/GrContextOptions.h"
#ifndef USE_SKIA_NEXT
#include "third_party/skia/include/gpu/GrTexture.h"
#endif
#include "third_party/skia/include/gpu/gl/GrGLInterface.h"
#include "third_party/skia/src/gpu/GrRenderTarget.h"
#include "third_party/skia/src/gpu/GrResourceProvider.h"

namespace {
// Some clients call Submit() multiple times with up to 2 different render
// targets each frame, so the max must be at least 2 to avoid constantly
// generating new surfaces.
static const size_t kMaxSkSurfaceCount = 2;
}  // namespace

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace skia {

class HardwareRasterizer::Impl {
 public:
  Impl(backend::GraphicsContext* graphics_context, int skia_atlas_width,
       int skia_atlas_height, int skia_cache_size_in_bytes,
       int scratch_surface_cache_size_in_bytes,
       bool purge_skia_font_caches_on_destruction,
       bool force_deterministic_rendering);
  ~Impl();

  void Submit(const scoped_refptr<render_tree::Node>& render_tree,
              const scoped_refptr<backend::RenderTarget>& render_target,
              const Options& options);

  void SubmitOffscreen(const scoped_refptr<render_tree::Node>& render_tree,
                       SkCanvas* canvas);

  void SubmitOffscreenToRenderTarget(
      const scoped_refptr<render_tree::Node>& render_tree,
      const scoped_refptr<backend::RenderTarget>& render_target);

  SkCanvas* GetCanvasFromRenderTarget(
      const scoped_refptr<backend::RenderTarget>& render_target);

  render_tree::ResourceProvider* GetResourceProvider();
  GrContext* GetGrContext();

  void MakeCurrent();
  void ReleaseContext();

 private:
  typedef std::unordered_map<int32_t, sk_sp<SkSurface>> SkSurfaceMap;
  class CachedScratchSurfaceHolder
      : public RenderTreeNodeVisitor::ScratchSurface {
   public:
    CachedScratchSurfaceHolder(ScratchSurfaceCache* cache,
                               const math::Size& size)
        : cached_scratch_surface_(cache, size) {}
    SkSurface* GetSurface() override {
      return cached_scratch_surface_.GetSurface();
    }

   private:
    CachedScratchSurface cached_scratch_surface_;
  };

  sk_sp<SkSurface> CreateSkSurface(const math::Size& size);
  std::unique_ptr<RenderTreeNodeVisitor::ScratchSurface> CreateScratchSurface(
      const math::Size& size);

  void RasterizeRenderTreeToCanvas(
      const scoped_refptr<render_tree::Node>& render_tree, SkCanvas* canvas,
      GrSurfaceOrigin origin);

  void ResetSkiaState();

  void RenderTextureEGL(const render_tree::ImageNode* image_node,
                        RenderTreeNodeVisitorDrawState* draw_state);
  void RenderTextureWithMeshFilterEGL(
      const render_tree::ImageNode* image_node,
      const render_tree::MapToMeshFilter& mesh_filter,
      RenderTreeNodeVisitorDrawState* draw_state);
  scoped_refptr<render_tree::Image> ConvertRenderTreeToImage(
      const scoped_refptr<render_tree::Node>& root);

  THREAD_CHECKER(thread_checker_);

  backend::GraphicsContextEGL* graphics_context_;
  std::unique_ptr<render_tree::ResourceProvider> resource_provider_;

  sk_sp<GrContext> gr_context_;

  SkSurfaceMap sk_output_surface_map_;

  base::Optional<ScratchSurfaceCache> scratch_surface_cache_;

  base::Optional<egl::TexturedMeshRenderer> textured_mesh_renderer_;

  // Valid only for the duration of a call to RasterizeRenderTreeToCanvas().
  // Useful for directing textured_mesh_renderer_ on whether to flip its y-axis
  // or not since Skia does not let us pull that information out of the
  // SkCanvas object (which Skia would internally use to get this information).
  base::Optional<GrSurfaceOrigin> current_surface_origin_;

  // If true, rasterizer will eschew performance optimizations in favor of
  // ensuring that each rasterization is pixel-wise deterministic, given
  // the same render tree.
  bool force_deterministic_rendering_;
};

namespace {

SkSurfaceProps GetRenderTargetSurfaceProps(bool force_deterministic_rendering) {
  uint32_t flags = 0;
#ifdef USE_SKIA_NEXT
  return SkSurfaceProps(flags, kUnknown_SkPixelGeometry);
#else
  return SkSurfaceProps(flags, SkSurfaceProps::kLegacyFontHost_InitType);
#endif
}

// Takes meta-data from a Cobalt RenderTarget object and uses it to fill out
// a Skia backend render target.  Additionally, it also references the actual
// render target object as well so that Skia can then recover the Cobalt render
// target object.
GrBackendRenderTarget CobaltRenderTargetToSkiaBackendRenderTarget(
    const cobalt::renderer::backend::RenderTarget& cobalt_render_target) {
  const math::Size& size = cobalt_render_target.GetSize();

  GrGLFramebufferInfo info;
  info.fFBOID = cobalt_render_target.GetPlatformHandle();
  info.fFormat = ConvertBaseGLFormatToSizedInternalFormat(GL_RGBA);
  GrBackendRenderTarget skia_render_target(size.width(), size.height(), 0, 0,
                                           info);

  return skia_render_target;
}

glm::mat4 ModelViewMatrixSurfaceOriginAdjustment(GrSurfaceOrigin origin) {
  if (origin == kTopLeft_GrSurfaceOrigin) {
    return glm::scale(glm::vec3(1.0f, -1.0f, 1.0f));
  } else {
    return glm::mat4(1.0f);
  }
}

glm::mat4 GetFallbackTextureModelViewProjectionMatrix(
    GrSurfaceOrigin origin, const SkISize& canvas_size,
    const SkMatrix& total_matrix, const math::RectF& destination_rect) {
  // We define a transformation from GLES normalized device coordinates (e.g.
  // [-1.0, 1.0]) into Skia coordinates (e.g. [0, canvas_size.width()]).  This
  // lets us apply Skia's transform inside of Skia's coordinate space.
  glm::mat4 gl_norm_coords_to_skia_canvas_coords(
      canvas_size.width() * 0.5f, 0, 0, 0, 0, -canvas_size.height() * 0.5f, 0,
      0, 0, 0, 1, 0, canvas_size.width() * 0.5f, canvas_size.height() * 0.5f, 0,
      1);

  // Convert Skia's current transform from the 3x3 row-major Skia matrix to a
  // 4x4 column-major GLSL matrix.  This is in Skia's coordinate system.
  glm::mat4 skia_transform_matrix(
      total_matrix[0], total_matrix[3], 0, total_matrix[6], total_matrix[1],
      total_matrix[4], 0, total_matrix[7], 0, 0, 1, 0, total_matrix[2],
      total_matrix[5], 0, total_matrix[8]);

  // Finally construct a matrix to map from full screen coordinates into the
  // destination rectangle.  This is in Skia's coordinate system.
  glm::mat4 dest_rect_matrix(
      destination_rect.width() / canvas_size.width(), 0, 0, 0, 0,
      destination_rect.height() / canvas_size.height(), 0, 0, 0, 0, 1, 0,
      destination_rect.x(), destination_rect.y(), 0, 1);

  // Since these matrices are applied in LIFO order, read the following inlined
  // comments in reverse order.
  glm::mat4 result =
      // Flip the y axis depending on the destination surface's origin.
      ModelViewMatrixSurfaceOriginAdjustment(origin) *
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
      gl_norm_coords_to_skia_canvas_coords;

  return result;
}

glm::mat4 ScaleMatrixFor360VideoAdjustment(const SkISize& canvas_size,
                                           const SkIRect& canvas_boundsi) {
  glm::mat4 scale_matrix(
      static_cast<float>(canvas_boundsi.width()) / canvas_size.width(), 0, 0, 0,
      0, static_cast<float>(canvas_boundsi.height()) / canvas_size.height(), 0,
      0, 0, 0, 1, 0,
      (canvas_boundsi.width() - canvas_size.width()) /
          static_cast<float>(canvas_size.width()),
      (canvas_size.width() - canvas_boundsi.width()) /
          static_cast<float>(canvas_size.width()),
      0, 1);

  return scale_matrix;
}

// Accommodate for the fact that for some image formats, like UYVY, our texture
// pixel width is actually half the size specified because there are two Y
// values in each pixel.
math::RectF AdjustContentRegionForImageType(
    const base::Optional<AlternateRgbaFormat>& alternate_rgba_format,
    const math::RectF& content_region) {
  if (!alternate_rgba_format) {
    return content_region;
  }

  switch (*alternate_rgba_format) {
    case AlternateRgbaFormat_UYVY: {
      math::RectF adjusted_content_region = content_region;
      adjusted_content_region.set_width(content_region.width() / 2);
      return adjusted_content_region;
    } break;
    default: {
      NOTREACHED();
      return content_region;
    }
  }
}

// For stereoscopic video, the actual video is split (either horizontally or
// vertically) in two, one video for the left eye and one for the right eye.
// This function will adjust the content region rectangle to match only the
// left eye's video region, since we are ultimately presenting to a monoscopic
// display.
math::RectF AdjustContentRegionForStereoMode(
    render_tree::StereoMode stereo_mode, const math::RectF& content_region) {
  switch (stereo_mode) {
    case render_tree::kLeftRight: {
      // Use the left half (left eye) of the video only.
      math::RectF adjusted_content_region(content_region);
      adjusted_content_region.set_width(content_region.width() / 2);
      return adjusted_content_region;
    }

    case render_tree::kTopBottom: {
      // Use the top half (left eye) of the video only.
      math::RectF adjusted_content_region(content_region);
      adjusted_content_region.set_height(content_region.height() / 2);
      return adjusted_content_region;
    }

    case render_tree::kMono:
    case render_tree::kLeftRightUnadjustedTextureCoords:
      // No modifications needed here, pass content region through unchanged.
      return content_region;
  }

  NOTREACHED();
  return content_region;
}

egl::TexturedMeshRenderer::Image::Texture GetTextureFromHardwareFrontendImage(
    const math::Matrix3F& local_transform, HardwareFrontendImage* image,
    render_tree::StereoMode stereo_mode) {
  egl::TexturedMeshRenderer::Image::Texture result;

  if (image->GetContentRegion()) {
    result.content_region = *image->GetContentRegion();
  } else {
    // If no content region is explicitly provided, we take this to mean that
    // the image was created from render_tree::ImageData in which case image
    // data is defined to be specified top-to-bottom, and so we must flip the
    // y-axis before passing it on to a GL renderer.
    math::Size image_size(image->GetSize());
    result.content_region = math::Rect::RoundFromRectF(math::RectF(
        -image_size.width() * local_transform.Get(0, 2) /
            local_transform.Get(0, 0),
        image_size.height() *
            (1 + local_transform.Get(1, 2) / local_transform.Get(1, 1)),
        image_size.width() / local_transform.Get(0, 0),
        -image_size.height() / local_transform.Get(1, 1)));
  }

  result.content_region = AdjustContentRegionForStereoMode(
      stereo_mode, AdjustContentRegionForImageType(
                       image->alternate_rgba_format(), result.content_region));

  result.texture = image->GetTextureEGL();

  return result;
}

inline void SetImageTextures(egl::TexturedMeshRenderer::Image& result,
                             unsigned int textures_num,
                             const math::Matrix3F& local_transform,
                             HardwareMultiPlaneImage* hardware_image,
                             render_tree::StereoMode stereo_mode) {
  for (auto i = 0; i < textures_num; i++) {
    result.textures[i] = GetTextureFromHardwareFrontendImage(
        local_transform, hardware_image->GetHardwareFrontendImage(i).get(),
        stereo_mode);
  }
}

egl::TexturedMeshRenderer::Image SkiaImageToTexturedMeshRendererImage(
    const math::Matrix3F& local_transform, Image* image,
    render_tree::StereoMode stereo_mode) {
  egl::TexturedMeshRenderer::Image result;

  if (image->GetTypeId() == base::GetTypeId<SinglePlaneImage>()) {
    HardwareFrontendImage* hardware_image =
        base::polymorphic_downcast<HardwareFrontendImage*>(image);

    if (!hardware_image->alternate_rgba_format()) {
      result.type = egl::TexturedMeshRenderer::Image::RGBA;
    } else {
      switch (*hardware_image->alternate_rgba_format()) {
        case AlternateRgbaFormat_UYVY: {
          result.type = egl::TexturedMeshRenderer::Image::YUV_UYVY_422_BT709;
        } break;
        default: {
          NOTREACHED();
        }
      }
    }

    result.textures[0] = GetTextureFromHardwareFrontendImage(
        local_transform, hardware_image, stereo_mode);
  } else if (image->GetTypeId() == base::GetTypeId<MultiPlaneImage>()) {
    HardwareMultiPlaneImage* hardware_image =
        base::polymorphic_downcast<HardwareMultiPlaneImage*>(image);
    if (hardware_image->GetFormat() ==
        render_tree::kMultiPlaneImageFormatYUV3PlaneBT601FullRange) {
      result.type =
          egl::TexturedMeshRenderer::Image::YUV_3PLANE_BT601_FULL_RANGE;
      SetImageTextures(result, 3, local_transform, hardware_image, stereo_mode);
    } else if (hardware_image->GetFormat() ==
               render_tree::kMultiPlaneImageFormatYUV2PlaneBT709) {
      result.type = egl::TexturedMeshRenderer::Image::YUV_2PLANE_BT709;
      SetImageTextures(result, 2, local_transform, hardware_image, stereo_mode);
    } else if (hardware_image->GetFormat() ==
               render_tree::kMultiPlaneImageFormatYUV3PlaneBT709) {
      result.type = egl::TexturedMeshRenderer::Image::YUV_3PLANE_BT709;
      SetImageTextures(result, 3, local_transform, hardware_image, stereo_mode);
    } else if (hardware_image->GetFormat() ==
               render_tree::kMultiPlaneImageFormatYUV3Plane10BitBT2020) {
      result.type = egl::TexturedMeshRenderer::Image::YUV_3PLANE_10BIT_BT2020;
      SetImageTextures(result, 3, local_transform, hardware_image, stereo_mode);
    } else if (hardware_image->GetFormat() ==
               render_tree::
                   kMultiPlaneImageFormatYUV3Plane10BitCompactedBT2020) {
      result.type =
          egl::TexturedMeshRenderer::Image::YUV_3PLANE_10BIT_COMPACT_BT2020;
      SetImageTextures(result, 3, local_transform, hardware_image, stereo_mode);
    }
  } else {
    NOTREACHED();
  }

  return result;
}

enum FaceOrientation {
  FaceOrientation_Ccw,
  FaceOrientation_Cw,
};

void SetupGLStateForImageRender(Image* image,
                                FaceOrientation face_orientation) {
  if (image->IsOpaque()) {
    GL_CALL(glDisable(GL_BLEND));
  } else {
    GL_CALL(glEnable(GL_BLEND));
  }
  GL_CALL(glDisable(GL_DEPTH_TEST));
  GL_CALL(glDisable(GL_STENCIL_TEST));
  GL_CALL(glEnable(GL_SCISSOR_TEST));
  GL_CALL(glCullFace(GL_BACK));
  GL_CALL(glFrontFace(GL_CCW));
  if (face_orientation == FaceOrientation_Ccw) {
    GL_CALL(glEnable(GL_CULL_FACE));
  } else {
    // Unfortunately, some GLES implementations (like software Mesa) have a
    // problem with flipping glCullFrace() from GL_BACK to GL_FRONT, they seem
    // to ignore it.  We need to render back faces though if the face
    // orientation is flipped, so the only compatible solution is to disable
    // back-face culling.
    GL_CALL(glDisable(GL_CULL_FACE));
  }
}

bool SetupGLTextureParameters(const egl::TexturedMeshRenderer::Image& image,
                              uint32 texture_wrap_s, uint32 texture_wrap_t) {
  for (int i = 0; i < image.num_textures(); ++i) {
    const backend::TextureEGL* texture = image.textures[i].texture;
    if (!texture) {
      return false;
    }
    GL_CALL(glBindTexture(texture->GetTarget(), texture->gl_handle()));
    GL_CALL(glTexParameteri(texture->GetTarget(), GL_TEXTURE_MAG_FILTER,
                            GL_LINEAR));
    GL_CALL(glTexParameteri(texture->GetTarget(), GL_TEXTURE_MIN_FILTER,
                            GL_LINEAR));
    GL_CALL(glTexParameteri(texture->GetTarget(), GL_TEXTURE_WRAP_S,
                            texture_wrap_s));
    GL_CALL(glTexParameteri(texture->GetTarget(), GL_TEXTURE_WRAP_T,
                            texture_wrap_t));
    GL_CALL(glBindTexture(texture->GetTarget(), 0));
  }
  return true;
}

FaceOrientation GetFaceOrientationFromModelViewProjectionMatrix(
    const glm::mat4& model_view_projection_matrix) {
  return glm::determinant(model_view_projection_matrix) >= 0
             ? FaceOrientation_Ccw
             : FaceOrientation_Cw;
}

}  // namespace

void HardwareRasterizer::Impl::RenderTextureEGL(
    const render_tree::ImageNode* image_node,
    RenderTreeNodeVisitorDrawState* draw_state) {
  Image* image =
      base::polymorphic_downcast<Image*>(image_node->data().source.get());
  if (!image) {
    return;
  }
  image->EnsureInitialized();

  // Flush the Skia draw state to ensure that all previously issued Skia calls
  // are rendered so that the following draw command will appear in the correct
  // order.
  draw_state->render_target->flush();

  // This may be the first use of the render target, so ensure it is bound.
  GL_CALL(glBindFramebuffer(
      GL_FRAMEBUFFER, draw_state->render_target->getRenderTargetHandle()));

  SkISize canvas_size = draw_state->render_target->getBaseLayerSize();
  GL_CALL(glViewport(0, 0, canvas_size.width(), canvas_size.height()));

  SkIRect canvas_boundsi = draw_state->render_target->getDeviceClipBounds();
  // We need to translate from Skia's top-left corner origin to GL's bottom-left
  // corner origin.
  GL_CALL(glScissor(
      canvas_boundsi.x(),
      *current_surface_origin_ == kBottomLeft_GrSurfaceOrigin
          ? canvas_size.height() - canvas_boundsi.height() - canvas_boundsi.y()
          : canvas_boundsi.y(),
      canvas_boundsi.width(), canvas_boundsi.height()));

  glm::mat4 model_view_projection_matrix =
      GetFallbackTextureModelViewProjectionMatrix(
          *current_surface_origin_, canvas_size,
          draw_state->render_target->getTotalMatrix(),
          image_node->data().destination_rect);

  SetupGLStateForImageRender(image,
                             GetFaceOrientationFromModelViewProjectionMatrix(
                                 model_view_projection_matrix));

  if (!textured_mesh_renderer_) {
    textured_mesh_renderer_.emplace(graphics_context_);
  }

  // Convert our image into a format digestable by TexturedMeshRenderer.
  egl::TexturedMeshRenderer::Image textured_mesh_renderer_image =
      SkiaImageToTexturedMeshRendererImage(image_node->data().local_transform,
                                           image, render_tree::kMono);

  if (SetupGLTextureParameters(textured_mesh_renderer_image, GL_CLAMP_TO_EDGE,
                               GL_CLAMP_TO_EDGE)) {
    // Invoke our TexturedMeshRenderer to actually perform the draw call.
    textured_mesh_renderer_->RenderQuad(textured_mesh_renderer_image,
                                        model_view_projection_matrix);
  }

// Let Skia know that we've modified GL state.
#ifdef USE_SKIA_NEXT
  uint32_t untouched_states =
      kMSAAEnable_GrGLBackendState | kStencil_GrGLBackendState |
      kPixelStore_GrGLBackendState | kFixedFunction_GrGLBackendState;
#else
  uint32_t untouched_states =
      kMSAAEnable_GrGLBackendState | kStencil_GrGLBackendState |
      kPixelStore_GrGLBackendState | kFixedFunction_GrGLBackendState |
      kPathRendering_GrGLBackendState;
#endif
  gr_context_->resetContext(~untouched_states & kAll_GrBackendState);
}

void HardwareRasterizer::Impl::RenderTextureWithMeshFilterEGL(
    const render_tree::ImageNode* image_node,
    const render_tree::MapToMeshFilter& mesh_filter,
    RenderTreeNodeVisitorDrawState* draw_state) {
  if (mesh_filter.mesh_type() == render_tree::kRectangular) {
    NOTREACHED() << "This rasterizer does not support rectangular meshes on "
                    "the map-to-mesh filter.";
    return;
  }
  Image* image =
      base::polymorphic_downcast<Image*>(image_node->data().source.get());
  if (!image) {
    return;
  }
  image->EnsureInitialized();

  SkISize canvas_size = draw_state->render_target->getBaseLayerSize();
  SkIRect canvas_boundsi = draw_state->render_target->getDeviceClipBounds();

  // Flush the Skia draw state to ensure that all previously issued Skia calls
  // are rendered so that the following draw command will appear in the correct
  // order.
  draw_state->render_target->flush();

  // This may be the first use of the render target, so ensure it is bound.
  GL_CALL(glBindFramebuffer(
      GL_FRAMEBUFFER, draw_state->render_target->getRenderTargetHandle()));

  // We setup our viewport to fill the entire canvas.
  GL_CALL(glViewport(0, 0, canvas_size.width(), canvas_size.height()));
  GL_CALL(glScissor(0, 0, canvas_size.width(), canvas_size.height()));

  glm::mat4 model_view_projection_matrix =
      ModelViewMatrixSurfaceOriginAdjustment(*current_surface_origin_) *
      ScaleMatrixFor360VideoAdjustment(canvas_size, canvas_boundsi) *
      draw_state->transform_3d;

  SetupGLStateForImageRender(image,
                             GetFaceOrientationFromModelViewProjectionMatrix(
                                 model_view_projection_matrix));

  if (!textured_mesh_renderer_) {
    textured_mesh_renderer_.emplace(graphics_context_);
  }

  const VertexBufferObject* mono_vbo =
      base::polymorphic_downcast<HardwareMesh*>(
          mesh_filter.mono_mesh(image->GetSize()).get())
          ->GetVBO();

  // Convert our image into a format digestable by TexturedMeshRenderer.
  egl::TexturedMeshRenderer::Image textured_mesh_renderer_image =
      SkiaImageToTexturedMeshRendererImage(image_node->data().local_transform,
                                           image, mesh_filter.stereo_mode());

  if (SetupGLTextureParameters(textured_mesh_renderer_image, GL_CLAMP_TO_EDGE,
                               GL_CLAMP_TO_EDGE)) {
    // Invoke out TexturedMeshRenderer to actually perform the draw call.
    textured_mesh_renderer_->RenderVBO(
        mono_vbo->GetHandle(), mono_vbo->GetVertexCount(),
        mono_vbo->GetDrawMode(), textured_mesh_renderer_image,
        model_view_projection_matrix);
  }

  // Let Skia know that we've modified GL state.
  gr_context_->resetContext();
}

scoped_refptr<render_tree::Image>
HardwareRasterizer::Impl::ConvertRenderTreeToImage(
    const scoped_refptr<render_tree::Node>& root) {
  return resource_provider_->DrawOffscreenImage(root);
}

HardwareRasterizer::Impl::Impl(backend::GraphicsContext* graphics_context,
                               int skia_atlas_width, int skia_atlas_height,
                               int skia_cache_size_in_bytes,
                               int scratch_surface_cache_size_in_bytes,
                               bool purge_skia_font_caches_on_destruction,
                               bool force_deterministic_rendering)
    : graphics_context_(
          base::polymorphic_downcast<backend::GraphicsContextEGL*>(
              graphics_context)),
      force_deterministic_rendering_(force_deterministic_rendering) {
  TRACE_EVENT0("cobalt::renderer", "HardwareRasterizer::Impl::Impl()");

  graphics_context_->MakeCurrent();

  GrContextOptions context_options;
  // Main Glyph cache is in Alpha8 format, so assume 1 byte per pixel.
  context_options.fGlyphCacheTextureMaximumBytes =
      skia_atlas_width * skia_atlas_height;
  context_options.fAvoidStencilBuffers = true;
  gr_context_.reset(GrContext::MakeGL(context_options).release());

  DCHECK(gr_context_);
  // The GrContext manages a budget for GPU resources.  Setting the budget equal
  // to |skia_cache_size_in_bytes| + glyph cache's size will let Skia use
  // additional |skia_cache_size_in_bytes| for GPU resources like textures,
  // vertex buffers, etc.
  const int kSkiaCacheMaxResources = 128;
  gr_context_->setResourceCacheLimits(
      kSkiaCacheMaxResources,
      skia_cache_size_in_bytes +
          context_options.fGlyphCacheTextureMaximumBytes);

  base::Callback<sk_sp<SkSurface>(const math::Size&)>
      create_sk_surface_function = base::Bind(
          &HardwareRasterizer::Impl::CreateSkSurface, base::Unretained(this));

  scratch_surface_cache_.emplace(create_sk_surface_function,
                                 scratch_surface_cache_size_in_bytes);

  // Setup a resource provider for resources to be used with a hardware
  // accelerated Skia rasterizer.
  resource_provider_.reset(new HardwareResourceProvider(
      graphics_context_, gr_context_.get(),
      base::Bind(&HardwareRasterizer::Impl::SubmitOffscreenToRenderTarget,
                 base::Unretained(this)),
      purge_skia_font_caches_on_destruction));

  graphics_context_->ReleaseCurrentContext();

  int max_surface_size = std::max(gr_context_->maxRenderTargetSize(),
                                  gr_context_->maxTextureSize());
  DLOG(INFO) << "Max renderer surface size: " << max_surface_size;
}

HardwareRasterizer::Impl::~Impl() {
  graphics_context_->MakeCurrent();
  textured_mesh_renderer_ = base::nullopt;

  scratch_surface_cache_ = base::nullopt;
  sk_output_surface_map_.clear();
  gr_context_.reset(NULL);
  graphics_context_->ReleaseCurrentContext();
}

void HardwareRasterizer::Impl::Submit(
    const scoped_refptr<render_tree::Node>& render_tree,
    const scoped_refptr<backend::RenderTarget>& render_target,
    const Options& options) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  scoped_refptr<backend::RenderTargetEGL> render_target_egl(
      base::polymorphic_downcast<backend::RenderTargetEGL*>(
          render_target.get()));

  // Skip rendering if we lost the surface. This can happen just before suspend
  // on Android, so now we're just waiting for the suspend to clean up.
  if (render_target_egl->is_surface_bad()) {
    return;
  }

  backend::GraphicsContextEGL::ScopedMakeCurrent scoped_make_current(
      graphics_context_, render_target_egl.get());

  // First reset the graphics context state for the pending render tree
  // draw calls, in case we have modified state in between.
  gr_context_->resetContext();

  // Get a SkCanvas that outputs to our hardware render target.
  SkCanvas* canvas = GetCanvasFromRenderTarget(render_target);

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

  // Rasterize the passed in render tree to our hardware render target.
  RasterizeRenderTreeToCanvas(render_tree, canvas, kBottomLeft_GrSurfaceOrigin);

  {
    TRACE_EVENT0("cobalt::renderer", "Skia Flush");
    canvas->flush();
  }

  graphics_context_->SwapBuffers(render_target_egl.get());
  canvas->restore();
}

void HardwareRasterizer::Impl::SubmitOffscreen(
    const scoped_refptr<render_tree::Node>& render_tree, SkCanvas* canvas) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  RasterizeRenderTreeToCanvas(render_tree, canvas, kBottomLeft_GrSurfaceOrigin);
}

void HardwareRasterizer::Impl::SubmitOffscreenToRenderTarget(
    const scoped_refptr<render_tree::Node>& render_tree,
    const scoped_refptr<backend::RenderTarget>& render_target) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  scoped_refptr<backend::RenderTargetEGL> render_target_egl(
      base::polymorphic_downcast<backend::RenderTargetEGL*>(
          render_target.get()));

  if (render_target_egl->is_surface_bad()) {
    return;
  }

  backend::GraphicsContextEGL::ScopedMakeCurrent scoped_make_current(
      graphics_context_, render_target_egl.get());

  // Create a canvas from the render target.
  GrBackendRenderTarget skia_render_target =
      CobaltRenderTargetToSkiaBackendRenderTarget(*render_target);

  SkSurfaceProps surface_props =
      GetRenderTargetSurfaceProps(force_deterministic_rendering_);
  sk_sp<SkSurface> sk_output_surface = SkSurface::MakeFromBackendRenderTarget(
      gr_context_.get(), skia_render_target, kTopLeft_GrSurfaceOrigin,
      kRGBA_8888_SkColorType, nullptr, &surface_props);
  SkCanvas* canvas = sk_output_surface->getCanvas();

  canvas->clear(SkColorSetARGB(0, 0, 0, 0));

  // Render to the canvas and clean up.
  RasterizeRenderTreeToCanvas(render_tree, canvas, kTopLeft_GrSurfaceOrigin);
  canvas->flush();
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

void HardwareRasterizer::Impl::ReleaseContext() {
  graphics_context_->ReleaseCurrentContext();
}

sk_sp<SkSurface> HardwareRasterizer::Impl::CreateSkSurface(
    const math::Size& size) {
  TRACE_EVENT2("cobalt::renderer", "HardwareRasterizer::CreateSkSurface()",
               "width", size.width(), "height", size.height());
  SkImageInfo image_info =
      SkImageInfo::MakeN32(size.width(), size.height(), kPremul_SkAlphaType);
  // Do not count the resources for this surface towards the budget since
  // the budget is currently only meant for Skia managed resources.
  return SkSurface::MakeRenderTarget(gr_context_.get(), SkBudgeted::kNo,
                                     image_info);
}

std::unique_ptr<RenderTreeNodeVisitor::ScratchSurface>
HardwareRasterizer::Impl::CreateScratchSurface(const math::Size& size) {
  TRACE_EVENT2("cobalt::renderer", "HardwareRasterizer::CreateScratchImage()",
               "width", size.width(), "height", size.height());

  std::unique_ptr<CachedScratchSurfaceHolder> scratch_surface(
      new CachedScratchSurfaceHolder(&scratch_surface_cache_.value(), size));
  if (scratch_surface->GetSurface()) {
    return std::unique_ptr<RenderTreeNodeVisitor::ScratchSurface>(
        scratch_surface.release());
  } else {
    return std::unique_ptr<RenderTreeNodeVisitor::ScratchSurface>();
  }
}

SkCanvas* HardwareRasterizer::Impl::GetCanvasFromRenderTarget(
    const scoped_refptr<backend::RenderTarget>& render_target) {
  sk_sp<SkSurface> sk_output_surface;
  int32_t surface_map_key = render_target->GetSerialNumber();
  SkSurfaceMap::iterator iter = sk_output_surface_map_.find(surface_map_key);
  if (iter == sk_output_surface_map_.end()) {
    // Remove the least recently used SkSurface from the map if we exceed the
    // max allowed saved surfaces.
    if (sk_output_surface_map_.size() > kMaxSkSurfaceCount) {
      SkSurfaceMap::iterator iter = sk_output_surface_map_.begin();
      DLOG(WARNING)
          << "Erasing the SkSurface for RenderTarget " << iter->first
          << ". This may happen nominally during movement-triggered "
          << "replacement of SkSurfaces or else it may indicate the surface "
          << "map is thrashing because the total number of RenderTargets ("
          << kMaxSkSurfaceCount << ") has been exceeded.";
      sk_output_surface_map_.erase(iter);
    }

    // Create an SkSurface from the render target so that we can acquire a
    // SkCanvas object from it in Submit().
    SkSurfaceProps surface_props =
        GetRenderTargetSurfaceProps(force_deterministic_rendering_);
    // Create a canvas from the render target.
    GrBackendRenderTarget skia_render_target =
        CobaltRenderTargetToSkiaBackendRenderTarget(*render_target);
    sk_output_surface = SkSurface::MakeFromBackendRenderTarget(
        gr_context_.get(), skia_render_target, kBottomLeft_GrSurfaceOrigin,
        kRGBA_8888_SkColorType, nullptr, &surface_props);

    sk_output_surface_map_[surface_map_key] = sk_output_surface;
  } else {
    sk_output_surface = sk_output_surface_map_[surface_map_key];
    // Mark this RenderTarget/SkCanvas pair as the most recently used by
    // popping it and re-adding it.
    sk_output_surface_map_.erase(iter);
    sk_output_surface_map_[surface_map_key] = sk_output_surface;
  }
  return sk_output_surface->getCanvas();
}

void HardwareRasterizer::Impl::RasterizeRenderTreeToCanvas(
    const scoped_refptr<render_tree::Node>& render_tree, SkCanvas* canvas,
    GrSurfaceOrigin origin) {
  TRACE_EVENT0("cobalt::renderer", "RasterizeRenderTreeToCanvas");
  // TODO: This trace uses the name in the current benchmark to keep it work as
  // expected. Remove after switching to webdriver benchmark.
  TRACE_EVENT0("cobalt::renderer", "VisitRenderTree");

  base::Optional<GrSurfaceOrigin> old_origin = current_surface_origin_;
  current_surface_origin_.emplace(origin);

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
      base::Bind(&HardwareRasterizer::Impl::ConvertRenderTreeToImage,
                 base::Unretained(this)));
  DCHECK(render_tree);
  render_tree->Accept(&visitor);

  current_surface_origin_ = old_origin;
}

void HardwareRasterizer::Impl::ResetSkiaState() { gr_context_->resetContext(); }

HardwareRasterizer::HardwareRasterizer(
    backend::GraphicsContext* graphics_context, int skia_atlas_width,
    int skia_atlas_height, int skia_cache_size_in_bytes,
    int scratch_surface_cache_size_in_bytes,
    bool purge_skia_font_caches_on_destruction,
    bool force_deterministic_rendering)
    : impl_(new Impl(graphics_context, skia_atlas_width, skia_atlas_height,
                     skia_cache_size_in_bytes,
                     scratch_surface_cache_size_in_bytes,
                     purge_skia_font_caches_on_destruction,
                     force_deterministic_rendering)) {}

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
    const scoped_refptr<render_tree::Node>& render_tree, SkCanvas* canvas) {
  TRACE_EVENT0("cobalt::renderer", "HardwareRasterizer::SubmitOffscreen()");
  impl_->SubmitOffscreen(render_tree, canvas);
}

SkCanvas* HardwareRasterizer::GetCachedCanvas(
    const scoped_refptr<backend::RenderTarget>& render_target) {
  return impl_->GetCanvasFromRenderTarget(render_target);
}

render_tree::ResourceProvider* HardwareRasterizer::GetResourceProvider() {
  return impl_->GetResourceProvider();
}

GrContext* HardwareRasterizer::GetGrContext() { return impl_->GetGrContext(); }

void HardwareRasterizer::MakeCurrent() { return impl_->MakeCurrent(); }

void HardwareRasterizer::ReleaseContext() { return impl_->ReleaseContext(); }

}  // namespace skia
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt
