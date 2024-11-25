// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/renderer_factory.h"

namespace media {

RendererFactory::RendererFactory() = default;

RendererFactory::~RendererFactory() = default;

#if BUILDFLAG(USE_STARBOARD_MEDIA)
std::unique_ptr<Renderer> RendererFactory::CreateRenderer(
    const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    AudioRendererSink* audio_renderer_sink,
    VideoRendererSink* video_renderer_sink,
    RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space,
    base::TimeDelta audio_write_duration_local,
    base::TimeDelta audio_write_duration_remote) {
  return nullptr;
}
#endif  // BUILDFLAG(USE_STARBOARD_MEDIA)

MediaResource::Type RendererFactory::GetRequiredMediaResourceType() {
  return MediaResource::Type::STREAM;
}

}  // namespace media
