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
#include "third_party/glm/glm/gtc/matrix_transform.hpp"
#include "third_party/glm/glm/gtc/type_ptr.hpp"

#ifdef ANGLE_ENABLE_D3D11
// Normally, ANGLE defines this symbol internally using its gyp files.
//
// This code is temporary, so for now just enforce that we are truly building
// Windows when this code path is taken.
#if !defined(_WIN32) && !defined(_WIN64)
#error Direct mirroring to the system window is only supported for Windows!
#endif
#define ANGLE_PLATFORM_WINDOWS 1

#include <d3d11.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include "third_party/angle/src/libANGLE/Context.h"
#include "third_party/angle/src/libANGLE/Display.h"
#include "third_party/angle/src/libANGLE/renderer/d3d/d3d11/Blit11.h"
#include "third_party/angle/src/libANGLE/renderer/d3d/d3d11/Context11.h"
#include "third_party/angle/src/libANGLE/renderer/d3d/d3d11/Renderer11.h"
#include "third_party/angle/src/libANGLE/renderer/d3d/d3d11/SwapChain11.h"
#include "third_party/angle/src/libANGLE/renderer/d3d/DisplayD3D.h"
#include "third_party/angle/src/libANGLE/renderer/d3d/SurfaceD3D.h"
#include "third_party/angle/src/libANGLE/Surface.h"
#endif

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

static const float kMaxRenderTargetSize = 15360.0f;

// The minimum amount of change required in the desired texture size to generate
// a new offscreen render target for the quad texture.
static const float kMinTextureSizeEpsilon = 20.0f;

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
INSTANCE_CALLBACK_UPDATE(UpdateRgbTextureId, CbLibVideoSetOnUpdateRgbTextureId);
INSTANCE_CALLBACK_UPDATE(UpdateProjectionTypeAndStereoMode,
                         CbLibVideoSetOnUpdateProjectionTypeAndStereoMode);
INSTANCE_CALLBACK_UPDATE(UpdateAspectRatio, CbLibVideoSetOnUpdateAspectRatio);
INSTANCE_CALLBACK_UPDATE(GraphicsContextCreated,
                         CbLibGraphicsSetContextCreatedCallback);
INSTANCE_CALLBACK_UPDATE(RenderFrame, CbLibGraphicsSetRenderFrameCallback);
#undef INSTANCE_CALLBACK_UPDATE

UpdateMeshes::LazyCallback g_update_meshes_callback = LAZY_INSTANCE_INITIALIZER;
UpdateRgbTextureId::LazyCallback g_update_rgb_texture_id_callback =
    LAZY_INSTANCE_INITIALIZER;
UpdateProjectionTypeAndStereoMode::LazyCallback
    g_update_projection_type_and_stereo_mode_callback =
        LAZY_INSTANCE_INITIALIZER;
UpdateAspectRatio::LazyCallback g_update_aspect_ratio_callback =
    LAZY_INSTANCE_INITIALIZER;
GraphicsContextCreated::LazyCallback g_graphics_context_created_callback =
    LAZY_INSTANCE_INITIALIZER;
RenderFrame::LazyCallback g_render_frame_callback = LAZY_INSTANCE_INITIALIZER;

bool ApproxEqual(const cobalt::math::Size& a, const cobalt::math::Size& b,
                 float epsilon) {
  return std::abs(a.width() - b.width()) < epsilon &&
         std::abs(a.height() - b.height()) < epsilon;
}

cobalt::math::Size CobaltSizeFromCbLibSize(CbLibSize size) {
  return cobalt::math::Size(size.width, size.height);
}

ExternalRasterizer::Impl* g_external_rasterizer_impl = nullptr;

}  // namespace

namespace cobalt {
namespace renderer {
namespace rasterizer {
namespace lib {

class ExternalRasterizer::Impl {
 public:
  Impl(backend::GraphicsContext* graphics_context, int skia_atlas_width,
       int skia_atlas_height, int skia_cache_size_in_bytes,
       int scratch_surface_cache_size_in_bytes,
#if defined(COBALT_FORCE_DIRECT_GLES_RASTERIZER)
       int offscreen_target_cache_size_in_bytes,
#endif
       bool purge_skia_font_caches_on_destruction,
       bool disable_rasterizer_caching);
  ~Impl();

