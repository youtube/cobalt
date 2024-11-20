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
#include "media/starboard/starboard_renderer.h"

namespace media {

StarboardRendererFactory::StarboardRendererFactory(MediaLog* media_log)
    : media_log_(media_log) {}

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
  return std::make_unique<media::StarboardRenderer>(
      media_task_runner, video_renderer_sink, media_log_);
}

}  // namespace media
