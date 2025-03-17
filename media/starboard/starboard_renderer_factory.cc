// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "media/starboard/starboard_renderer_factory.h"

#include "base/check.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "media/renderers/video_overlay_factory.h"
#include "media/starboard/starboard_renderer.h"

namespace media {

StarboardRendererFactory::StarboardRendererFactory(
    MediaLog* media_log,
    base::TimeDelta audio_write_duration_local,
    base::TimeDelta audio_write_duration_remote,
    BindHostReceiverCallback bind_host_receiver_callback)
    : media_log_(media_log),
      audio_write_duration_local_(audio_write_duration_local),
      audio_write_duration_remote_(audio_write_duration_remote),
      bind_host_receiver_callback_(bind_host_receiver_callback) {}

StarboardRendererFactory::~StarboardRendererFactory() = default;

std::unique_ptr<Renderer> StarboardRendererFactory::CreateRenderer(
    const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    AudioRendererSink* audio_renderer_sink,
    VideoRendererSink* video_renderer_sink,
    RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space) {
  DCHECK(video_renderer_sink);
  DCHECK(media_log_);
  DCHECK(bind_host_receiver_callback_);
  auto overlay_factory = std::make_unique<::media::VideoOverlayFactory>();
  return std::make_unique<media::StarboardRenderer>(
      media_task_runner, video_renderer_sink, media_log_,
      std::move(overlay_factory), audio_write_duration_local_,
      audio_write_duration_remote_, bind_host_receiver_callback_);
}

}  // namespace media
