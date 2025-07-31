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

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/thread_checker.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "media/base/renderer.h"
#include "media/gpu/starboard/starboard_gpu_factory_impl.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"
#include "media/mojo/services/gpu_mojo_media_client.h"
#include "media/starboard/starboard_renderer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace media {

class StarboardGpuFactory;

// Simple wrapper around a StarboardRenderer.
// Wraps media::StarboardRenderer to remove its dependence on
// media::mojom::StarboardRendererExtension interface.
// StarboardRendererWrapper in Chrome_InProcGpuThread talks to the
// StarboardRendererClient living in the Chrome_InProcRendererThread
// (Media thread), using `renderer_extension_receiver_`
// and `client_extension_remote_`.
class StarboardRendererWrapper : public Renderer,
                                 public mojom::StarboardRendererExtension {
 public:
  using RendererExtension = mojom::StarboardRendererExtension;
  using ClientExtension = mojom::StarboardRendererClientExtension;

  explicit StarboardRendererWrapper(StarboardRendererTraits traits);

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
  void GetCurrentVideoFrame(GetCurrentVideoFrameCallback callback) override;
#if BUILDFLAG(IS_ANDROID)
  void OnOverlayInfoChanged(const OverlayInfo& overlay_info) override;
#endif  // BUILDFLAG(IS_ANDROID)

  StarboardRenderer* GetRenderer();
  base::SequenceBound<StarboardGpuFactory>* GetGpuFactory();

  void SetRendererForTesting(StarboardRenderer* renderer) {
    test_renderer_ = renderer;
  }

  void SetGpuFactoryForTesting(
      base::SequenceBound<StarboardGpuFactory>* gpu_factory) {
    test_gpu_factory_ = gpu_factory;
  }

 private:
  void OnPaintVideoHoleFrameByStarboard(const gfx::Size& size);
  void OnUpdateStarboardRenderingModeByStarboard(
      const StarboardRenderingMode mode);
#if BUILDFLAG(IS_ANDROID)
  void OnRequestOverlayInfoByStarboard(bool restart_for_transitions);
#endif  // BUILDFLAG(IS_ANDROID)

  void ContinueInitialization(MediaResource* media_resource,
                              RendererClient* client,
                              PipelineStatusCallback init_cb);
  bool IsGpuChannelTokenAvailable() const { return !!command_buffer_id_; }

  mojo::Receiver<RendererExtension> renderer_extension_receiver_;
  mojo::Remote<ClientExtension> client_extension_remote_;
  StarboardRenderer renderer_;
  mojom::CommandBufferIdPtr command_buffer_id_;
  base::SequenceBound<StarboardGpuFactory> gpu_factory_;

  raw_ptr<StarboardRenderer> test_renderer_;
  raw_ptr<base::SequenceBound<StarboardGpuFactory>> test_gpu_factory_;

  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<StarboardRendererWrapper> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_STARBOARD_RENDERER_WRAPPER_H_
