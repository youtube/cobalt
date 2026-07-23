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

#ifndef MEDIA_MOJO_SERVICES_STARBOARD_STARBOARD_RENDERER_WRAPPER_H_
#define MEDIA_MOJO_SERVICES_STARBOARD_STARBOARD_RENDERER_WRAPPER_H_

#include <functional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "cobalt/media/service/mojom/video_geometry_setter.mojom.h"
#include "cobalt/media/service/video_geometry_setter_service.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "media/base/renderer.h"
#include "media/gpu/starboard/starboard_gpu_factory_impl.h"
#include "media/mojo/common/starboard/mojo_renderer_bypass_bridge.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"
#include "media/mojo/services/gpu_mojo_media_client.h"
#include "media/starboard/starboard_renderer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "starboard/decode_target.h"

#if BUILDFLAG(IS_ANDROID)
#include "gpu/command_buffer/service/ref_counted_lock.h"
#include "media/base/android_overlay_mojo_factory.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace base {
class TimeDelta;
}  // namespace base

namespace media {

class StarboardGpuFactory;
class ProxyRendererClient;

// Simple wrapper around a StarboardRenderer.
// Wraps media::StarboardRenderer to remove its dependence on
// media::mojom::StarboardRendererExtension interface.
// StarboardRendererWrapper in Chrome_InProcGpuThread talks to the
// StarboardRendererClient living in the Chrome_InProcRendererThread
// (Media thread), using `renderer_extension_receiver_`
// and `client_extension_remote_`.
class StarboardRendererWrapper
    : public Renderer,
      public mojom::StarboardRendererExtension,
#if BUILDFLAG(IS_ANDROID)
      public gpu::RefCountedLockHelperDrDc,
#endif  // BUILDFLAG(IS_ANDROID)
      public cobalt::media::mojom::VideoGeometryChangeClient {
 public:
  using RendererExtension = mojom::StarboardRendererExtension;
  using ClientExtension = mojom::StarboardRendererClientExtension;
  using GetStarboardCommandBufferStubCB = StarboardGpuFactory::GetStubCB;

#if BUILDFLAG(IS_ANDROID)
  StarboardRendererWrapper(StarboardRendererTraits traits,
                           scoped_refptr<gpu::RefCountedLock> drdc_lock);
#else   // BUILDFLAG(IS_ANDROID)
  explicit StarboardRendererWrapper(StarboardRendererTraits traits);
#endif  // BUILDFLAG(IS_ANDROID)

  StarboardRendererWrapper(const StarboardRendererWrapper&) = delete;
  StarboardRendererWrapper& operator=(const StarboardRendererWrapper&) = delete;

  ~StarboardRendererWrapper() override;

  // Renderer implementation.
  void Initialize(MediaResource* media_resource,
                  RendererClient* client,
                  PipelineStatusCallback init_cb) override;
  void Flush(base::OnceClosure flush_cb) override;
  void StartPlayingFrom(base::TimeDelta time) override;
  void SetPlaybackRate(double playback_rate) override;
  void SetVolume(float volume) override;
  void SetCdm(CdmContext* cdm_context, CdmAttachedCB cdm_attached_cb) override;
  void SetLatencyHint(absl::optional<base::TimeDelta> latency_hint) override;
  base::TimeDelta GetMediaTime() override;
  RendererType GetRendererType() override;

  // mojom::StarboardRendererExtension implementation.
  void OnGpuChannelTokenReady(
      mojom::CommandBufferIdPtr command_buffer_id) override;
  void GetCurrentVideoFrame(GetCurrentVideoFrameCallback callback) override;
  void OnSbWindowHandleReady(uint64_t sb_window_handle) override;
  void InitializeWithBypassBridge(
      uint32_t bypass_bridge_id,
      InitializeWithBypassBridgeCallback callback) override;
#if BUILDFLAG(IS_ANDROID)
  void OnOverlayInfoChanged(const OverlayInfo& overlay_info) override;
#endif  // BUILDFLAG(IS_ANDROID)

  bool IsBypassing() const { return !!bypass_bridge_; }

  StarboardRenderer* GetRenderer();
  base::SequenceBound<StarboardGpuFactory>* GetGpuFactory();

  // cobalt::media::mojom::VideoGeometryChangeClient implementation.
  void OnVideoGeometryChange(const gfx::RectF& rect_f,
                             gfx::OverlayTransform transform) override;

  void SetRendererForTesting(StarboardRenderer* renderer) {
    test_renderer_ = renderer;
  }

  void SetGpuFactoryForTesting(
      base::SequenceBound<StarboardGpuFactory>* gpu_factory) {
    test_gpu_factory_ = gpu_factory;
  }

 private:
  void ContinueInitialization(MediaResource* media_resource,
                              RendererClient* client,
                              PipelineStatusCallback init_cb);
  bool IsGpuChannelTokenAvailable() const { return !!command_buffer_id_; }
  void OnPaintVideoHoleFrameByStarboard(const gfx::Size& size);
  void OnUpdateStarboardRenderingModeByStarboard(
      const StarboardRenderingMode mode);
  void OnGetSbWindowHandle();
  void OnSubscribeToVideoGeometryChange(MediaResource* media_resource,
                                        RendererClient* client);
#if BUILDFLAG(IS_ANDROID)
  void OnRequestOverlayInfoByStarboard(bool restart_for_transitions);
#endif  // BUILDFLAG(IS_ANDROID)
  SbDecodeTargetGraphicsContextProvider*
  GetSbDecodeTargetGraphicsContextProvider();
  void GetCurrentDecodeTarget();
  void CreateVideoFrame_OnImageReady(
      VideoPixelFormat format,
      const gfx::Size& coded_size,
      const gfx::Rect& visible_rect,
      const gfx::Size& natural_size,
      scoped_refptr<gpu::ClientSharedImage> shared_image);

  static void GraphicsContextRunner(
      SbDecodeTargetGraphicsContextProvider* graphics_context_provider,
      SbDecodeTargetGlesContextRunnerTarget target_function,
      void* target_function_context);
  void PollMediaTime();
  void StartMediaTimePollingIfNeeded();
  void StopMediaTimePolling();

  mojo::Receiver<RendererExtension> renderer_extension_receiver_;
  mojo::Remote<ClientExtension> client_extension_remote_;
  cobalt::media::VideoGeometrySetterService* video_geometry_setter_service_;
  const base::UnguessableToken overlay_plane_id_;
  mojom::CommandBufferIdPtr command_buffer_id_;
  base::SequenceBound<StarboardGpuFactory> gpu_factory_;
  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;
  // |renderer_| must be declared after |gpu_task_runner_| as |renderer|'s dtor
  // may make use of |gpu_task_runner_|.
  StarboardRenderer renderer_;

  SbDecodeTargetGraphicsContextProvider
      decode_target_graphics_context_provider_ = {};

  bool is_gpu_factory_initialized_ = false;
  scoped_refptr<VideoFrame> current_frame_;
  scoped_refptr<gpu::ClientSharedImage> current_shared_image_;
  std::vector<uint32_t> last_texture_service_ids_;
  SbDecodeTarget decode_target_ = kSbDecodeTargetInvalid;

  scoped_refptr<MojoRendererBypassBridge> bypass_bridge_;
  std::unique_ptr<MediaResource> proxy_media_resource_;
  std::unique_ptr<ProxyRendererClient> proxy_renderer_client_;

  raw_ptr<StarboardRenderer> test_renderer_;
  raw_ptr<base::SequenceBound<StarboardGpuFactory>> test_gpu_factory_;

  mojo::Remote<cobalt::media::mojom::VideoGeometryChangeSubscriber>
      video_geometry_change_subcriber_remote_;
  mojo::Receiver<cobalt::media::mojom::VideoGeometryChangeClient>
      video_geometry_change_client_receiver_{this};

  double playback_rate_ = 0.0;
  base::RepeatingTimer time_update_timer_;
  THREAD_CHECKER(thread_checker_);

  // NOTE: Do not add member variables after weak_factory_
  // It should be the first one destroyed among all members.
  // See base/memory/weak_ptr.h.
  base::WeakPtrFactory<StarboardRendererWrapper> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_STARBOARD_STARBOARD_RENDERER_WRAPPER_H_
