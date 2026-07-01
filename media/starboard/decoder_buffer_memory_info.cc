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

#include <algorithm>
#include <atomic>

#include "base/logging.h"
#include "media/base/video_codecs.h"
#include "media/starboard/starboard_utils.h"
#include "starboard/media.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(IS_ANDROID)
#include "starboard/android/shared/runtime_resource_overlay.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace media {

namespace {
std::atomic<bool> g_enable_area_based_video_buffer_budget{false};

#if BUILDFLAG(IS_ANDROID)
constexpr int kMaxVideoBufferBudget = 200 * 1024 * 1024;
// this is duplicated from
// starboard/android/shared/media_get_video_buffer_budget.cc in order to allow
// h5vcc experimentation.
int ExperimentalAreaBasedVideoBufferBudget(const gfx::Size& resolution,
                                           int bits_per_pixel) {
  auto get_overlaid_video_buffer_budget = []() {
    auto* overlay = starboard::RuntimeResourceOverlay::GetInstance();
    if (!overlay) {
      return kMaxVideoBufferBudget;
    }

    int buffer_budget = overlay->max_video_buffer_budget();
    if (buffer_budget <= 0 || buffer_budget > 2047) {
      return kMaxVideoBufferBudget;
    }
    LOG(INFO) << "RRO \"max_video_buffer_budget\" is set to " << buffer_budget
              << " MB.";
    return buffer_budget * 1024 * 1024;
  };

  static const int overlaid_video_buffer_budget =
      get_overlaid_video_buffer_budget();

  constexpr int64_t k1080pArea = 1920 * 1080;
  constexpr int64_t k4kArea = 3840 * 2160;

  int video_buffer_budget = 0;
  const int64_t area =
      static_cast<int64_t>(resolution.width()) * resolution.height();
  if (resolution.IsEmpty() || area <= k1080pArea) {
    video_buffer_budget = 30 * 1024 * 1024;
  } else if (area <= k4kArea) {
    if (bits_per_pixel <= 8) {
      video_buffer_budget = 100 * 1024 * 1024;
    } else {
      video_buffer_budget = 160 * 1024 * 1024;
    }
  } else {
    video_buffer_budget = kMaxVideoBufferBudget;
  }
  return std::min(video_buffer_budget, overlaid_video_buffer_budget);
}
#endif  // BUILDFLAG(IS_ANDROID)
}  // namespace

void EnableAreaBasedVideoBufferBudget() {
  g_enable_area_based_video_buffer_budget.store(true);
}

int GetAudioDecoderBufferLimitBytes() {
  return SbMediaGetAudioBufferBudget();
}

int GetVideoDecoderBufferLimitBytes(VideoCodec codec,
                                    const gfx::Size& resolution,
                                    int bits_per_pixel) {
#if BUILDFLAG(IS_ANDROID)
  if (g_enable_area_based_video_buffer_budget.load()) {
    return ExperimentalAreaBasedVideoBufferBudget(resolution, bits_per_pixel);
  }
#endif  // BUILDFLAG(IS_ANDROID)
  return SbMediaGetVideoBufferBudget(MediaVideoCodecToSbMediaVideoCodec(codec),
                                     resolution.width(), resolution.height(),
                                     bits_per_pixel);
}

}  // namespace media
