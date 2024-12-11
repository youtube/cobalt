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

#ifndef MEDIA_STARBOARD_STARBOARD_RENDERER_FACTORY_H_
#define MEDIA_STARBOARD_STARBOARD_RENDERER_FACTORY_H_

#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "media/base/media_log.h"
#include "media/base/renderer_factory.h"

namespace media {

// Creates Renderers using Starboard.
class MEDIA_EXPORT StarboardRendererFactory final : public RendererFactory {
 public:
  explicit StarboardRendererFactory(
      MediaLog* media_log,
      base::TimeDelta audio_write_duration_local,
      base::TimeDelta audio_write_duration_remote);

  StarboardRendererFactory(const StarboardRendererFactory&) = delete;
  StarboardRendererFactory& operator=(const StarboardRendererFactory&) = delete;

  ~StarboardRendererFactory() final;

  // RendererFactory implementation.
  std::unique_ptr<Renderer> CreateRenderer(
      const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
      const scoped_refptr<base::TaskRunner>& worker_task_runner,
      AudioRendererSink* audio_renderer_sink,
      VideoRendererSink* video_renderer_sink,
      RequestOverlayInfoCB request_overlay_info_cb,
      const gfx::ColorSpace& target_color_space) final;

 private:
  raw_ptr<MediaLog> media_log_;
  const base::TimeDelta audio_write_duration_local_;
  const base::TimeDelta audio_write_duration_remote_;
};

}  // namespace media

#endif  // MEDIA_STARBOARD_STARBOARD_RENDERER_FACTORY_H_
