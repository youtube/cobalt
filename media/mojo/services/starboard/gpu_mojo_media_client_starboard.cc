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

#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_encoder.h"
#include "media/base/media_log.h"
#include "media/base/video_decoder.h"
#include "media/mojo/services/gpu_mojo_media_client.h"
#include "media/mojo/services/starboard/starboard_renderer_wrapper.h"
#include "media/starboard/starboard_cdm_factory.h"

namespace media {

std::unique_ptr<VideoDecoder> CreatePlatformVideoDecoder(
    VideoDecoderTraits& traits) {
  return nullptr;
}

absl::optional<SupportedVideoDecoderConfigs>
GetPlatformSupportedVideoDecoderConfigs(
    gpu::GpuDriverBugWorkarounds gpu_workarounds,
    gpu::GpuPreferences gpu_preferences,
    const gpu::GPUInfo& gpu_info,
    base::OnceCallback<SupportedVideoDecoderConfigs()> get_vda_configs) {
  return {};
}

std::unique_ptr<AudioDecoder> CreatePlatformAudioDecoder(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::unique_ptr<MediaLog> media_log) {
  return nullptr;
}

std::unique_ptr<AudioEncoder> CreatePlatformAudioEncoder(
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  return nullptr;
}

std::unique_ptr<Renderer> CreatePlatformStarboardRenderer(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    mojo::PendingRemote<mojom::MediaLog> media_log_remote,
    const base::UnguessableToken& overlay_plane_id,
    base::TimeDelta audio_write_duration_local,
    base::TimeDelta audio_write_duration_remote,
    mojo::PendingReceiver<mojom::StarboardRendererExtension>
        renderer_extension_receiver,
    mojo::PendingRemote<mojom::StarboardRendererClientExtension>
        client_extension_remote) {
  return std::make_unique<StarboardRendererWrapper>(
      std::move(task_runner), std::move(media_log_remote), overlay_plane_id,
      audio_write_duration_local, audio_write_duration_remote,
      std::move(renderer_extension_receiver),
      std::move(client_extension_remote));
}

std::unique_ptr<CdmFactory> CreatePlatformCdmFactory(
    mojom::FrameInterfaceFactory* frame_interfaces) {
  return std::make_unique<media::StarboardCdmFactory>();
}

VideoDecoderType GetPlatformDecoderImplementationType(
    gpu::GpuDriverBugWorkarounds gpu_workarounds,
    gpu::GpuPreferences gpu_preferences,
    const gpu::GPUInfo& gpu_info) {
  return VideoDecoderType::kMediaCodec;
}

}  // namespace media
