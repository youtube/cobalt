// Copyright 2025 The Cobalt Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_RENDERER_COBALT_CONTENT_RENDERER_CLIENT_H_
#define COBALT_RENDERER_COBALT_CONTENT_RENDERER_CLIENT_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "cobalt/media/audio/cobalt_audio_device_factory.h"
#include "content/public/renderer/content_renderer_client.h"
#include "media/base/starboard/renderer_factory_traits.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace media {
class MediaLog;
class RendererFactory;
}  // namespace media

namespace mojo {
class GenericPendingReceiver;
}  // namespace mojo

namespace cobalt {
// This class utilizes embedder API for participating in renderer logic.
// It allows Cobalt to customize content Renderer module.
class CobaltContentRendererClient : public content::ContentRendererClient {
 public:
  CobaltContentRendererClient();

  CobaltContentRendererClient(const CobaltContentRendererClient&) = delete;
  CobaltContentRendererClient& operator=(const CobaltContentRendererClient&) =
      delete;

  ~CobaltContentRendererClient() override;

  // ContentRendererClient implementation.
  void RenderFrameCreated(content::RenderFrame* render_frame) override;
  void GetSupportedKeySystems(::media::GetSupportedKeySystemsCB cb) override;
  bool IsSupportedAudioType(const ::media::AudioType& type) override;
  bool IsSupportedVideoType(const ::media::VideoType& type) override;
  // JS Injection hook
  void RunScriptsAtDocumentStart(content::RenderFrame* render_frame) override;
  void GetStarboardRendererFactoryTraits(
      media::RendererFactoryTraits* traits) override;

  // Bind Host Receiver to VideoGeometryChangeSubscriber on Browser thread.
  // This is called from StarboardRenderer with |BindPostTaskToCurrentDefault|
  // on media thread to post the task on Renderer thread.
  void BindHostReceiver(mojo::GenericPendingReceiver receiver);

 private:
  // Registers a custom content::AudioDeviceFactory
  ::media::CobaltAudioDeviceFactory cobalt_audio_device_factory_;

  base::WeakPtrFactory<CobaltContentRendererClient> weak_factory_{this};

  THREAD_CHECKER(thread_checker_);
};

}  // namespace cobalt

#endif  // COBALT_RENDERER_COBALT_CONTENT_RENDERER_CLIENT_H_
