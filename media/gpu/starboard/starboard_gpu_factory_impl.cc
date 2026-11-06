// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include "media/gpu/starboard/starboard_gpu_factory_impl.h"

#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "gpu/command_buffer/service/shared_image/shared_image_factory.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "gpu/ipc/service/shared_image_stub.h"
#include "media/base/media_switches.h"
#include "starboard/extension/protected_video_plane.h"
#include "starboard/system.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_display.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"
#include "ui/gl/scoped_binders.h"
#include "ui/gl/scoped_make_current.h"
#include "ui/gl/scoped_restore_texture.h"

#ifndef GL_TEXTURE_NATIVE_ID_ANGLE
#define GL_TEXTURE_NATIVE_ID_ANGLE 0x3481
#endif

namespace media {
namespace {
bool MakeContextCurrent(gpu::CommandBufferStub* stub) {
  return stub && stub->decoder_context() &&
         stub->decoder_context()->MakeCurrent();
}

#if BUILDFLAG(IS_ANDROID)
const StarboardExtensionProtectedVideoPlaneApi* GetProtectedVideoPlaneApi() {
  static const auto* api =
      static_cast<const StarboardExtensionProtectedVideoPlaneApi*>(
          SbSystemGetExtension(kStarboardExtensionProtectedVideoPlaneName));
  return api;
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace

StarboardGpuFactoryImpl::StarboardGpuFactoryImpl(GetStubCB get_stub_cb)
    : get_stub_cb_(std::move(get_stub_cb)) {
  DETACH_FROM_THREAD(thread_checker_);
}

StarboardGpuFactoryImpl::~StarboardGpuFactoryImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if BUILDFLAG(IS_ANDROID)
  UnregisterTextureHooks();
#endif  // BUILDFLAG(IS_ANDROID)
  if (stub_) {
    stub_->RemoveDestructionObserver(this);
  }
}

void StarboardGpuFactoryImpl::Initialize(base::UnguessableToken channel_token,
                                         int32_t route_id,
                                         base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  channel_token_ = channel_token;
  route_id_ = route_id;
  stub_ = get_stub_cb_.Run(channel_token_, route_id_);
  if (stub_) {
    stub_->AddDestructionObserver(this);
  }
  std::move(callback).Run();
}

// Disable CFI checks for this method because it executes function pointers
// provided by the Starboard library, which cannot be verified across the
// DSO boundary.
NO_SANITIZE("cfi-icall")
void StarboardGpuFactoryImpl::RunSbDecodeTargetFunctionOnGpu(
    SbDecodeTargetGlesContextRunnerTarget target_function,
    void* target_function_context,
    scoped_refptr<GlesClosureRun> run) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(run);
  using Outcome = GlesClosureRun::Outcome;

  // Hold the lock for the whole execution so that a caller which timed out
  // cannot return (and free |target_function_context|) while the closure is
  // running; see StarboardRendererWrapper::GraphicsContextRunner().
  base::AutoLock lock(run->lock);
  if (run->abandoned) {
    run->done.Signal();
    return;
  }

  // The stub is cleared in OnWillDestroyStub(); the channel may since have
  // created a new one for the same route, so try to re-acquire it.
  if (!stub_) {
    stub_ = get_stub_cb_.Run(channel_token_, route_id_);
    if (stub_) {
      stub_->AddDestructionObserver(this);
    }
  }

  if (!stub_) {
    LOG(WARNING) << "RunSbDecodeTargetFunctionOnGpu: no command buffer stub; "
                 << "decode target closure not run.";
    run->outcome = Outcome::kNoStub;
  } else if (!MakeDecodeTargetContextCurrent()) {
    LOG(WARNING) << "RunSbDecodeTargetFunctionOnGpu: could not make the GL "
                 << "context current; decode target closure not run.";
    run->outcome = Outcome::kContextNotCurrent;
  } else {
    target_function(target_function_context);
    run->outcome = Outcome::kRan;
  }
  run->done.Signal();
}

void StarboardGpuFactoryImpl::RunCallbackOnGpu(
    base::OnceCallback<void()> callback,
    base::WaitableEvent* done_event) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Runs SbPlayerGetCurrentFrame(), i.e. SurfaceTexture.updateTexImage(), which
  // must happen on the context the SurfaceTexture is attached to.
  if (MakeDecodeTargetContextCurrent()) {
    std::move(callback).Run();
  }
  done_event->Signal();
}

