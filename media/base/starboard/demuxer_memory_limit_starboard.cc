// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "base/logging.h"
#include "build/build_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_memory_limit.h"
#include "media/base/video_codecs.h"

#if !BUILDFLAG(USE_STARBOARD_MEDIA)
#error "This file only works with Starboard media."
#endif  // !BUILDFLAG(USE_STARBOARD_MEDIA)

namespace media {
namespace {

int GetBitsPerPixel(const VideoDecoderConfig& video_config) {
  bool is_hdr = false;

  if (video_config.codec() == VideoCodec::kVP9) {
    if (video_config.profile() == VP9PROFILE_PROFILE2 ||
        video_config.profile() == VP9PROFILE_PROFILE3) {
      is_hdr = true;
    }
  } else if (video_config.codec() == VideoCodec::kAV1) {
    const VideoColorSpace& color_space = video_config.color_space_info();

    if (color_space.primaries >= VideoColorSpace::PrimaryID::BT2020 ||
        color_space.transfer >= VideoColorSpace::TransferID::BT2020_10) {
      is_hdr = true;
    }
  }

  int bits = is_hdr ? 10 : 8;

  LOG(INFO) << "Assume " << bits << " bits for "
            << video_config.AsHumanReadableString();

  return bits;
}

}  // namespace

size_t GetDemuxerStreamAudioMemoryLimit(
    const AudioDecoderConfig* /*audio_config*/) {
  return DecoderBuffer::Allocator::GetInstance()->GetAudioBufferBudget();
}

size_t GetDemuxerStreamVideoMemoryLimit(
    DemuxerType /*demuxer_type*/,
    const VideoDecoderConfig* video_config) {
  if (!video_config) {
    return DecoderBuffer::Allocator::GetInstance()->GetVideoBufferBudget(
        VideoCodec::kH264, 1920, 1080, 8);
  }

  auto codec = video_config->codec();
  auto width = video_config->visible_rect().size().width();
  auto height = video_config->visible_rect().size().height();
  auto bits_per_pixel = GetBitsPerPixel(*video_config);

  return DecoderBuffer::Allocator::GetInstance()->GetVideoBufferBudget(
      codec, width, height, bits_per_pixel);
}

size_t GetDemuxerMemoryLimit(DemuxerType demuxer_type) {
  return GetDemuxerStreamAudioMemoryLimit(nullptr) +
         GetDemuxerStreamVideoMemoryLimit(demuxer_type, nullptr);
}

}  // namespace media
