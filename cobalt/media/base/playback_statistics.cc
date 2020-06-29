// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/media/base/playback_statistics.h"

#include "starboard/atomic.h"
#include "starboard/common/string.h"

namespace cobalt {
namespace media {
namespace {

volatile SbAtomic32 s_max_active_instances = 0;
volatile SbAtomic32 s_active_instances = 0;
volatile SbAtomic32 s_av1_played = 0;
volatile SbAtomic32 s_h264_played = 0;
volatile SbAtomic32 s_hevc_played = 0;
volatile SbAtomic32 s_vp9_played = 0;
volatile SbAtomic32 s_min_video_width = 999999;
volatile SbAtomic32 s_min_video_height = 999999;
volatile SbAtomic32 s_max_video_width = 0;
volatile SbAtomic32 s_max_video_height = 0;
volatile SbAtomic32 s_last_working_codec = kUnknownVideoCodec;

int RoundValues(SbAtomic32 value) {
  if (value < 10) {
    return value;
  }
  if (value < 100) {
    return value / 10 * 10;
  }
  return value / 100 * 100;
}

void UpdateMaxValue(SbAtomic32 new_value, volatile SbAtomic32* max) {
  SB_DCHECK(max);

  for (;;) {
    auto current_max_value = SbAtomicNoBarrier_Load(max);
    if (new_value <= current_max_value) {
      return;
    }
    if (SbAtomicAcquire_CompareAndSwap(max, current_max_value, new_value) ==
        current_max_value) {
      return;
    }
  }
}

void UpdateMinValue(SbAtomic32 new_value, volatile SbAtomic32* min) {
  SB_DCHECK(min);

  for (;;) {
    auto current_min_value = SbAtomicNoBarrier_Load(min);
    if (new_value >= current_min_value) {
      return;
    }
    if (SbAtomicAcquire_CompareAndSwap(min, current_min_value, new_value) ==
        current_min_value) {
      return;
    }
  }
}

}  // namespace

PlaybackStatistics::Record::Record(const VideoDecoderConfig& video_config) {
  SbAtomicNoBarrier_Increment(&s_active_instances, 1);

  UpdateMaxValue(s_active_instances, &s_max_active_instances);

  if (video_config.codec() == kCodecAV1) {
    SbAtomicBarrier_Increment(&s_av1_played, 1);
  } else if (video_config.codec() == kCodecH264) {
    SbAtomicBarrier_Increment(&s_h264_played, 1);
  } else if (video_config.codec() == kCodecHEVC) {
    SbAtomicBarrier_Increment(&s_hevc_played, 1);
  } else if (video_config.codec() == kCodecVP9) {
    SbAtomicBarrier_Increment(&s_vp9_played, 1);
  } else {
    SB_NOTREACHED();
  }

  OnConfigChange(video_config);
}

PlaybackStatistics::Record::~Record() {
  SbAtomicNoBarrier_Increment(&s_active_instances, -1);
}

void PlaybackStatistics::Record::OnPresenting(
    const VideoDecoderConfig& video_config) {
  SbAtomicNoBarrier_Store(&s_last_working_codec, video_config.codec());
}

void PlaybackStatistics::Record::OnConfigChange(
    const VideoDecoderConfig& video_config) {
  auto width = static_cast<SbAtomic32>(video_config.natural_size().width());
  auto height = static_cast<SbAtomic32>(video_config.natural_size().height());

  UpdateMinValue(width, &s_min_video_width);
  UpdateMaxValue(width, &s_max_video_width);
  UpdateMinValue(height, &s_min_video_height);
  UpdateMaxValue(height, &s_max_video_height);

  LOG(INFO) << "Playback statistics on config change: "
            << PlaybackStatistics::GetStatistics(video_config);
}

// static
std::string PlaybackStatistics::GetStatistics(
    const VideoDecoderConfig& current_video_config) {
  return starboard::FormatString(
      "current_codec: %s, drm: %s, width: %d, height: %d,"
      " active_players (max): %d (%d), av1: ~%d, h264: ~%d, hevc: ~%d,"
      " vp9: ~%d, min_width: %d, min_height: %d, max_width: %d, max_height: %d,"
      " last_working_codec: %s",
      GetCodecName(current_video_config.codec()).c_str(),
      (current_video_config.is_encrypted() ? "Y" : "N"),
      static_cast<int>(current_video_config.natural_size().width()),
      static_cast<int>(current_video_config.natural_size().height()),
      SbAtomicNoBarrier_Load(&s_active_instances),
      SbAtomicNoBarrier_Load(&s_max_active_instances),
      RoundValues(SbAtomicNoBarrier_Load(&s_av1_played)),
      RoundValues(SbAtomicNoBarrier_Load(&s_h264_played)),
      RoundValues(SbAtomicNoBarrier_Load(&s_hevc_played)),
      RoundValues(SbAtomicNoBarrier_Load(&s_vp9_played)),
      SbAtomicNoBarrier_Load(&s_min_video_width),
      SbAtomicNoBarrier_Load(&s_min_video_height),
      SbAtomicNoBarrier_Load(&s_max_video_width),
      SbAtomicNoBarrier_Load(&s_max_video_height),
      GetCodecName(static_cast<VideoCodec>(
                       SbAtomicNoBarrier_Load(&s_last_working_codec)))
          .c_str());
}

}  // namespace media
}  // namespace cobalt
