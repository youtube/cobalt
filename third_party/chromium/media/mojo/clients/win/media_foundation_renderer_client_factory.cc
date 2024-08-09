// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromium/media/mojo/clients/win/media_foundation_renderer_client_factory.h"

#include "third_party/chromium/media/base/win/dcomp_texture_wrapper.h"
#include "third_party/chromium/media/base/win/mf_helpers.h"
#include "third_party/chromium/media/mojo/clients/mojo_media_log_service.h"
#include "third_party/chromium/media/mojo/clients/mojo_renderer.h"
#include "third_party/chromium/media/mojo/clients/mojo_renderer_factory.h"
#include "third_party/chromium/media/mojo/clients/win/media_foundation_renderer_client.h"
#include "third_party/chromium/media/mojo/mojom/renderer_extensions.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace media_m96 {

MediaFoundationRendererClientFactory::MediaFoundationRendererClientFactory(
    MediaLog* media_log,
    GetDCOMPTextureWrapperCB get_dcomp_texture_cb,
    std::unique_ptr<media_m96::MojoRendererFactory> mojo_renderer_factory)
    : media_log_(media_log),
      get_dcomp_texture_cb_(std::move(get_dcomp_texture_cb)),
      mojo_renderer_factory_(std::move(mojo_renderer_factory)) {
  DVLOG_FUNC(1);
}

MediaFoundationRendererClientFactory::~MediaFoundationRendererClientFactory() {
  DVLOG_FUNC(1);
}

std::unique_ptr<media_m96::Renderer>
MediaFoundationRendererClientFactory::CreateRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& /*worker_task_runner*/,
    media_m96::AudioRendererSink* /*audio_renderer_sink*/,
    media_m96::VideoRendererSink* video_renderer_sink,
    media_m96::RequestOverlayInfoCB /*request_overlay_info_cb*/,
    const gfx::ColorSpace& /*target_color_space*/) {
  DVLOG_FUNC(1);

  // Use `mojo::MakeSelfOwnedReceiver` for MediaLog so logs may go through even
  // after `this` is destructed. `Clone()` is necessary since the remote could
  // live longer than `media_log_`.
  mojo::PendingReceiver<mojom::MediaLog> media_log_pending_receiver;
  auto media_log_pending_remote =
      media_log_pending_receiver.InitWithNewPipeAndPassRemote();
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<MojoMediaLogService>(media_log_->Clone()),
      std::move(media_log_pending_receiver));

  // Used to send messages from the MediaFoundationRendererClient (Renderer
  // process), to the MediaFoundationRenderer (MF_CDM LPAC Utility process).
  // The |renderer_extension_receiver| will be bound in MediaFoundationRenderer.
  mojo::PendingRemote<media_m96::mojom::MediaFoundationRendererExtension>
      renderer_extension_remote;
  auto renderer_extension_receiver =
      renderer_extension_remote.InitWithNewPipeAndPassReceiver();

  auto dcomp_texture = get_dcomp_texture_cb_.Run();
  DCHECK(dcomp_texture);

  std::unique_ptr<media_m96::MojoRenderer> mojo_renderer =
      mojo_renderer_factory_->CreateMediaFoundationRenderer(
          std::move(media_log_pending_remote),
          std::move(renderer_extension_receiver), media_task_runner,
          video_renderer_sink);

  // mojo_renderer's ownership is passed to MediaFoundationRendererClient.
  return std::make_unique<MediaFoundationRendererClient>(
      media_task_runner, media_log_->Clone(), std::move(mojo_renderer),
      std::move(renderer_extension_remote), std::move(dcomp_texture),
      video_renderer_sink);
}

media_m96::MediaResource::Type
MediaFoundationRendererClientFactory::GetRequiredMediaResourceType() {
  return media_m96::MediaResource::Type::STREAM;
}

}  // namespace media_m96