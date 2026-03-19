// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_RENDER_FRAME_AUDIO_INPUT_STREAM_FACTORY_CORE_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_RENDER_FRAME_AUDIO_INPUT_STREAM_FACTORY_CORE_H_

#include <map>
#include <utility>

#include "base/synchronization/lock.h"
#include "content/browser/renderer_host/media/render_frame_audio_input_stream_factory.h"
#include "content/browser/media/forwarding_audio_stream_factory.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_processing.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/media/renderer_audio_input_stream_factory.mojom.h"

namespace content {

struct MediaDeviceSaltAndOrigin;
class WebContentsMediaCaptureId;

class RenderFrameAudioInputStreamFactory::Core final
    : public blink::mojom::RendererAudioInputStreamFactory {
 public:
  static Core* Get(int render_process_id, int render_frame_id);

  Core(mojo::PendingReceiver<blink::mojom::RendererAudioInputStreamFactory>
           receiver,
       MediaStreamManager* media_stream_manager,
       RenderFrameHost* render_frame_host);

  Core(const Core&) = delete;
  Core& operator=(const Core&) = delete;

  ~Core() final;

  void Init(mojo::PendingReceiver<blink::mojom::RendererAudioInputStreamFactory>
                receiver);

  // mojom::RendererAudioInputStreamFactory implementation.
  void CreateStream(
      mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
          client,
      const base::UnguessableToken& session_id,
      const media::AudioParameters& audio_params,
      bool automatic_gain_control,
      uint32_t shared_memory_count,
      media::mojom::AudioProcessingConfigPtr processing_config) final;

  void AssociateInputAndOutputForAec(
      const base::UnguessableToken& input_stream_id,
      const std::string& output_device_id) final;

  void CreateLoopbackStream(
      mojo::PendingRemote<blink::mojom::RendererAudioInputStreamFactoryClient>
          client,
      const media::AudioParameters& audio_params,
      uint32_t shared_memory_count,
      bool disable_local_echo,
      AudioStreamBroker::LoopbackSource* loopback_source);

  void AssociateInputAndOutputForAecAfterCheckingAccess(
      const base::UnguessableToken& input_stream_id,
      const std::string& output_device_id,
      MediaDeviceSaltAndOrigin salt_and_origin,
      bool access_granted);

  void AssociateTranslatedOutputDeviceForAec(
      const base::UnguessableToken& input_stream_id,
      const std::string& raw_output_device_id);

  const raw_ptr<MediaStreamManager> media_stream_manager_;
  const int process_id_;
  const int frame_id_;
  const url::Origin origin_;

  mojo::Receiver<blink::mojom::RendererAudioInputStreamFactory> receiver_{this};
  // Always null-check this weak pointer before dereferencing it.
  base::WeakPtr<ForwardingAudioStreamFactory::Core> forwarding_factory_;

  base::WeakPtrFactory<Core> weak_ptr_factory_{this};

  static base::Lock& GetLock();
  using CoreMap = std::map<std::pair<int, int>, Core*>;
  static CoreMap& GetMap();
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_RENDER_FRAME_AUDIO_INPUT_STREAM_FACTORY_CORE_H_