void StarboardGpuFactoryImpl::PostCallbackToGpu(
    base::OnceCallback<void()> callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Releases decode targets, whose textures live on the decode-target context.
  if (!MakeDecodeTargetContextCurrent()) {
    return;
  }
  std::move(callback).Run();
}

void StarboardGpuFactoryImpl::SetProtectedDecodeTargets(
    SbDecodeTargetGraphicsContextProvider* provider) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if BUILDFLAG(IS_ANDROID)
  if (provider == hooks_provider_) {
    return;
  }
  UnregisterTextureHooks();
  if (!provider || !base::FeatureList::IsEnabled(kCobaltProtectedWebGL)) {
    return;
  }
  const auto* api = GetProtectedVideoPlaneApi();
  if (!api) {
    LOG(ERROR) << "ProtectedVideoPlane extension missing.";
    return;
  }
  hooks_provider_ = provider;
  InstallTextureHooks();
#endif  // BUILDFLAG(IS_ANDROID)
}

void StarboardGpuFactoryImpl::CreateImageOnGpu(
    const gfx::Size& coded_size,
    const gfx::ColorSpace& color_space,
    viz::SharedImageFormat format,
    scoped_refptr<gpu::ClientSharedImage>& shared_image,
    const std::vector<uint32_t>& texture_service_ids,
    const std::vector<uint32_t>& texture_targets,
    uint64_t decode_target,
#if BUILDFLAG(IS_ANDROID)
    scoped_refptr<gpu::RefCountedLock> drdc_lock,
#endif  // BUILDFLAG(IS_ANDROID)
    base::WaitableEvent* done_event) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (MakeContextCurrent(stub_)) {
    DCHECK_EQ(texture_service_ids.size(), texture_targets.size());

    scoped_refptr<gpu::GpuChannelSharedImageInterface>
        gpu_channel_shared_image_interface =
            stub_->channel()->shared_image_stub()->shared_image_interface();

    shared_image = gpu_channel_shared_image_interface
                       ->CreateSharedImageForStarboardGLTexture(
                           format, coded_size, color_space, texture_service_ids,
                           texture_targets, decode_target
#if BUILDFLAG(IS_ANDROID)
                           ,
                           std::move(drdc_lock)
#endif
                       );
  }
  done_event->Signal();
}

void StarboardGpuFactoryImpl::OnWillDestroyStub(bool /*have_context*/) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(stub_);
  stub_ = nullptr;
#if BUILDFLAG(IS_ANDROID)
  // The stub goes away with its context (e.g. lost while backgrounded). Do not
  // keep the protected decode-target context alive past it: while any context
  // of the protected group lives, its (possibly lost) native context does too.
  ResetProtectedContext();
#endif  // BUILDFLAG(IS_ANDROID)
}

bool StarboardGpuFactoryImpl::MakeDecodeTargetContextCurrent() {
#if BUILDFLAG(IS_ANDROID)
  if (hooks_provider_) {
    return MakeProtectedDecodeTargetContextCurrent();
  }
#endif  // BUILDFLAG(IS_ANDROID)
  return MakeContextCurrent(stub_);
}

