// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_RENDERER_WEB_ENGINE_CONTENT_RENDERER_CLIENT_H_
#define FUCHSIA_WEB_WEBENGINE_RENDERER_WEB_ENGINE_CONTENT_RENDERER_CLIENT_H_

#include <memory>

#include "base/synchronization/waitable_event.h"
#include "base/thread_annotations.h"
#include "build/chromecast_buildflags.h"
#include "content/public/renderer/content_renderer_client.h"
#include "fuchsia_web/webengine/renderer/web_engine_audio_device_factory.h"
#include "fuchsia_web/webengine/renderer/web_engine_render_frame_observer.h"
#include "media/base/supported_video_decoder_config.h"
#include "media/mojo/mojom/fuchsia_media.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

#if BUILDFLAG(ENABLE_CAST_RECEIVER)
namespace cast_streaming {
class ResourceProvider;
}
#endif

namespace memory_pressure {
class MultiSourceMemoryPressureMonitor;
}  // namespace memory_pressure

class WebEngineContentRendererClient : public content::ContentRendererClient {
 public:
  WebEngineContentRendererClient();

  WebEngineContentRendererClient(const WebEngineContentRendererClient&) =
      delete;
  WebEngineContentRendererClient& operator=(
      const WebEngineContentRendererClient&) = delete;

  ~WebEngineContentRendererClient() override;

  // Returns the WebEngineRenderFrameObserver corresponding to
  // |render_frame_id|.
  WebEngineRenderFrameObserver* GetWebEngineRenderFrameObserverForRenderFrameId(
      int render_frame_id) const;

 private:
  // Called by WebEngineRenderFrameObserver when its corresponding RenderFrame
  // is in the process of being deleted.
  void OnRenderFrameDeleted(int render_frame_id);

  // Ensures the MediaCodecProvider is connected and ready to retrieve
  // hardwared-supported video decoder configs.
  void EnsureMediaCodecProvider();

  // Updates the cache of `supported_decoder_configs_` and signal the waiting
  // events.
  void OnGetSupportedVideoDecoderConfigs(
      const media::SupportedVideoDecoderConfigs& configs);

  // Returns true if the specified video format can be decoded on hardware.
  bool IsHardwareSupportedVideoType(const media::VideoType& type);

  // content::ContentRendererClient overrides.
  void RenderThreadStarted() override;
  void RenderFrameCreated(content::RenderFrame* render_frame) override;
  void GetSupportedKeySystems(media::GetSupportedKeySystemsCB cb) override;
  bool IsSupportedVideoType(const media::VideoType& type) override;
  std::unique_ptr<blink::URLLoaderThrottleProvider>
  CreateURLLoaderThrottleProvider(
      blink::URLLoaderThrottleProviderType type) override;
  bool DeferMediaLoad(content::RenderFrame* render_frame,
                      bool has_played_media_before,
                      base::OnceClosure closure) override;
  std::unique_ptr<media::RendererFactory> GetBaseRendererFactory(
      content::RenderFrame* render_frame,
      media::MediaLog* media_log,
      media::DecoderFactory* decoder_factory,
      base::RepeatingCallback<media::GpuVideoAcceleratorFactories*()>
          get_gpu_factories_cb,
      int element_id) override;

#if BUILDFLAG(ENABLE_CAST_RECEIVER)
  std::unique_ptr<cast_streaming::ResourceProvider>
  CreateCastStreamingResourceProvider() override;
#endif

  bool RunClosureWhenInForeground(content::RenderFrame* render_frame,
                                  base::OnceClosure closure);

  // Overrides the default Content/Blink audio pipeline, to allow Renderers to
  // use the AudioConsumer service directly.
  WebEngineAudioDeviceFactory audio_device_factory_;

  // Map of RenderFrame ID to WebEngineRenderFrameObserver.
  std::map<int, std::unique_ptr<WebEngineRenderFrameObserver>>
      render_frame_id_to_observer_map_;

  // Initiates cache purges and Blink/V8 garbage collection when free memory
  // is limited.
  std::unique_ptr<memory_pressure::MultiSourceMemoryPressureMonitor>
      memory_pressure_monitor_;

  // Used to indicate if optional video profile support information has been
  // retrieved from the FuchsiaMediaCodecProvider.
  base::WaitableEvent decoder_configs_event_;

  absl::optional<media::SupportedVideoDecoderConfigs>
      supported_decoder_configs_;

  SEQUENCE_CHECKER(sequence_checker_);
  mojo::Remote<media::mojom::FuchsiaMediaCodecProvider> media_codec_provider_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

#endif  // FUCHSIA_WEB_WEBENGINE_RENDERER_WEB_ENGINE_CONTENT_RENDERER_CLIENT_H_
