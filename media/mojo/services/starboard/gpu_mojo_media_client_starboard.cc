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

class GpuMojoMediaClientStarboard final : public GpuMojoMediaClient {
 public:
  GpuMojoMediaClientStarboard(GpuMojoMediaClientTraits& traits)
      : GpuMojoMediaClient(traits) {}
  ~GpuMojoMediaClientStarboard() final = default;

 protected:
  std::unique_ptr<VideoDecoder> CreatePlatformVideoDecoder(
      VideoDecoderTraits& traits) final {
    return nullptr;
  }

  absl::optional<SupportedVideoDecoderConfigs>
  GetPlatformSupportedVideoDecoderConfigs() final {
    return {};
  }

  VideoDecoderType GetPlatformDecoderImplementationType() final {
    return VideoDecoderType::kMediaCodec;
  }

  std::unique_ptr<AudioDecoder> CreatePlatformAudioDecoder(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      std::unique_ptr<MediaLog> media_log) final {
    return nullptr;
  }

  std::unique_ptr<AudioEncoder> CreatePlatformAudioEncoder(
      scoped_refptr<base::SequencedTaskRunner> task_runner) final {
    return nullptr;
  }

  std::unique_ptr<CdmFactory> CreatePlatformCdmFactory(
      mojom::FrameInterfaceFactory* frame_interfaces) final {
    return std::make_unique<media::StarboardCdmFactory>();
  }
};

std::unique_ptr<Renderer> CreatePlatformStarboardRenderer(
    StarboardRendererTraits traits) {
  return std::make_unique<StarboardRendererWrapper>(std::move(traits));
}

std::unique_ptr<GpuMojoMediaClient> CreateGpuMediaService(
    GpuMojoMediaClientTraits& traits) {
  return std::make_unique<GpuMojoMediaClientStarboard>(traits);
}

}  // namespace media
