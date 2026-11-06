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

#ifndef MEDIA_GPU_STARBOARD_STARBOARD_GPU_FACTORY_IMPL_H_
#define MEDIA_GPU_STARBOARD_STARBOARD_GPU_FACTORY_IMPL_H_

#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_checker.h"
#include "media/gpu/starboard/starboard_gpu_factory.h"

namespace media {
// Implement StarboardGpuFactory class.
class StarboardGpuFactoryImpl : public StarboardGpuFactory {
 public:
  explicit StarboardGpuFactoryImpl(GetStubCB get_stub_cb);

  StarboardGpuFactoryImpl(const StarboardGpuFactoryImpl&) = delete;
  StarboardGpuFactoryImpl& operator=(const StarboardGpuFactoryImpl&) = delete;

  ~StarboardGpuFactoryImpl() override;

  void Initialize(base::UnguessableToken channel_token,
                  int32_t route_id,
                  base::OnceClosure callback) override;
  void RunSbDecodeTargetFunctionOnGpu(
      SbDecodeTargetGlesContextRunnerTarget target_function,
      void* target_function_context,
      scoped_refptr<GlesClosureRun> run) override;
  void RunCallbackOnGpu(base::OnceCallback<void()> callback,
                        base::WaitableEvent* done_event) override;
  void PostCallbackToGpu(base::OnceCallback<void()> callback) override;
  void SetProtectedDecodeTargets(
      SbDecodeTargetGraphicsContextProvider* provider) override;
  void CreateImageOnGpu(const gfx::Size& coded_size,
                        const gfx::ColorSpace& color_space,
                        viz::SharedImageFormat format,
                        scoped_refptr<gpu::ClientSharedImage>& shared_image,
                        const std::vector<uint32_t>& texture_service_ids,
                        const std::vector<uint32_t>& texture_targets,
                        uint64_t decode_target,
#if BUILDFLAG(IS_ANDROID)
                        scoped_refptr<gpu::RefCountedLock> drdc_lock,
#endif  // BUILDFLAG(IS_ANDROID)
                        base::WaitableEvent* done_event) override;

 private:
  void OnWillDestroyStub(bool have_context) override;

  // Makes the decode targets' context current: the media stub's context, or
  // the protected decode-target context for DRM players.
  bool MakeDecodeTargetContextCurrent();
#if BUILDFLAG(IS_ANDROID)
  // Makes the protected decode-target context current, creating it on first
  // use and recreating it once if it was lost.
  bool MakeProtectedDecodeTargetContextCurrent();
  void InstallTextureHooks();
  // Deletes the protected context and its textures, and reinstalls the hooks
  // so that older decode targets no longer reach this factory.
  void ResetProtectedContext();
  void UnregisterTextureHooks();

  // StarboardProtectedDecodeTargetTextureHooks, with |context| being this.
  static bool CreateProtectedTextureHook(void* context,
                                         uint32_t* out_texture,
                                         uint32_t* out_native_texture);
  static void DeleteProtectedTextureHook(void* context, uint32_t texture);
  static void AfterProtectedTextureUpdateHook(void* context,
                                              uint32_t texture,
                                              int width,
                                              int height);
  bool CreateProtectedTexture(uint32_t* out_texture,
                              uint32_t* out_native_texture);
  void DeleteProtectedTexture(uint32_t texture);
  void AfterProtectedTextureUpdate(uint32_t texture, int width, int height);
#endif  // BUILDFLAG(IS_ANDROID)

  GetStubCB get_stub_cb_;

  base::UnguessableToken channel_token_;
  int32_t route_id_ = 0;

  raw_ptr<gpu::CommandBufferStub> stub_ = nullptr;

#if BUILDFLAG(IS_ANDROID)
  // The provider whose protected decode targets this factory renders (DRM
  // players only), or nullptr; see SetProtectedDecodeTargets().
  raw_ptr<SbDecodeTargetGraphicsContextProvider> hooks_provider_ = nullptr;
  // Surfaceless protected context for those decode targets, and the textures
  // created on it.
  scoped_refptr<gl::GLSurface> protected_surface_;
  scoped_refptr<gl::GLContext> protected_context_;
  base::flat_set<uint32_t> protected_textures_;
  bool recreating_protected_context_ = false;
  bool logged_no_stub_ = false;
#endif  // BUILDFLAG(IS_ANDROID)

  THREAD_CHECKER(thread_checker_);
};

}  // namespace media

#endif  // MEDIA_GPU_STARBOARD_STARBOARD_GPU_FACTORY_IMPL_H_