#if BUILDFLAG(IS_ANDROID)
bool StarboardGpuFactoryImpl::MakeProtectedDecodeTargetContextCurrent() {
  // A DRM player's decode targets live on a surfaceless protected context in
  // the protected virtualization group, so that they share a native context
  // with protected WebGL. It is created on first use, possibly before any
  // WebGL context exists.
  if (!protected_context_) {
    if (!stub_) {
      LOG_IF(ERROR, !logged_no_stub_)
          << "Protected decode-target context: no command buffer stub.";
      logged_no_stub_ = true;
      return false;
    }
    gl::GLDisplay* display =
        stub_->decoder_context()->GetGLContext()
            ? stub_->decoder_context()->GetGLContext()->GetGLDisplayEGL()
            : nullptr;
    if (!display) {
      LOG(ERROR) << "No GL display for the protected decode-target context.";
      return false;
    }
    protected_surface_ =
        gl::init::CreateOffscreenGLSurface(display, gfx::Size());
    if (!protected_surface_ || !protected_surface_->IsSurfaceless()) {
      LOG(ERROR) << "Protected decode-target context needs a surfaceless "
                    "surface.";
      protected_surface_ = nullptr;
      return false;
    }
    // Decoder-style attributes (see GenerateGLContextAttribsForDecoder) so the
    // context agrees with protected WebGL contexts in the same group.
    gl::GLContextAttribs attribs;
    attribs.global_texture_share_group = true;
    attribs.global_semaphore_share_group = true;
    attribs.robust_resource_initialization = true;
    attribs.robust_buffer_access = true;
    attribs.client_major_es_version = 3;
    attribs.client_minor_es_version = 0;
    attribs.protected_content = true;
    attribs.angle_context_virtualization_group_number =
        gl::AngleContextVirtualizationGroup::kCobaltProtected;
    protected_context_ =
        gl::init::CreateGLContext(nullptr, protected_surface_.get(), attribs);
    if (!protected_context_) {
      LOG(ERROR) << "Failed to create the protected decode-target context.";
      protected_surface_ = nullptr;
      return false;
    }
    VLOG(1) << "Created the protected decode-target context.";
  }
  if (protected_context_->MakeCurrent(protected_surface_.get()) &&
      protected_context_->CheckStickyGraphicsResetStatus() == GL_NO_ERROR) {
    return true;
  }
  // Lost (e.g. the GPU dropped its contexts while the app was backgrounded):
  // recreate it once. Textures of the old context are gone with it; decode
  // targets are recreated with their decoder.
  LOG(WARNING) << "Protected decode-target context lost; recreating.";
  ResetProtectedContext();
  if (!recreating_protected_context_) {
    recreating_protected_context_ = true;
    const bool ok = MakeProtectedDecodeTargetContextCurrent();
    recreating_protected_context_ = false;
    return ok;
  }
  return false;
}

void StarboardGpuFactoryImpl::InstallTextureHooks() {
  DCHECK(hooks_provider_);
  const StarboardProtectedDecodeTargetTextureHooks hooks = {
      this,
      &StarboardGpuFactoryImpl::CreateProtectedTextureHook,
      &StarboardGpuFactoryImpl::DeleteProtectedTextureHook,
      &StarboardGpuFactoryImpl::AfterProtectedTextureUpdateHook,
  };
  GetProtectedVideoPlaneApi()->SetDecodeTargetTextureHooks(hooks_provider_,
                                                           &hooks);
}

void StarboardGpuFactoryImpl::ResetProtectedContext() {
  // Decode targets may outlive the context, e.g. while a SharedImage still
  // holds one. ANGLE keeps their textures in the display-wide share group, so
  // delete them now if the context still works; a lost context took the
  // native textures with it.
  if (protected_context_ && !protected_textures_.empty()) {
    ui::ScopedMakeCurrent scoped_make_current(protected_context_.get(),
                                              protected_surface_.get());
    if (scoped_make_current.IsContextCurrent() &&
        protected_context_->CheckStickyGraphicsResetStatus() == GL_NO_ERROR) {
      std::vector<GLuint> textures(protected_textures_.begin(),
                                   protected_textures_.end());
      glDeleteTextures(textures.size(), textures.data());
    }
  }
  protected_textures_.clear();
  protected_context_ = nullptr;
  protected_surface_ = nullptr;
  // Install the hooks anew, so that those decode targets never pass their
  // texture names, which a new context may reuse, back to this factory.
  if (hooks_provider_) {
    InstallTextureHooks();
  }
}

