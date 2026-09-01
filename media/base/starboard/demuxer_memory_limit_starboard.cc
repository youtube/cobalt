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

// clang-format off
#include "media/base/demuxer_memory_limit.h"
// clang-format on

#include <atomic>

#include "base/logging.h"
#include "build/build_config.h"
#include "media/base/video_codecs.h"
#include "media/starboard/decoder_buffer_memory_info.h"

#if !BUILDFLAG(USE_STARBOARD_MEDIA)
#error "This file only works with Starboard media."
#endif  // !BUILDFLAG(USE_STARBOARD_MEDIA)

namespace media {
namespace {

// |g_video_buffer_size_reduction_percent| is process-wide, and is currently set
// by h5vcc flags.
std::atomic<std::optional<int>> g_video_buffer_size_reduction_percent{
    std::nullopt};

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

void SetVideoBufferSizeReductionPercent(int reduction_pct) {
  CHECK_GT(reduction_pct, 0);
  CHECK_LT(reduction_pct, 100);
  g_video_buffer_size_reduction_percent = reduction_pct;
}

base::ByteCount GetDemuxerStreamAudioMemoryLimit(
    const AudioDecoderConfig* /*audio_config*/) {
  return base::ByteCount(GetAudioDecoderBufferLimitBytes());
}

base::ByteCount GetDemuxerStreamVideoMemoryLimit(
    DemuxerType /*demuxer_type*/,
    const VideoDecoderConfig* video_config) {
  int64_t limit;
  if (!video_config) {
    limit = GetVideoDecoderBufferLimitBytes(
        VideoCodec::kH264, /*resolution=*/{1920, 1080}, /*bits_per_pixel=*/8);
  } else {
    limit = GetVideoDecoderBufferLimitBytes(video_config->codec(),
                                            video_config->visible_rect().size(),
                                            GetBitsPerPixel(*video_config));
  }

  std::optional<int> reduction_pct =
      g_video_buffer_size_reduction_percent.load();
  if (!reduction_pct.has_value()) {
    return base::ByteCount(limit);
  }

  // Multiplying limit by uint64_t remaining_pct preserves full precision and
  // automatically promotes the calculation to 64-bit to prevent 32-bit
  // overflow.
  const uint64_t remaining_pct = 100 - reduction_pct.value();

  return base::ByteCount(
      static_cast<int64_t>((static_cast<uint64_t>(limit) * remaining_pct) / 100));
}

base::ByteCount GetDemuxerMemoryLimit(DemuxerType demuxer_type) {
  return GetDemuxerStreamAudioMemoryLimit(nullptr) +
         GetDemuxerStreamVideoMemoryLimit(demuxer_type, nullptr);
}

}  // namespace media
