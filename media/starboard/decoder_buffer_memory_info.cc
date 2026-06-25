// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "media/starboard/decoder_buffer_memory_info.h"

#include "base/metrics/histogram_macros.h"
#include "media/base/video_codecs.h"
#include "media/starboard/starboard_utils.h"
#include "starboard/media.h"
#include "ui/gfx/geometry/size.h"

namespace media {

constexpr int kBytesPerMegabyte = 1024 * 1024;

int GetAudioDecoderBufferLimitBytes() {
  // TODO(mcasas): This value should always be the same on every call, enforce.
  const auto audio_budget_in_bytes = SbMediaGetAudioBufferBudget();
  UMA_HISTOGRAM_MEMORY_MEDIUM_MB("Media.Starboard.AudioBufferBudget",
                                 audio_budget_in_bytes / kBytesPerMegabyte);
  return audio_budget_in_bytes;
}

int GetVideoDecoderBufferLimitBytes(VideoCodec codec,
                                    const gfx::Size& resolution,
                                    int bits_per_pixel) {
  const auto video_budget_in_bytes = SbMediaGetVideoBufferBudget(
      MediaVideoCodecToSbMediaVideoCodec(codec), resolution.width(),
      resolution.height(), bits_per_pixel);
  UMA_HISTOGRAM_MEMORY_MEDIUM_MB("Media.Starboard.VideoBufferBudget",
                                 video_budget_in_bytes / kBytesPerMegabyte);
  return video_budget_in_bytes;
}

}  // namespace media
