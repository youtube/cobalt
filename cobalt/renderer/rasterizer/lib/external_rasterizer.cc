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

#include <algorithm>
#include <cmath>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/lazy_instance.h"
#include "base/threading/thread_checker.h"
#include "cobalt/math/clamp.h"
#include "cobalt/render_tree/image.h"
#include "cobalt/renderer/backend/egl/graphics_context.h"
#include "cobalt/renderer/backend/egl/render_target.h"
#include "cobalt/renderer/rasterizer/common/find_node.h"
#include "cobalt/renderer/rasterizer/egl/hardware_rasterizer.h"
#include "cobalt/renderer/rasterizer/lib/exported/graphics.h"
#include "cobalt/renderer/rasterizer/lib/exported/video.h"
#include "cobalt/renderer/rasterizer/skia/hardware_image.h"
#include "cobalt/renderer/rasterizer/skia/hardware_mesh.h"
#include "cobalt/renderer/rasterizer/skia/hardware_rasterizer.h"
#include "starboard/shared/gles/gl_call.h"

COMPILE_ASSERT(
    cobalt::render_tree::kMono == kCbLibVideoStereoModeMono &&
        cobalt::render_tree::kLeftRight ==
            kCbLibVideoStereoModeStereoLeftRight &&
        cobalt::render_tree::kTopBottom ==
            kCbLibVideoStereoModeStereoTopBottom &&
        cobalt::render_tree::kLeftRightUnadjustedTextureCoords ==
            kCbLibVideoStereoModeStereoLeftRightUnadjustedCoordinates,
    lib_video_and_map_to_mesh_stereo_mode_constants_mismatch);

using cobalt::renderer::rasterizer::lib::ExternalRasterizer;

namespace {

const float kMaxRenderTargetSize = 15360.0f;

// Matches the signatures of the callback setter functions in exported/video.h
// and exported/graphics.h.
template <typename Ret, typename... Args>
using CallbackSetter = void(void*, Ret (*)(void*, Args...));

template <typename T, T* t, const char* ErrorMessage>
struct CallbackUpdate;

// Defines useful base:: wrappers for callback setter functions.
template <typename Ret, typename... Args, CallbackSetter<Ret, Args...>* Setter,
          const char* ErrorMessage>
struct CallbackUpdate<CallbackSetter<Ret, Args...>, Setter, ErrorMessage> {
  // Equivalent to the callback types defined in exported/video.h and
  // exported/graphics.h but with the context bound.
  using Callback = base::Callback<Ret(Args...)>;

  static Ret DefaultImplementation(Args...) { LOG(WARNING) << ErrorMessage; }

  struct LazyTraits {
    static const bool kRegisterOnExit = true;
    static const bool kAllowedToAccessOnNonjoinableThread = false;

    static Callback* New(void* instance) {
      return new (instance) Callback(base::Bind(DefaultImplementation));
    }
    static void Delete(Callback* instance) {
      return base::DefaultLazyInstanceTraits<Callback>::Delete(instance);
    }
  };

