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

#include "media/base/demuxer_memory_limit.h"
#include "media/base/video_codecs.h"
#include "base/logging.h"
#include "starboard/media.h"

namespace media {
namespace {

// TODO(b/231375871): Move this to video_codecs.h.  Also consider remove
// starboard_utils.* and move the functions to individual files.
SbMediaVideoCodec MediaVideoCodecToSbMediaVideoCodec(VideoCodec codec) {
  switch (codec) {
    case VideoCodec::kH264:
      return kSbMediaVideoCodecH264;
    case VideoCodec::kVC1:
      return kSbMediaVideoCodecVc1;
    case VideoCodec::kMPEG2:
      return kSbMediaVideoCodecMpeg2;
    case VideoCodec::kTheora:
      return kSbMediaVideoCodecTheora;
    case VideoCodec::kVP8:
      return kSbMediaVideoCodecVp8;
    case VideoCodec::kVP9:
      return kSbMediaVideoCodecVp9;
    case VideoCodec::kHEVC:
      return kSbMediaVideoCodecH265;
    case VideoCodec::kAV1:
      return kSbMediaVideoCodecAv1;
    default:
      // Cobalt only supports a subset of video codecs defined by Chromium.
      DLOG(ERROR) << "Unsupported video codec " << GetCodecName(codec);
      return kSbMediaVideoCodecNone;
  }
  NOTREACHED();
  return kSbMediaVideoCodecNone;
}

}  // namespace

size_t GetDemuxerStreamAudioMemoryLimit(
    const AudioDecoderConfig* /*audio_config*/) {
  return SbMediaGetAudioBufferBudget();
}

size_t GetDemuxerStreamVideoMemoryLimit(
    Demuxer::DemuxerTypes /*demuxer_type*/,
    const VideoDecoderConfig* video_config) {
  if (!video_config) {
    return static_cast<size_t>(
        SbMediaGetVideoBufferBudget(kSbMediaVideoCodecH264, 1920, 1080, 8));
  }

  auto width = video_config->visible_rect().size().width();
  auto height = video_config->visible_rect().size().height();
  // TODO(b/230799815_): Ensure |bits_per_pixel| always contains a valid value,
  // or consider deprecating it from `SbMediaGetVideoBufferBudget()`.
  auto bits_per_pixel = 0;
  auto codec = MediaVideoCodecToSbMediaVideoCodec(video_config->codec());
  return static_cast<size_t>(
      SbMediaGetVideoBufferBudget(codec, width, height, bits_per_pixel));
}

size_t GetDemuxerMemoryLimit(Demuxer::DemuxerTypes demuxer_type) {
  return GetDemuxerStreamAudioMemoryLimit(nullptr) +
         GetDemuxerStreamVideoMemoryLimit(demuxer_type, nullptr);
}

}  // namespace media