  void Submit(const scoped_refptr<render_tree::Node>& render_tree,
              const scoped_refptr<backend::RenderTarget>& render_target);
  void RenderCobalt();
  void CopyBackbuffer(uintptr_t surface, float width_scale);
  void SwapBackbuffer();

  render_tree::ResourceProvider* GetResourceProvider();

  void MakeCurrent() { hardware_rasterizer_.MakeCurrent(); }

  intptr_t GetMainTextureHandle();

  // Sets the target size in pixels to use for the main render target buffer.
  void SetTargetMainTextureSize(const cobalt::math::Size& target_render_size) {
    target_main_render_target_size_ = target_render_size;
  }

 private:
  void RenderOffscreenVideo(render_tree::FilterNode* map_to_mesh_filter_node);

  void SubmitWithUpdatedTextureSize(
      const cobalt::math::Size& native_render_target_size,
      const scoped_refptr<render_tree::Node>& render_tree);

  base::ThreadChecker thread_checker_;

  backend::GraphicsContextEGL* graphics_context_;

#if defined(COBALT_FORCE_DIRECT_GLES_RASTERIZER)
  egl::HardwareRasterizer hardware_rasterizer_;
#else
  skia::HardwareRasterizer hardware_rasterizer_;
#endif
  Rasterizer::Options options_;

