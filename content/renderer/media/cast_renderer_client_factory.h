// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CAST_RENDERER_CLIENT_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_CAST_RENDERER_CLIENT_FACTORY_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/renderer.h"
#include "media/base/renderer_factory.h"
#include "ui/gfx/color_space.h"

namespace media {
class MojoRendererFactory;
class MediaLog;
}  // namespace media

namespace content {

// Creates a renderer for chromecast.
// This class creates a cast specific MojoRenderer from a MojoRendererFactory,
// and wraps it within a DecryptingRenderer.
class CastRendererClientFactory : public media::RendererFactory {
 public:
  CastRendererClientFactory(
      media::MediaLog* media_log,
      std::unique_ptr<media::MojoRendererFactory> mojo_renderer_factory);

  CastRendererClientFactory(const CastRendererClientFactory&) = delete;
  CastRendererClientFactory& operator=(const CastRendererClientFactory&) =
      delete;

  ~CastRendererClientFactory() override;

  std::unique_ptr<media::Renderer> CreateRenderer(
      const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
      const scoped_refptr<base::TaskRunner>& worker_task_runner,
      media::AudioRendererSink* audio_renderer_sink,
      media::VideoRendererSink* video_renderer_sink,
      media::RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space) override;

 private:
  media::MediaLog* media_log_;
  std::unique_ptr<media::MojoRendererFactory> mojo_renderer_factory_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CAST_RENDERER_CLIENT_FACTORY_H_
