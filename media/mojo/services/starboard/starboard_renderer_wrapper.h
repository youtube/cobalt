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

#ifndef MEDIA_MOJO_SERVICES_STARBOARD_RENDERER_WRAPPER_H_
#define MEDIA_MOJO_SERVICES_STARBOARD_RENDERER_WRAPPER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "media/base/renderer.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"
#include "media/mojo/services/gpu_mojo_media_client.h"
#include "media/starboard/starboard_renderer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "starboard/decode_target.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/threading/sequence_bound.h"
#include "gpu/command_buffer/service/starboard/starboard_surface_texture_gl_owner.h"
#include "gpu/command_buffer/service/texture_owner.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "media/gpu/android/codec_buffer_wait_coordinator.h"
#include "media/gpu/android/shared_image_video_provider.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace base {
class TimeDelta;
}  // namespace base

namespace media {

#if BUILDFLAG(IS_ANDROID)
class StarboardGpuFactory;
#endif  // BUILDFLAG(IS_ANDROID)

// Simple wrapper around a StarboardRenderer.
// Wraps media::StarboardRenderer to remove its dependence on
// media::mojom::StarboardRendererExtension interface.
// StarboardRendererWrapper in Chrome_InProcGpuThread talks to the
// StarboardRendererClient living in the Chrome_InProcRendererThread
// (Media thread), using `renderer_extension_receiver_`
// and `client_extension_remote_`.
class StarboardRendererWrapper final
    : public Renderer,
      public mojom::StarboardRendererExtension {
 public:
  using RendererExtension = mojom::StarboardRendererExtension;
  using ClientExtension = mojom::StarboardRendererClientExtension;
#if BUILDFLAG(IS_ANDROID)
  using InitCB =
      base::RepeatingCallback<void(scoped_refptr<gpu::TextureOwner>,
                                   base::UnguessableToken channel_token,
                                   int32_t route_id)>;
#endif  // BUILDFLAG(IS_ANDROID)

  StarboardRendererWrapper(StarboardRendererTraits& traits);

  StarboardRendererWrapper(const StarboardRendererWrapper&) = delete;
  StarboardRendererWrapper& operator=(const StarboardRendererWrapper&) = delete;

  ~StarboardRendererWrapper() override;

  // Renderer implementation.
  void Initialize(MediaResource* media_resource,
                  RendererClient* client,
                  PipelineStatusCallback init_cb) override;
  void SetCdm(CdmContext* cdm_context, CdmAttachedCB cdm_attached_cb) override;
  void SetLatencyHint(absl::optional<base::TimeDelta> latency_hint) override;
  void Flush(base::OnceClosure flush_cb) override;
  void StartPlayingFrom(base::TimeDelta time) override;
  void SetPlaybackRate(double playback_rate) override;
  void SetVolume(float volume) override;
  base::TimeDelta GetMediaTime() override;
  RendererType GetRendererType() override;

  // mojom::StarboardRendererExtension implementation.
  void OnVideoGeometryChange(const gfx::Rect& output_rect) override;
  void OnGpuChannelTokenReady(
      mojom::CommandBufferIdPtr command_buffer_id) override;
  void Render(RenderCallback callback) override;

#if BUILDFLAG(IS_ANDROID)
  static scoped_refptr<gpu::TextureOwner> CreateTextureOwner(
      scoped_refptr<gpu::SharedContextState> context_state);
#endif  // BUILDFLAG(IS_ANDROID)

 private:
  void OnPaintVideoHoleFrameByStarboard(const gfx::Size& size);
  void OnUpdateStarboardRenderingModeByStarboard(
      const StarboardRenderingMode mode);
#if BUILDFLAG(IS_ANDROID)
  void OnGpuFactoryInitialized();
  void OnTextureOwnerInitialized(scoped_refptr<gpu::TextureOwner> texture_owner,
                                 base::UnguessableToken channel_token,
                                 int32_t route_id);
  void CreateVideoFrame_OnImageReady(
      base::WeakPtr<StarboardRendererWrapper> thiz,
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      SharedImageVideoProvider::ImageRecord record);
#endif  // BUILDFLAG(IS_ANDROID)
  SbDecodeTargetGraphicsContextProvider*
  GetSbDecodeTargetGraphicsContextProvider();
  void GetCurrentDecodeTarget();

  static void GraphicsContextRunner(
      SbDecodeTargetGraphicsContextProvider* graphics_context_provider,
      SbDecodeTargetGlesContextRunnerTarget target_function,
      void* target_function_context);

  mojo::Receiver<RendererExtension> renderer_extension_receiver_;
  mojo::Remote<ClientExtension> client_extension_remote_;
  std::unique_ptr<StarboardRenderer> renderer_;
  mojom::CommandBufferIdPtr command_buffer_id_;
#if BUILDFLAG(IS_ANDROID)
  base::SequenceBound<StarboardGpuFactory> gpu_factory_;
  scoped_refptr<gpu::TextureOwner> texture_owner_;
  scoped_refptr<gpu::RefCountedLock> ref_counted_lock_;
  // |codec_buffer_wait_coordinator_| owns the TextureOwner.
  scoped_refptr<CodecBufferWaitCoordinator> codec_buffer_wait_coordinator_;
#endif  // BUILDFLAG(IS_ANDROID)
  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;

  bool is_decode_to_texture_preferred_ = false;
  raw_ptr<RendererClient> client_ = nullptr;
  raw_ptr<MediaResource> media_resource_ = nullptr;
  PipelineStatusCallback init_cb_;

  SbDecodeTargetGraphicsContextProvider
      decode_target_graphics_context_provider_;

#if BUILDFLAG(IS_ANDROID)
  bool is_gpu_factory_initialized_ = false;
  base::RepeatingCallback<gpu::CommandBufferStub*(base::UnguessableToken,
                                                  int32_t)>
      get_starboard_command_buffer_stub_cb_;
  std::unique_ptr<SharedImageVideoProvider> image_provider_;
  scoped_refptr<VideoFrame> current_frame_;
  int texture_native_id_ = -1;
#endif  // BUILDFLAG(IS_ANDROID)
  SbDecodeTarget decode_target_ = kSbDecodeTargetInvalid;

  base::WeakPtrFactory<StarboardRendererWrapper> weak_factory_{this};

  THREAD_CHECKER(thread_checker_);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_STARBOARD_RENDERER_WRAPPER_H_
