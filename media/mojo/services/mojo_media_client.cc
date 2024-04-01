// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_media_client.h"

#include "base/single_thread_task_runner.h"
#include "media/base/audio_decoder.h"
#include "media/base/cdm_factory.h"
#include "media/base/media_log.h"
#include "media/base/renderer.h"
#include "media/base/video_decoder.h"

namespace media {

MojoMediaClient::MojoMediaClient() = default;

MojoMediaClient::~MojoMediaClient() = default;

void MojoMediaClient::Initialize() {}

std::unique_ptr<AudioDecoder> MojoMediaClient::CreateAudioDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  return nullptr;
}

void MojoMediaClient::GetSupportedVideoDecoderConfigs(
    MojoMediaClient::SupportedVideoDecoderConfigsCallback callback) {
  std::move(callback).Run({});
}

VideoDecoderType MojoMediaClient::GetDecoderImplementationType() {
  return VideoDecoderType::kUnknown;
}

std::unique_ptr<VideoDecoder> MojoMediaClient::CreateVideoDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    MediaLog* media_log,
    mojom::CommandBufferIdPtr command_buffer_id,
    RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space) {
  return nullptr;
}

std::unique_ptr<Renderer> MojoMediaClient::CreateRenderer(
    mojom::FrameInterfaceFactory* frame_interfaces,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    MediaLog* media_log,
    const std::string& audio_device_id) {
  return nullptr;
}

#if BUILDFLAG(ENABLE_CAST_RENDERER)
std::unique_ptr<Renderer> MojoMediaClient::CreateCastRenderer(
    mojom::FrameInterfaceFactory* frame_interfaces,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    MediaLog* media_log,
    const base::UnguessableToken& overlay_plane_id) {
  return nullptr;
}
#endif  // BUILDFLAG(ENABLE_CAST_RENDERER)

#if defined(OS_WIN)
std::unique_ptr<Renderer> MojoMediaClient::CreateMediaFoundationRenderer(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    mojom::FrameInterfaceFactory* frame_interfaces,
    mojo::PendingRemote<mojom::MediaLog> media_log_remote,
    mojo::PendingReceiver<mojom::MediaFoundationRendererExtension>
        renderer_extension_receiver) {
  return nullptr;
}
#endif  // defined(OS_WIN)

std::unique_ptr<CdmFactory> MojoMediaClient::CreateCdmFactory(
    mojom::FrameInterfaceFactory* frame_interfaces) {
  return nullptr;
}

}  // namespace media
