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

#include <math.h>
#include <stdint.h>

#include "base/strings/stringprintf.h"
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

int64_t RoundValues(int64_t value) {
  if (value < 10) {
    return value;
  }
  int64_t closest_power = pow(10, floor(log10(value)));
  return floor(value / closest_power) * closest_power;
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

PlaybackStatistics::PlaybackStatistics(const std::string& pipeline_identifier)
    : seek_time_(base::StringPrintf("Media.Pipeline.%s.SeekTime",
                                    pipeline_identifier.c_str()),
                 base::TimeDelta(), "The seek-to time of the media pipeline."),
      first_written_audio_timestamp_(
          base::StringPrintf("Media.Pipeline.%s.FirstWrittenAudioTimestamp",
                             pipeline_identifier.c_str()),
          base::TimeDelta(),
          "The timestamp of the first written audio buffer in the media "
          "pipeline."),
      first_written_video_timestamp_(
          base::StringPrintf("Media.Pipeline.%s.FirstWrittenVideoTimestamp",
                             pipeline_identifier.c_str()),
          base::TimeDelta(),
          "The timestamp of the first written audio buffer in the media "
          "pipeline."),
      last_written_audio_timestamp_(
          base::StringPrintf("Media.Pipeline.%s.LastWrittenAudioTimestamp",
                             pipeline_identifier.c_str()),
          base::TimeDelta(),
          "The timestamp of the last written audio buffer in the media "
          "pipeline."),
      last_written_video_timestamp_(
          base::StringPrintf("Media.Pipeline.%s.LastWrittenVideoTimestamp",
                             pipeline_identifier.c_str()),
          base::TimeDelta(),
          "The timestamp of the last written video buffer in the media "
          "pipeline."),
      video_width_(base::StringPrintf("Media.Pipeline.%s.VideoWidth",
                                      pipeline_identifier.c_str()),
                   0, "The frame width of the video."),
      video_height_(base::StringPrintf("Media.Pipeline.%s.VideoHeight",
                                       pipeline_identifier.c_str()),
                    0, "The frame height of the video."),
      is_audio_eos_written_(
          base::StringPrintf("Media.Pipeline.%s.IsAudioEOSWritten",
                             pipeline_identifier.c_str()),
          false, "Indicator of if the audio eos is written."),
      is_video_eos_written_(
          base::StringPrintf("Media.Pipeline.%s.IsVideoEOSWritten",
                             pipeline_identifier.c_str()),
          false, "Indicator of if the video eos is written."),
      pipeline_status_(base::StringPrintf("Media.Pipeline.%s.PipelineStatus",
                                          pipeline_identifier.c_str()),
                       PIPELINE_OK, "The status of the media pipeline."),
      error_message_(base::StringPrintf("Media.Pipeline.%s.ErrorMessage",
                                        pipeline_identifier.c_str()),
                     "", "The error message of the media pipeline error.") {}

PlaybackStatistics::~PlaybackStatistics() {
  if (has_active_instance_) {
    DCHECK(SbAtomicAcquire_Load(&s_active_instances) > 0);
    SbAtomicNoBarrier_Increment(&s_active_instances, -1);
  }
}

void PlaybackStatistics::UpdateVideoConfig(
    const VideoDecoderConfig& video_config) {
  if (!has_active_instance_) {
    has_active_instance_ = true;

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
  }

  video_width_ = video_config.natural_size().width();
  video_height_ = video_config.natural_size().height();

  const auto width = static_cast<SbAtomic32>(video_width_);
  const auto height = static_cast<SbAtomic32>(video_height_);

  UpdateMinValue(width, &s_min_video_width);
  UpdateMaxValue(width, &s_max_video_width);
  UpdateMinValue(height, &s_min_video_height);
  UpdateMaxValue(height, &s_max_video_height);

  LOG(INFO) << "Playback statistics on config change: "
            << GetStatistics(video_config);
}


void PlaybackStatistics::OnPresenting(const VideoDecoderConfig& video_config) {
  SbAtomicNoBarrier_Store(&s_last_working_codec, video_config.codec());
}

void PlaybackStatistics::OnSeek(const base::TimeDelta& seek_time) {
  seek_time_ = seek_time;
  is_first_audio_buffer_written_ = false;
  is_first_video_buffer_written_ = false;
}

void PlaybackStatistics::OnAudioAU(const scoped_refptr<DecoderBuffer>& buffer) {
  if (buffer->end_of_stream()) {
    is_audio_eos_written_ = true;
    return;
  }
  last_written_audio_timestamp_ = buffer->timestamp();
  if (!is_first_audio_buffer_written_) {
    is_first_audio_buffer_written_ = true;
    first_written_audio_timestamp_ = buffer->timestamp();
  }
}

void PlaybackStatistics::OnVideoAU(const scoped_refptr<DecoderBuffer>& buffer) {
  if (buffer->end_of_stream()) {
    is_video_eos_written_ = true;
    return;
  }
  last_written_video_timestamp_ = buffer->timestamp();
  if (!is_first_video_buffer_written_) {
    is_first_video_buffer_written_ = true;
    first_written_video_timestamp_ = buffer->timestamp();
  }
}

void PlaybackStatistics::OnError(PipelineStatus status,
                                 const std::string& error_message) {
  pipeline_status_ = status;
  error_message_ = error_message;
}

std::string PlaybackStatistics::GetStatistics(
    const VideoDecoderConfig& current_video_config) const {
  return starboard::FormatString(
      "current_codec: %s, drm: %s, width: %d, height: %d"
      ", active_players (max): %d (%d), av1: ~%" PRId64 ", h264: ~%" PRId64
      ", hevc: ~%" PRId64 ", vp9: ~%" PRId64
      ", min_width: %d, min_height: %d, max_width: %d, max_height: %d"
      ", last_working_codec: %s, seek_time: %s"
      ", first_audio_time: ~%" PRId64 ", first_video_time: ~%" PRId64
      ", last_audio_time: ~%" PRId64 ", last_video_time: ~%" PRId64,
      GetCodecName(current_video_config.codec()).c_str(),
      (current_video_config.is_encrypted() ? "Y" : "N"), video_width_.value(),
      video_height_.value(), SbAtomicNoBarrier_Load(&s_active_instances),
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
          .c_str(),
      ValToString(seek_time_).c_str(),
      is_first_audio_buffer_written_
          ? RoundValues(first_written_audio_timestamp_.value().InSeconds())
          : -1,
      is_first_video_buffer_written_
          ? RoundValues(first_written_video_timestamp_.value().InSeconds())
          : -1,
      is_first_audio_buffer_written_
          ? RoundValues(last_written_audio_timestamp_.value().InSeconds())
          : -1,
      is_first_video_buffer_written_
          ? RoundValues(last_written_video_timestamp_.value().InSeconds())
          : -1);
}

}  // namespace media
}  // namespace cobalt