  // Temporary reference to the render tree and main window render target.
  //
  // Only valid inside of Submit().
  scoped_refptr<render_tree::Node> render_tree_temp_;
  backend::RenderTargetEGL* main_render_target_temp_;

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
  GLuint video_texture_rgb_;
  // The 'target'/'ideal' size to use for the main RenderTarget. The actual size
  // of the buffer for the main RenderTarget should aim to be within some small
  // delta of this whenever a new RenderTree is rendered.
  cobalt::math::Size target_main_render_target_size_;
  int video_width_;
  int video_height_;
};

ExternalRasterizer::Impl::Impl(backend::GraphicsContext* graphics_context,
                               int skia_atlas_width, int skia_atlas_height,
                               int skia_cache_size_in_bytes,
                               int scratch_surface_cache_size_in_bytes,
#if defined(COBALT_FORCE_DIRECT_GLES_RASTERIZER)
                               int offscreen_target_cache_size_in_bytes,
#endif
                               bool purge_skia_font_caches_on_destruction,
                               bool disable_rasterizer_caching)
    : graphics_context_(
          base::polymorphic_downcast<backend::GraphicsContextEGL*>(
              graphics_context)),
      hardware_rasterizer_(graphics_context, skia_atlas_width,
                           skia_atlas_height, skia_cache_size_in_bytes,
                           scratch_surface_cache_size_in_bytes,

#if defined(COBALT_FORCE_DIRECT_GLES_RASTERIZER)
                           offscreen_target_cache_size_in_bytes,
#endif
                           purge_skia_font_caches_on_destruction
#if defined(COBALT_FORCE_DIRECT_GLES_RASTERIZER)
                           ,
                           disable_rasterizer_caching
#endif
                           ),
      render_tree_temp_(nullptr),
      main_render_target_temp_(nullptr),
      video_projection_type_(kCbLibVideoProjectionTypeNone),
      video_stereo_mode_(render_tree::StereoMode::kMono),
      video_texture_rgb_(0),
      video_width_(-1),
      video_height_(-1),
      target_main_render_target_size_(1, 1) {
  CHECK(!g_external_rasterizer_impl);
  g_external_rasterizer_impl = this;
  options_.flags = Rasterizer::kSubmitFlags_Clear;
  graphics_context_->MakeCurrent();

  main_offscreen_render_target_ =
      graphics_context_->CreateOffscreenRenderTarget(
          target_main_render_target_size_);
  main_texture_.reset(new backend::TextureEGL(
      graphics_context_,
      make_scoped_refptr(base::polymorphic_downcast<backend::RenderTargetEGL*>(
          main_offscreen_render_target_.get()))));

  g_graphics_context_created_callback.Get().Run();
}

ExternalRasterizer::Impl::~Impl() {
  graphics_context_->MakeCurrent();
  g_external_rasterizer_impl = nullptr;
}

void ExternalRasterizer::Impl::Submit(
    const scoped_refptr<render_tree::Node>& render_tree,
    const scoped_refptr<backend::RenderTarget>& render_target) {
  DCHECK(thread_checker_.CalledOnValidThread());

  backend::RenderTargetEGL* render_target_egl =
      base::polymorphic_downcast<backend::RenderTargetEGL*>(
          render_target.get());

  // When the provided RenderTarget is not a window RenderTarget, then this
  // implies the rasterized RenderTree should not be shown directly to the user
  // and thus should not be rasterized into a texture and sent through to the
  // client implementing CbLibRenderFrame.
  if (!render_target_egl->IsWindowRenderTarget()) {
    hardware_rasterizer_.Submit(render_tree, render_target, options_);
  } else {
    // Store the pointers to the render tree and target temporarily so they
    // can be used from callbacks triggered from the app's RenderFrameCallback.
    render_tree_temp_ = render_tree.get();
    main_render_target_temp_ = render_target_egl;

    // TODO: Allow clients to specify arbitrary subtrees to render into
    // different textures?
    g_render_frame_callback.Get().Run();

    render_tree_temp_ = nullptr;
    main_render_target_temp_ = nullptr;
  }
}

void ExternalRasterizer::Impl::RenderCobalt() {
  // This method must be called from the g_render_frame_callback set by the
  // host, otherwise these members will be nullptr.
  CHECK(main_render_target_temp_);
  CHECK(render_tree_temp_.get());
  DCHECK(thread_checker_.CalledOnValidThread());

  graphics_context_->MakeCurrentWithSurface(main_render_target_temp_);

  // Attempt to find map to mesh filter node, then render video subtree
  // offscreen.
  auto map_to_mesh_search = common::FindNode<render_tree::FilterNode>(
      render_tree_temp_, base::Bind(common::HasMapToMesh),
      base::Bind(common::ReplaceWithEmptyCompositionNode));
  if (map_to_mesh_search.found_node != NULL) {
    base::optional<render_tree::MapToMeshFilter> filter =
        map_to_mesh_search.found_node->data().map_to_mesh_filter;

    CbLibVideoProjectionType new_projection_type;
    switch (filter->mesh_type()) {
      case render_tree::kRectangular:
        // Video is rectangular. Mesh is provided externally (by host).
        new_projection_type = kCbLibVideoProjectionTypeRectangular;
        break;
      case render_tree::kCustomMesh:
        new_projection_type = kCbLibVideoProjectionTypeMesh;
        break;
    }

    if (video_projection_type_ != new_projection_type ||
        filter->stereo_mode() != video_stereo_mode_) {
      // Note the above condition will always be true when playback has not
      // started.
      video_projection_type_ = new_projection_type;
      video_stereo_mode_ = filter->stereo_mode();
      g_update_projection_type_and_stereo_mode_callback.Get().Run(
          video_projection_type_,
          static_cast<CbLibVideoStereoMode>(video_stereo_mode_));
    }

    const scoped_refptr<render_tree::Node>& video_render_tree =
        map_to_mesh_search.found_node->data().source;
    math::SizeF resolutionf = video_render_tree->GetBounds().size();
    int width = static_cast<int>(resolutionf.width());
    int height = static_cast<int>(resolutionf.height());

    if (video_width_ != width || video_height_ != height) {
      g_update_aspect_ratio_callback.Get().Run(
          width > 0 && height > 0 ?  // Avoid division by zero.
              resolutionf.width() / resolutionf.height()
                                  : 0.0f);
      video_width_ = width;
      video_height_ = height;
    }

    if (video_projection_type_ == kCbLibVideoProjectionTypeMesh) {
      // Use resolution to lookup custom mesh map.
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
      g_update_projection_type_and_stereo_mode_callback.Get().Run(
          video_projection_type_, kCbLibVideoStereoModeMono);
      video_width_ = video_height_ = -1;
    }
  }

  SubmitWithUpdatedTextureSize(main_render_target_temp_->GetSize(),
                               map_to_mesh_search.replaced_tree);
}

void ExternalRasterizer::Impl::CopyBackbuffer(uintptr_t surface,
                                              float width_scale) {
  // This method must be called from the g_render_frame_callback set by the
  // host, otherwise this member will be nullptr.
  CHECK(main_render_target_temp_);
  DCHECK(thread_checker_.CalledOnValidThread());

  // If there is any host-provided surface, mirror it directly into the
  // system window with a blit.  The surface will be streched or shrunk as
  // appropriate so that it fits the system window's backbuffer.
  if (surface != 0) {
#ifdef ANGLE_ENABLE_D3D11
    EGLContext egl_context = eglGetCurrentContext();
    EGLSurface egl_mirror_surface = reinterpret_cast<EGLSurface>(surface);
    EGLSurface egl_window_surface = main_render_target_temp_->GetSurface();
    ::gl::Context* angle_context =
        reinterpret_cast<::gl::Context*>(egl_context);
    ::egl::Surface* angle_mirror_surface = reinterpret_cast<::egl::Surface*>(
        egl_mirror_surface);
    ::egl::Surface* angle_window_surface =
        reinterpret_cast<::egl::Surface*>(egl_window_surface);
    ::rx::SurfaceD3D* d3d_mirror_surface =
        ::rx::GetImplAs<rx::SurfaceD3D>(angle_mirror_surface);
    ::rx::SurfaceD3D* d3d_window_surface =
        ::rx::GetImplAs<rx::SurfaceD3D>(angle_window_surface);
    ::rx::Context11* d3d11_context =
        ::rx::GetImplAs<::rx::Context11>(angle_context);
    ::rx::Renderer11* d3d11_renderer = d3d11_context->getRenderer();
    ::rx::SwapChain11* d3d11_mirror_swapchain =
        static_cast<::rx::SwapChain11*>(d3d_mirror_surface->getSwapChain());
    ::rx::SwapChain11* d3d11_window_swapchain =
        static_cast<::rx::SwapChain11*>(d3d_window_surface->getSwapChain());
    float src_width_scale = std::min(1.0f, std::max(width_scale, 0.0f));

    d3d11_renderer->getBlitter()->copyTexture(
        d3d11_mirror_swapchain->getRenderTargetShaderResource(),
        gl::Box(0, 0, 0, d3d11_mirror_swapchain->getWidth() * src_width_scale,
                d3d11_mirror_swapchain->getHeight(), 1),
        gl::Extents(d3d11_mirror_swapchain->getWidth(),
                    d3d11_mirror_swapchain->getHeight(), 1),
        d3d11_window_swapchain->getRenderTarget(),
        gl::Box(0, 0, 0, d3d11_window_swapchain->getWidth(),
                d3d11_window_swapchain->getHeight(), 1),
        gl::Extents(d3d11_window_swapchain->getWidth(),
                    d3d11_window_swapchain->getHeight(), 1),
        nullptr, GL_RGBA_INTEGER, GL_LINEAR, false, false, false);
#else
    LOG(FATAL) << "Direct mirroring to the system window is only supported "
               << "under DirectX.";
#endif
  }
}

void ExternalRasterizer::Impl::SwapBackbuffer() {
  // This method must be called from the g_render_frame_callback set by the
  // host, otherwise this member will be nullptr.
  CHECK(main_render_target_temp_);
  DCHECK(thread_checker_.CalledOnValidThread());

  graphics_context_->SwapBuffers(main_render_target_temp_);
}

// TODO: Share this logic with the ComponentRenderer.
void ExternalRasterizer::Impl::SubmitWithUpdatedTextureSize(
    const cobalt::math::Size& native_render_target_size,
    const scoped_refptr<render_tree::Node>& render_tree) {
  // We need to make sure that our texture ID gets updated after the
  // HardwareRasterizer submits, because the render target in use might get
  // swapped during the submit.
  bool texture_id_needs_update = false;

  // Create a new offscreen render target if the exist one's size is far enough
  // off from the target/ideal size.
  if (!main_offscreen_render_target_ ||
      !ApproxEqual(main_offscreen_render_target_->GetSize(),
                   target_main_render_target_size_, kMinTextureSizeEpsilon)) {
    LOG(INFO) << "Creating a new offscreen render target of size "
              << target_main_render_target_size_;
    main_offscreen_render_target_ =
        graphics_context_->CreateOffscreenRenderTarget(
            target_main_render_target_size_);
    texture_id_needs_update = true;
  }

  DCHECK(native_render_target_size.width());
  DCHECK(native_render_target_size.height());
  // We wrap the RenderTree in a MatrixTransformNode to scale the RenderTree so
  // that its scale relative to our RenderTarget matches its original scale
  // relative to native_render_target_size. This makes the texture cropped to
  // fit our RenderTarget the same amount it would be for the original
  // RenderTarget.
  const float texture_x_scale =
      static_cast<float>(main_offscreen_render_target_->GetSize().width()) /
      native_render_target_size.width();
  const float texture_y_scale =
      static_cast<float>(main_offscreen_render_target_->GetSize().height()) /
      native_render_target_size.height();
  const glm::mat3 scale_mat(glm::scale(
      glm::mat4(1.0f), glm::vec3(texture_x_scale, texture_y_scale, 1)));
  const cobalt::math::Matrix3F root_transform_matrix =
      cobalt::math::Matrix3F::FromArray(glm::value_ptr(scale_mat));
  scoped_refptr<render_tree::MatrixTransformNode> scaled_main_node(
      new render_tree::MatrixTransformNode(render_tree, root_transform_matrix));

  hardware_rasterizer_.Submit(scaled_main_node, main_offscreen_render_target_,
                              options_);

  if (texture_id_needs_update) {
    // Note: The TextureEGL this pointer references must first be destroyed by
    // calling reset() before a new TextureEGL can be constructed.
    main_texture_.reset();
    main_texture_.reset(new backend::TextureEGL(
        graphics_context_,
        make_scoped_refptr(
            base::polymorphic_downcast<backend::RenderTargetEGL*>(
                main_offscreen_render_target_.get()))));
  }
}

render_tree::ResourceProvider* ExternalRasterizer::Impl::GetResourceProvider() {
  return hardware_rasterizer_.GetResourceProvider();
}

intptr_t ExternalRasterizer::Impl::GetMainTextureHandle() {
  return main_texture_->GetPlatformHandle();
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

  // We need to make sure that our texture ID gets updated after the
  // HardwareRasterizer submits, because the render target in use might get
  // swapped during the submit.
  bool texture_id_needs_update = false;

  if (!video_offscreen_render_target_ ||
      video_offscreen_render_target_->GetSize() != video_size) {
    video_offscreen_render_target_ =
        graphics_context_->CreateOffscreenRenderTarget(video_size);
    DLOG(INFO) << "Created new video_offscreen_render_target_: "
               << video_offscreen_render_target_->GetSize();
    texture_id_needs_update = true;
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

    if (texture_id_needs_update) {
      // Note: The TextureEGL this pointer references must first be destroyed by
      // calling reset() before a new TextureEGL can be constructed.
      video_texture_.reset();
      video_texture_.reset(new backend::TextureEGL(
          graphics_context_,
          make_scoped_refptr(
              base::polymorphic_downcast<backend::RenderTargetEGL*>(
                  video_offscreen_render_target_.get()))));
    }

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
#if defined(COBALT_FORCE_DIRECT_GLES_RASTERIZER)
    int offscreen_target_cache_size_in_bytes,
#endif
    bool purge_skia_font_caches_on_destruction,
    bool disable_rasterizer_caching)
    : impl_(new Impl(graphics_context, skia_atlas_width, skia_atlas_height,
                     skia_cache_size_in_bytes,
                     scratch_surface_cache_size_in_bytes,
#if defined(COBALT_FORCE_DIRECT_GLES_RASTERIZER)
                     offscreen_target_cache_size_in_bytes,
#endif
                     purge_skia_font_caches_on_destruction,
                     disable_rasterizer_caching)) {}

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

void ExternalRasterizer::MakeCurrent() { return impl_->MakeCurrent(); }

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

void CbLibVideoSetOnUpdateRgbTextureId(
    void* context, CbLibVideoUpdateRgbTextureIdCallback callback) {
  g_update_rgb_texture_id_callback.Get() =
      callback ? base::Bind(callback, context)
               : base::Bind(&UpdateRgbTextureId::DefaultImplementation);
}

void CbLibVideoSetOnUpdateProjectionTypeAndStereoMode(
    void* context,
    CbLibVideoUpdateProjectionTypeAndStereoModeCallback callback) {
  g_update_projection_type_and_stereo_mode_callback.Get() =
      callback ? base::Bind(callback, context)
               : base::Bind(
                     &UpdateProjectionTypeAndStereoMode::DefaultImplementation);
}

void CbLibVideoSetOnUpdateAspectRatio(
    void* context, CbLibVideoUpdateAspectRatioCallback callback) {
  g_update_aspect_ratio_callback.Get() =
      callback ? base::Bind(callback, context)
               : base::Bind(&UpdateAspectRatio::DefaultImplementation);
}

void CbLibGraphicsSetContextCreatedCallback(
    void* context, CbLibGraphicsContextCreatedCallback callback) {
  g_graphics_context_created_callback.Get() =
      callback ? base::Bind(callback, context)
               : base::Bind(&GraphicsContextCreated::DefaultImplementation);
}

void CbLibGraphicsSetRenderFrameCallback(
    void* context, CbLibGraphicsRenderFrameCallback callback) {
  g_render_frame_callback.Get() =
      callback ? base::Bind(callback, context)
               : base::Bind(&RenderFrame::DefaultImplementation);
}

intptr_t CbLibGrapicsGetMainTextureHandle() {
  DCHECK(g_external_rasterizer_impl);
  if (!g_external_rasterizer_impl) {
    LOG(WARNING) << __FUNCTION__
                 << "ExternalRasterizer not yet created; unable to progress.";
    return 0;
  }

  return g_external_rasterizer_impl->GetMainTextureHandle();
}

void CbLibGraphicsSetTargetMainTextureSize(
    const CbLibSize& target_render_size) {
  DCHECK(g_external_rasterizer_impl);
  if (!g_external_rasterizer_impl) {
    LOG(WARNING) << __FUNCTION__
                 << "ExternalRasterizer not yet created; unable to progress.";
    return;
  }
  const cobalt::math::Size size = CobaltSizeFromCbLibSize(target_render_size);
  g_external_rasterizer_impl->SetTargetMainTextureSize(size);
}

void CbLibGraphicsRenderCobalt() {
  DCHECK(g_external_rasterizer_impl);
  if (!g_external_rasterizer_impl) {
    LOG(WARNING) << __FUNCTION__
                 << "ExternalRasterizer not yet created; unable to progress.";
    return;
  }
  g_external_rasterizer_impl->RenderCobalt();
}

void CbLibGraphicsCopyBackbuffer(uintptr_t surface, float width_scale) {
  DCHECK(g_external_rasterizer_impl);
  if (!g_external_rasterizer_impl) {
    LOG(WARNING) << __FUNCTION__
                 << "ExternalRasterizer not yet created; unable to progress.";
    return;
  }
  g_external_rasterizer_impl->CopyBackbuffer(surface, width_scale);
}

void CbLibGraphicsSwapBackbuffer() {
  DCHECK(g_external_rasterizer_impl);
  if (!g_external_rasterizer_impl) {
    LOG(WARNING) << __FUNCTION__
                 << "ExternalRasterizer not yet created; unable to progress.";
    return;
  }
  g_external_rasterizer_impl->SwapBackbuffer();
}
