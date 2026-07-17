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

// |g_720p_video_buffer_size_clamp_bytes| and
// |g_video_buffer_size_clamp_bytes| are process-wide, and are currently set by
// h5vcc flags.
std::atomic<size_t> g_720p_video_buffer_size_clamp_bytes{
    std::numeric_limits<size_t>::max()};
std::atomic<size_t> g_video_buffer_size_clamp_bytes{
    std::numeric_limits<size_t>::max()};

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

void Set720pVideoBufferSizeClamp(int size_mb) {
  CHECK_GE(size_mb, 0);
  CHECK_LT(size_mb, 4096);
  g_720p_video_buffer_size_clamp_bytes =
      static_cast<size_t>(size_mb) * 1024 * 1024;
}

size_t GetVideoBufferSizeClamp() {
  return g_video_buffer_size_clamp_bytes.load();
}

void SetVideoBufferSizeClamp(int size_mb) {
  // We convert the value from MBs to bytes, as the values returned by
  // GetVideoDecoderBufferLimitBytes's return value is in bytes.
  CHECK_GT(size_mb, 0);
  // Prevent overflow bugs by setting the limit to 4 GiB.
  CHECK_LT(size_mb, 4096);
  g_video_buffer_size_clamp_bytes = static_cast<size_t>(size_mb) * 1024 * 1024;
}

size_t GetDemuxerStreamAudioMemoryLimit(
    const AudioDecoderConfig* /*audio_config*/) {
  return GetAudioDecoderBufferLimitBytes();
}

size_t GetDemuxerStreamVideoMemoryLimit(
    DemuxerType /*demuxer_type*/,
    const VideoDecoderConfig* video_config) {
  if (const size_t limit_720p = g_720p_video_buffer_size_clamp_bytes.load();
      limit_720p != std::numeric_limits<size_t>::max() && video_config) {
    const gfx::Size resolution = video_config->visible_rect().size();
    if (resolution.width() <= 1280 && resolution.height() <= 720) {
      return limit_720p;
    }
  }

  size_t limit;
  if (!video_config) {
    limit = GetVideoDecoderBufferLimitBytes(
        VideoCodec::kH264, /*resolution=*/{1920, 1080}, /*bits_per_pixel=*/8);
  } else {
    limit = GetVideoDecoderBufferLimitBytes(video_config->codec(),
                                            video_config->visible_rect().size(),
                                            GetBitsPerPixel(*video_config));
  }

  return std::min(limit, g_video_buffer_size_clamp_bytes.load());
}

size_t GetDemuxerMemoryLimit(DemuxerType demuxer_type) {
  return GetDemuxerStreamAudioMemoryLimit(nullptr) +
         GetDemuxerStreamVideoMemoryLimit(demuxer_type, nullptr);
}

}  // namespace media