  // This provides a default warning function for the callbacks and allows to
  // set them even before the external rasterizer is created.
  using LazyCallback = base::LazyInstance<Callback, LazyTraits>;
};

// Creates an instance for the above template for a given callback setter
// function, with an error message.
// We must use 'extern' in here as otherwise we get compiler error C2970 when
// using MSVC for compilation.
#define INSTANCE_CALLBACK_UPDATE(instance_name, callback_setter)  \
  extern const char kWarningMessageDidNotSet##instance_name[] =   \
      #callback_setter                                            \
      "was never called to set a callback, yet Cobalt is "        \
      "attempting to call it.";                                   \
  using instance_name =                                           \
      CallbackUpdate<decltype(callback_setter), &callback_setter, \
                     kWarningMessageDidNotSet##instance_name>;

INSTANCE_CALLBACK_UPDATE(UpdateMeshes, CbLibVideoSetOnUpdateMeshes);
INSTANCE_CALLBACK_UPDATE(UpdateStereoMode, CbLibVideoSetOnUpdateStereoMode);
INSTANCE_CALLBACK_UPDATE(UpdateRgbTextureId, CbLibVideoSetOnUpdateRgbTextureId);
INSTANCE_CALLBACK_UPDATE(UpdateProjectionType,
                         CbLibVideoSetOnUpdateProjectionType);
INSTANCE_CALLBACK_UPDATE(GraphicsContextCreated,
                         CbLibGraphicsSetContextCreatedCallback);
INSTANCE_CALLBACK_UPDATE(BeginRenderFrame,
                         CbLibGraphicsSetBeginRenderFrameCallback);
INSTANCE_CALLBACK_UPDATE(EndRenderFrame,
                         CbLibGraphicsSetEndRenderFrameCallback);
#undef INSTANCE_CALLBACK_UPDATE

UpdateMeshes::LazyCallback g_update_meshes_callback = LAZY_INSTANCE_INITIALIZER;
UpdateStereoMode::LazyCallback g_update_stereo_mode_callback =
    LAZY_INSTANCE_INITIALIZER;
UpdateRgbTextureId::LazyCallback g_update_rgb_texture_id_callback =
    LAZY_INSTANCE_INITIALIZER;
UpdateProjectionType::LazyCallback g_update_projection_type_callback =
    LAZY_INSTANCE_INITIALIZER;
GraphicsContextCreated::LazyCallback g_graphics_context_created_callback =
    LAZY_INSTANCE_INITIALIZER;
BeginRenderFrame::LazyCallback g_begin_render_frame_callback =
    LAZY_INSTANCE_INITIALIZER;
EndRenderFrame::LazyCallback g_end_render_frame_callback =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace lib {

class ExternalRasterizer::Impl {
 public:
  Impl(backend::GraphicsContext* graphics_context, int skia_atlas_width,
       int skia_atlas_height, int skia_cache_size_in_bytes,
       int scratch_surface_cache_size_in_bytes, int surface_cache_size_in_bytes,
       bool purge_skia_font_caches_on_destruction);
  ~Impl();

  void Submit(const scoped_refptr<render_tree::Node>& render_tree,
              const scoped_refptr<backend::RenderTarget>& render_target);

  render_tree::ResourceProvider* GetResourceProvider();

  void MakeCurrent() { hardware_rasterizer_.MakeCurrent(); }

 private:
  void RenderOffscreenVideo(render_tree::FilterNode* map_to_mesh_filter_node);

  base::ThreadChecker thread_checker_;

  backend::GraphicsContextEGL* graphics_context_;

#if defined(COBALT_FORCE_DIRECT_GLES_RASTERIZER)
  egl::HardwareRasterizer hardware_rasterizer_;
#else
  skia::HardwareRasterizer hardware_rasterizer_;
#endif
  Rasterizer::Options options_;

  // The main offscreen render target to use when rendering UI or rectangular
  // video.
  scoped_refptr<backend::RenderTarget> main_offscreen_render_target_;
  scoped_ptr<backend::TextureEGL> main_texture_;

  // TODO: do not actually rasterize offscreen video, but just submit it to the
  // host directly.
  scoped_refptr<backend::RenderTarget> video_offscreen_render_target_;
  scoped_ptr<backend::TextureEGL> video_texture_;

  CbLibVideoProjectionType video_projection_type_;
  scoped_refptr<skia::HardwareMesh> left_eye_video_mesh_;
  scoped_refptr<skia::HardwareMesh> right_eye_video_mesh_;
  render_tree::StereoMode video_stereo_mode_;
  int video_texture_rgb_;
};

ExternalRasterizer::Impl::Impl(backend::GraphicsContext* graphics_context,
                               int skia_atlas_width, int skia_atlas_height,
                               int skia_cache_size_in_bytes,
                               int scratch_surface_cache_size_in_bytes,
                               int rasterizer_gpu_cache_size_in_bytes,
                               bool purge_skia_font_caches_on_destruction)
    : graphics_context_(
          base::polymorphic_downcast<backend::GraphicsContextEGL*>(
              graphics_context)),
      hardware_rasterizer_(
          graphics_context, skia_atlas_width, skia_atlas_height,
          skia_cache_size_in_bytes, scratch_surface_cache_size_in_bytes,
          rasterizer_gpu_cache_size_in_bytes,
          purge_skia_font_caches_on_destruction),
      video_projection_type_(kCbLibVideoProjectionTypeNone),
      video_stereo_mode_(render_tree::StereoMode::kMono),
      video_texture_rgb_(0) {
  options_.flags = Rasterizer::kSubmitFlags_Clear;
  graphics_context_->MakeCurrent();

  // TODO: Import the correct size for this and any other textures from the lib
  // client and re-generate the size as appropriate.
  main_offscreen_render_target_ =
      graphics_context_->CreateOffscreenRenderTarget(math::Size(1920, 1080));
  main_texture_.reset(new backend::TextureEGL(
      graphics_context_,
      make_scoped_refptr(base::polymorphic_downcast<backend::RenderTargetEGL*>(
          main_offscreen_render_target_.get()))));

  g_graphics_context_created_callback.Get().Run();
}

ExternalRasterizer::Impl::~Impl() {
  graphics_context_->MakeCurrent();
}

void ExternalRasterizer::Impl::Submit(
    const scoped_refptr<render_tree::Node>& render_tree,
    const scoped_refptr<backend::RenderTarget>& render_target) {
  backend::RenderTargetEGL* render_target_egl =
      base::polymorphic_downcast<backend::RenderTargetEGL*>(
          render_target.get());

  // When the provided RenderTarget is not a window RenderTarget, then this
  // implies the rasterized RenderTree should not be shown directly to the user
  // and thus should not be rasterized into a texture and sent through to the
  // client implementing CbLibRenderFrame.
  if (!render_target_egl->IsWindowRenderTarget()) {
    hardware_rasterizer_.Submit(render_tree, render_target, options_);
    return;
  }

  graphics_context_->MakeCurrentWithSurface(render_target_egl);

  // Attempt to find map to mesh filter node, then render video subtree
  // offscreen.
  auto map_to_mesh_search = common::FindNode<render_tree::FilterNode>(
      render_tree, base::Bind(common::HasMapToMesh),
      base::Bind(common::ReplaceWithEmptyCompositionNode));
  if (map_to_mesh_search.found_node != NULL) {
    base::optional<render_tree::MapToMeshFilter> filter =
        map_to_mesh_search.found_node->data().map_to_mesh_filter;

    CbLibVideoProjectionType new_projection_type;
    if (!filter->left_eye_mesh()) {
      // Video is rectangular. Mesh is provided externally (by host).
      new_projection_type = kCbLibVideoProjectionTypeRectangular;
    } else {
      new_projection_type = kCbLibVideoProjectionTypeMesh;
    }

    if (video_projection_type_ != new_projection_type) {
      video_projection_type_ = new_projection_type;
      g_update_projection_type_callback.Get().Run(video_projection_type_);
    }

    if (filter->stereo_mode() != video_stereo_mode_) {
      video_stereo_mode_ = filter->stereo_mode();
      g_update_stereo_mode_callback.Get().Run(
          static_cast<CbLibVideoStereoMode>(video_stereo_mode_));
    }

    if (video_projection_type_ == kCbLibVideoProjectionTypeMesh) {
      // Use resolution to lookup custom mesh map.
      const scoped_refptr<render_tree::Node>& video_render_tree =
          map_to_mesh_search.found_node->data().source;
      math::SizeF resolutionf = video_render_tree->GetBounds().size();
      int width = static_cast<int>(resolutionf.width());
      int height = static_cast<int>(resolutionf.height());

      math::Size resolution(width, height);
      scoped_refptr<skia::HardwareMesh> left_eye_video_mesh(
          base::polymorphic_downcast<skia::HardwareMesh*>(
              filter->left_eye_mesh(resolution).get()));
      scoped_refptr<skia::HardwareMesh> right_eye_video_mesh(
          base::polymorphic_downcast<skia::HardwareMesh*>(
              filter->right_eye_mesh(resolution).get()));

      DCHECK(left_eye_video_mesh);

      if (left_eye_video_mesh_.get() != left_eye_video_mesh.get() ||
          right_eye_video_mesh_.get() != right_eye_video_mesh.get()) {
        left_eye_video_mesh_ = left_eye_video_mesh;
        right_eye_video_mesh_ = right_eye_video_mesh;

        CbLibVideoMesh left_mesh;
        left_mesh.vertex_count =
            static_cast<int>(left_eye_video_mesh_->GetVertexCount());
        left_mesh.draw_mode = static_cast<CbLibVideoMeshDrawMode>(
            left_eye_video_mesh_->GetDrawMode());
        left_mesh.vertices = left_eye_video_mesh_->GetVertices();

        if (right_eye_video_mesh_) {
          CbLibVideoMesh right_mesh;
          right_mesh.vertex_count =
              static_cast<int>(right_eye_video_mesh_->GetVertexCount());
          right_mesh.draw_mode = static_cast<CbLibVideoMeshDrawMode>(
              right_eye_video_mesh_->GetDrawMode());
          right_mesh.vertices = right_eye_video_mesh_->GetVertices();
          g_update_meshes_callback.Get().Run(left_mesh, right_mesh);
        } else {
          g_update_meshes_callback.Get().Run(left_mesh, left_mesh);
        }
      }
    }

    // Render video to external texture(s) and pass those to the host.
    RenderOffscreenVideo(map_to_mesh_search.found_node);
  } else {
    if (video_projection_type_ != kCbLibVideoProjectionTypeNone) {
      video_projection_type_ = kCbLibVideoProjectionTypeNone;
      g_update_projection_type_callback.Get().Run(video_projection_type_);
    }
  }

  backend::RenderTargetEGL* main_texture_render_target_egl =
      base::polymorphic_downcast<backend::RenderTargetEGL*>(
          main_offscreen_render_target_.get());
  hardware_rasterizer_.Submit(map_to_mesh_search.replaced_tree,
                              main_offscreen_render_target_, options_);

  // TODO: Allow clients to specify arbitrary subtrees to render into
  // different textures?
  const intptr_t texture_handle = main_texture_->GetPlatformHandle();
  g_begin_render_frame_callback.Get().Run(texture_handle);
  graphics_context_->SwapBuffers(render_target_egl);
  g_end_render_frame_callback.Get().Run();
}

render_tree::ResourceProvider* ExternalRasterizer::Impl::GetResourceProvider() {
  return hardware_rasterizer_.GetResourceProvider();
}

void ExternalRasterizer::Impl::RenderOffscreenVideo(
    render_tree::FilterNode* map_to_mesh_filter_node) {
  DCHECK(map_to_mesh_filter_node);
  if (!map_to_mesh_filter_node) {
    return;
  }

  // Render the mesh-video into a texture to render into 3D space.
  const scoped_refptr<render_tree::Node>& video_render_tree =
      map_to_mesh_filter_node->data().source;
  // Search the video_render_tree for the video frame ImageNode.
  auto image_node =
      common::FindNode<render_tree::ImageNode>(video_render_tree).found_node;

  math::SizeF video_size_float = video_render_tree->GetBounds().size();
  if (image_node.get() && image_node->data().source) {
    video_size_float = image_node->data().source.get()->GetSize();
  }

  // Width and height of the video render target are based on the size of the
  // video clamped to the valid range for creating an offscreen render target.
  const float target_width = math::Clamp(std::floor(video_size_float.width()),
                                         1.0f, kMaxRenderTargetSize);
  const float target_height = math::Clamp(std::floor(video_size_float.height()),
                                          1.0f, kMaxRenderTargetSize);
  const math::Size video_size(target_width, target_height);

  if (!video_offscreen_render_target_ ||
      video_offscreen_render_target_->GetSize() != video_size) {
    video_offscreen_render_target_ =
        graphics_context_->CreateOffscreenRenderTarget(video_size);
    DLOG(INFO) << "Created new video_offscreen_render_target_: "
               << video_offscreen_render_target_->GetSize();

    // Note: The TextureEGL this pointer references must first be destroyed by
    // calling reset() before a new TextureEGL can be constructed.
    video_texture_.reset();
    video_texture_.reset(new backend::TextureEGL(
        graphics_context_,
        make_scoped_refptr(
            base::polymorphic_downcast<backend::RenderTargetEGL*>(
                video_offscreen_render_target_.get()))));
  }

  if (image_node.get()) {
    // Create a new ImageNode around the raw image data which will
    // automatically scale it to the right size.
    backend::RenderTargetEGL* video_offscreen_render_target_egl =
        base::polymorphic_downcast<backend::RenderTargetEGL*>(
            video_offscreen_render_target_.get());

    const scoped_refptr<render_tree::ImageNode> correctly_scaled_image_node(
        new render_tree::ImageNode(image_node->data().source));

    // TODO: Instead of submitting the image for rendering and producing an
    // RGB texture, try to cast to HardwareMultiPlaneImage to use the YUV
    // textures already produced by decode-to-texture.
    hardware_rasterizer_.Submit(correctly_scaled_image_node,
                                video_offscreen_render_target_, options_);

    const intptr_t video_texture_handle = video_texture_->GetPlatformHandle();
    if (video_texture_rgb_ != video_texture_handle) {
      video_texture_rgb_ = video_texture_handle;
      g_update_rgb_texture_id_callback.Get().Run(video_texture_handle);
    }
  }
}

ExternalRasterizer::ExternalRasterizer(
    backend::GraphicsContext* graphics_context, int skia_atlas_width,
    int skia_atlas_height, int skia_cache_size_in_bytes,
    int scratch_surface_cache_size_in_bytes,
    int rasterizer_gpu_cache_size_in_bytes,
    bool purge_skia_font_caches_on_destruction)
    : impl_(new Impl(
          graphics_context, skia_atlas_width, skia_atlas_height,
          skia_cache_size_in_bytes, scratch_surface_cache_size_in_bytes,
          rasterizer_gpu_cache_size_in_bytes,
          purge_skia_font_caches_on_destruction)) {
}

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

void ExternalRasterizer::MakeCurrent() {
  return impl_->MakeCurrent();
}

}  // namespace lib
}  // namespace rasterizer
}  // namespace renderer
}  // namespace cobalt

