// Copyright 2025 The Cobalt Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COBALT_RENDERER_COBALT_CONTENT_RENDERER_CLIENT_H_
#define COBALT_RENDERER_COBALT_CONTENT_RENDERER_CLIENT_H_

#include "cobalt/media/audio/cobalt_audio_device_factory.h"
#include "components/js_injection/renderer/js_communication.h"
#include "content/public/renderer/content_renderer_client.h"

namespace cobalt {

class CobaltContentRendererClient : public content::ContentRendererClient {
 public:
  CobaltContentRendererClient();
  ~CobaltContentRendererClient() override;

  // ContentRendererClient implementation.
  void RenderFrameCreated(content::RenderFrame* render_frame) override;
  void GetSupportedKeySystems(media::GetSupportedKeySystemsCB cb) override;
  bool IsSupportedAudioType(const media::AudioType& type) override;
  bool IsSupportedVideoType(const media::VideoType& type) override;

  // JS Injection hook
  void RunScriptsAtDocumentStart(content::RenderFrame* render_frame) override;

 private:
  // Registers a custom content::AudioDeviceFactory
  media::CobaltAudioDeviceFactory cobalt_audio_device_factory_;
};

}  // namespace cobalt

#endif  // COBALT_RENDERER_COBALT_CONTENT_RENDERER_CLIENT_H_
