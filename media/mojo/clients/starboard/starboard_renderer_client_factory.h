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

#ifndef MEDIA_MOJO_CLIENTS_STARBOARD_STARBOARD_RENDERER_CLIENT_FACTORY_H_
#define MEDIA_MOJO_CLIENTS_STARBOARD_STARBOARD_RENDERER_CLIENT_FACTORY_H_

#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/renderer_factory.h"
#include "media/base/starboard/renderer_factory_traits.h"
#include "media/starboard/bind_host_receiver_callback.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace media {
class MediaLog;
class MojoRendererFactory;

// Creates StarboardRendererClient and binds
// media::mojom::StarboardRendererExtension and
// mojom::StarboardRendererClientExtension to StarboardRenderer.
class MEDIA_EXPORT StarboardRendererClientFactory final
    : public RendererFactory {
 public:
  StarboardRendererClientFactory(
      MediaLog* media_log,
      std::unique_ptr<MojoRendererFactory> mojo_renderer_factory,
      const media::RendererFactoryTraits* traits);

  StarboardRendererClientFactory(const StarboardRendererClientFactory&) =
      delete;
  StarboardRendererClientFactory& operator=(
      const StarboardRendererClientFactory&) = delete;

  ~StarboardRendererClientFactory() final;

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
  std::unique_ptr<MojoRendererFactory> mojo_renderer_factory_;
  const base::TimeDelta audio_write_duration_local_;
  const base::TimeDelta audio_write_duration_remote_;
  const BindHostReceiverCallback bind_host_receiver_callback_;
};

}  // namespace media

#endif  // MEDIA_MOJO_CLIENTS_STARBOARD_STARBOARD_RENDERER_CLIENT_FACTORY_H_
