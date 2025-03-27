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

#include "media/mojo/clients/starboard/starboard_renderer_client.h"

#include "base/functional/bind.h"
#include "media/base/media_log.h"
#include "media/base/media_resource.h"
#include "media/base/renderer_client.h"
#include "media/base/video_renderer_sink.h"
#include "media/mojo/clients/mojo_renderer.h"
#include "media/renderers/video_overlay_factory.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace media {

StarboardRendererClient::StarboardRendererClient(
    const scoped_refptr<base::SequencedTaskRunner>& media_task_runner,
    std::unique_ptr<MediaLog> media_log,
    std::unique_ptr<MojoRenderer> mojo_renderer,
    std::unique_ptr<VideoOverlayFactory> video_overlay_factory,
    VideoRendererSink* video_renderer_sink,
    mojo::PendingRemote<RendererExtension> pending_renderer_extension,
    mojo::PendingReceiver<ClientExtension> client_extension_receiver,
    BindHostReceiverCallback bind_host_receiver_callback)
    : media_task_runner_(media_task_runner),
      media_log_(std::move(media_log)),
      MojoRendererWrapper(std::move(mojo_renderer)),
      video_overlay_factory_(std::move(video_overlay_factory)),
      video_renderer_sink_(video_renderer_sink),
      pending_renderer_extension_(std::move(pending_renderer_extension)),
      pending_client_extension_receiver_(std::move(client_extension_receiver)),
      client_extension_receiver_(this),
      bind_host_receiver_callback_(bind_host_receiver_callback) {
  DCHECK(media_task_runner_);
  DCHECK(video_renderer_sink_);
  DCHECK(video_overlay_factory_);
  DCHECK(bind_host_receiver_callback_);
  LOG(INFO) << "StarboardRendererClient constructed.";
}

StarboardRendererClient::~StarboardRendererClient() {}

void StarboardRendererClient::Initialize(
    media::MediaResource* media_resource,
    media::RendererClient* client,
    media::PipelineStatusCallback init_cb) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());

  // Consume and bind the delayed PendingRemote and PendingReceiver now that
  // we are on |media_task_runner_|.
  renderer_extension_.Bind(std::move(pending_renderer_extension_),
                           media_task_runner_);
  client_extension_receiver_.Bind(std::move(pending_client_extension_receiver_),
                                  media_task_runner_);

  renderer_extension_.set_disconnect_handler(base::BindOnce(
      &StarboardRendererClient::OnConnectionError, base::Unretained(this)));

  client_ = client;

  // Bind the receiver of VideoGeometryChangeSubscriber on renderer Thread.
  // This uses BindPostTaskToCurrentDefault() to ensure the callback is ran
  // on renderer thread, not media thread.
  bind_host_receiver_callback_.Run(
      video_geometry_change_subcriber_remote_.BindNewPipeAndPassReceiver());
  DCHECK(video_geometry_change_subcriber_remote_);
  video_geometry_change_subcriber_remote_->SubscribeToVideoGeometryChange(
      video_overlay_factory_->overlay_plane_id(),
      video_geometry_change_client_receiver_.BindNewPipeAndPassRemote(),
      base::BindOnce(&StarboardRendererClient::OnSubscribeToVideoGeometryChange,
                     base::Unretained(this), media_resource, client));

  MojoRendererWrapper::Initialize(media_resource, client, std::move(init_cb));
}

RendererType StarboardRendererClient::GetRendererType() {
  return RendererType::kStarboard;
}

void StarboardRendererClient::PaintVideoHoleFrame(const gfx::Size& size) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  video_renderer_sink_->PaintSingleFrame(
      video_overlay_factory_->CreateFrame(size));
}

void StarboardRendererClient::OnVideoGeometryChange(
    const gfx::RectF& rect_f,
    gfx::OverlayTransform /* transform */) {
  gfx::Rect new_bounds = gfx::ToEnclosingRect(rect_f);
  renderer_extension_->OnVideoGeometryChange(new_bounds);
}

void StarboardRendererClient::OnConnectionError() {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
  MEDIA_LOG(ERROR, media_log_) << "StarboardRendererClient disconnected";
  client_->OnError(PIPELINE_ERROR_DISCONNECTED);
}

void StarboardRendererClient::OnSubscribeToVideoGeometryChange(
    MediaResource* media_resource,
    RendererClient* client) {
  DCHECK(media_task_runner_->RunsTasksInCurrentSequence());
}

}  // namespace media