void StarboardGpuFactoryImpl::UnregisterTextureHooks() {
  if (!hooks_provider_) {
    return;
  }
  // Waits for running hooks; none runs after this returns.
  GetProtectedVideoPlaneApi()->SetDecodeTargetTextureHooks(hooks_provider_,
                                                           nullptr);
  hooks_provider_ = nullptr;
  ResetProtectedContext();
}

// static
bool StarboardGpuFactoryImpl::CreateProtectedTextureHook(
    void* context,
    uint32_t* out_texture,
    uint32_t* out_native_texture) {
  return static_cast<StarboardGpuFactoryImpl*>(context)->CreateProtectedTexture(
      out_texture, out_native_texture);
}

// static
void StarboardGpuFactoryImpl::DeleteProtectedTextureHook(void* context,
                                                         uint32_t texture) {
  static_cast<StarboardGpuFactoryImpl*>(context)->DeleteProtectedTexture(
      texture);
}

// static
void StarboardGpuFactoryImpl::AfterProtectedTextureUpdateHook(void* context,
                                                              uint32_t texture,
                                                              int width,
                                                              int height) {
  static_cast<StarboardGpuFactoryImpl*>(context)->AfterProtectedTextureUpdate(
      texture, width, height);
}

bool StarboardGpuFactoryImpl::CreateProtectedTexture(
    uint32_t* out_texture,
    uint32_t* out_native_texture) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(protected_context_ && protected_context_->IsCurrent(nullptr));
  if (!gl::g_current_gl_driver->ext.b_GL_ANGLE_texture_external_update) {
    LOG(ERROR) << "GL_ANGLE_texture_external_update is required.";
    return false;
  }
  GLuint texture = 0;
  glGenTextures(1, &texture);
  gl::ScopedTextureBinder binder(GL_TEXTURE_EXTERNAL_OES, texture);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  // SurfaceTexture attaches to the driver's name, not ANGLE's.
  GLint native_texture = 0;
  glGetTexParameteriv(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_NATIVE_ID_ANGLE,
                      &native_texture);
  if (!native_texture) {
    LOG(ERROR) << "No native id for decode-target texture " << texture;
    glDeleteTextures(1, &texture);
    return false;
  }
  protected_textures_.insert(texture);
  *out_texture = texture;
  *out_native_texture = static_cast<uint32_t>(native_texture);
  return true;
}

void StarboardGpuFactoryImpl::DeleteProtectedTexture(uint32_t texture) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!protected_textures_.erase(texture)) {
    return;
  }
  // Typically runs with an unprotected context current (SharedImage
  // teardown). ANGLE shares texture names across both, but the native texture
  // lives in the protected native context.
  ui::ScopedMakeCurrent scoped_make_current(protected_context_.get(),
                                            protected_surface_.get());
  if (!scoped_make_current.IsContextCurrent()) {
    return;
  }
  GLuint name = texture;
  glDeleteTextures(1, &name);
}

void StarboardGpuFactoryImpl::AfterProtectedTextureUpdate(uint32_t texture,
                                                          int width,
                                                          int height) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // As AbstractTextureAndroid::BindToServiceId and SurfaceTexture do for
  // Chromium's own SurfaceTextures: give ANGLE the image size (it cannot query
  // the driver for it) and invalidate its cached state, with the texture bound.
  GLint previous = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_EXTERNAL_OES, &previous);
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, texture);
  if (width > 0 && height > 0) {
    glTexImage2DExternalANGLE(GL_TEXTURE_EXTERNAL_OES, /*level=*/0, GL_RGBA,
                              width, height, /*border=*/0, GL_RGBA,
                              GL_UNSIGNED_BYTE);
  }
  glInvalidateTextureANGLE(GL_TEXTURE_EXTERNAL_OES);
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, previous);
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace media