void CbLibVideoSetOnUpdateMeshes(void* context,
                                 CbLibVideoUpdateMeshesCallback callback) {
  g_update_meshes_callback.Get() =
      callback ? base::Bind(callback, context)
               : base::Bind(&UpdateMeshes::DefaultImplementation);
}

void CbLibVideoSetOnUpdateStereoMode(
    void* context, CbLibVideoUpdateStereoModeCallback callback) {
  g_update_stereo_mode_callback.Get() =
      callback ? base::Bind(callback, context)
               : base::Bind(&UpdateStereoMode::DefaultImplementation);
}

void CbLibVideoSetOnUpdateRgbTextureId(
    void* context, CbLibVideoUpdateRgbTextureIdCallback callback) {
  g_update_rgb_texture_id_callback.Get() =
      callback ? base::Bind(callback, context)
               : base::Bind(&UpdateRgbTextureId::DefaultImplementation);
}

void CbLibVideoSetOnUpdateProjectionType(
    void* context, CbLibVideoUpdateProjectionTypeCallback callback) {
  g_update_projection_type_callback.Get() =
      callback ? base::Bind(callback, context)
               : base::Bind(&UpdateProjectionType::DefaultImplementation);
}

void CbLibGraphicsSetContextCreatedCallback(
    void* context, CbLibGraphicsContextCreatedCallback callback) {
  g_graphics_context_created_callback.Get() =
      callback ? base::Bind(callback, context)
               : base::Bind(&GraphicsContextCreated::DefaultImplementation);
}

void CbLibGraphicsSetBeginRenderFrameCallback(
    void* context, CbLibGraphicsBeginRenderFrameCallback callback) {
  g_begin_render_frame_callback.Get() =
      callback ? base::Bind(callback, context)
               : base::Bind(&BeginRenderFrame::DefaultImplementation);
}

void CbLibGraphicsSetEndRenderFrameCallback(
    void* context, CbLibGraphicsEndRenderFrameCallback callback) {
  g_end_render_frame_callback.Get() =
      callback ? base::Bind(callback, context)
               : base::Bind(&EndRenderFrame::DefaultImplementation);
}
