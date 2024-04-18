// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/media/base/metrics_provider.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"

namespace cobalt {
namespace media {

using starboard::ScopedLock;

MediaMetricsProvider::~MediaMetricsProvider() { ReportPipelineUMA(); }

void MediaMetricsProvider::OnError(const PipelineStatus status) {
  ScopedLock scoped_lock(mutex_);
  uma_info_.last_pipeline_status = status;
}

void MediaMetricsProvider::SetHasAudio(AudioCodec audio_codec) {
  ScopedLock scoped_lock(mutex_);
  uma_info_.audio_codec = audio_codec;
  uma_info_.has_audio = true;
}

void MediaMetricsProvider::SetHasVideo(VideoCodec video_codec) {
  ScopedLock scoped_lock(mutex_);
  uma_info_.video_codec = video_codec;
  uma_info_.has_video = true;
}

void MediaMetricsProvider::SetHasPlayed() {
  ScopedLock scoped_lock(mutex_);
  uma_info_.has_ever_played = true;
}

void MediaMetricsProvider::SetHaveEnough() {
  ScopedLock scoped_lock(mutex_);
  uma_info_.has_reached_have_enough = true;
}

void MediaMetricsProvider::SetIsEME() {
  ScopedLock scoped_lock(mutex_);
  uma_info_.is_eme = true;
}

void MediaMetricsProvider::ReportPipelineUMA() {
  ScopedLock scoped_lock(mutex_);
  if (uma_info_.has_video && uma_info_.has_audio) {
    base::UmaHistogramExactLinear(
        GetUMANameForAVStream(uma_info_), uma_info_.last_pipeline_status.code(),
        PipelineStatus::Codes::PIPELINE_STATUS_MAX + 1);
  } else if (uma_info_.has_audio) {
    base::UmaHistogramExactLinear(
        "Cobalt.Media.PipelineStatus.AudioOnly",
        uma_info_.last_pipeline_status.code(),
        PipelineStatus::Codes::PIPELINE_STATUS_MAX + 1);
  } else if (uma_info_.has_video) {
    base::UmaHistogramExactLinear(
        "Cobalt.Media.PipelineStatus.VideoOnly",
        uma_info_.last_pipeline_status.code(),
        PipelineStatus::Codes::PIPELINE_STATUS_MAX + 1);
  } else {
    // Note: This metric can be recorded as a result of normal operation with
    // Media Source Extensions. If a site creates a MediaSource object but never
    // creates a source buffer or appends data, PIPELINE_OK will be recorded.
    base::UmaHistogramExactLinear(
        "Cobalt.Media.PipelineStatus.Unsupported",
        uma_info_.last_pipeline_status.code(),
        PipelineStatus::Codes::PIPELINE_STATUS_MAX + 1);
  }

  // Report whether this player ever saw a playback event. Used to measure the
  // effectiveness of efforts to reduce loaded-but-never-used players.
  if (uma_info_.has_reached_have_enough)
    base::UmaHistogramBoolean("Cobalt.Media.HasEverPlayed",
                              uma_info_.has_ever_played);
}

std::string MediaMetricsProvider::GetUMANameForAVStream(
    const PipelineInfo& player_info) const {
  constexpr char kPipelineUmaPrefix[] =
      "Cobalt.Media.PipelineStatus.AudioVideo.";
  std::string uma_name = kPipelineUmaPrefix;
  if (player_info.video_codec == VideoCodec::kVP9)
    uma_name += "VP9";
  else if (player_info.video_codec == VideoCodec::kH264)
    uma_name += "H264";
  else if (player_info.video_codec == VideoCodec::kAV1)
    uma_name += "AV1";
  else
    uma_name += "Other";

  return uma_name;
}

void MediaMetricsProvider::StartTrackingAction(MediaAction action) {
  DCHECK(!IsActionCurrentlyTracked(action));
  ScopedLock scoped_lock(mutex_);

  tracked_actions_start_times_[action] = clock_->NowTicks();
}

void MediaMetricsProvider::EndTrackingAction(MediaAction action) {
  DCHECK(IsActionCurrentlyTracked(action));
  ScopedLock scoped_lock(mutex_);

  auto duration = clock_->NowTicks() - tracked_actions_start_times_[action];
  ReportActionLatencyUMA(action, duration);
  tracked_actions_start_times_.erase(action);
}

bool MediaMetricsProvider::IsActionCurrentlyTracked(MediaAction action) {
  ScopedLock scoped_lock(mutex_);
  return tracked_actions_start_times_.find(action) !=
         tracked_actions_start_times_.end();
}

void MediaMetricsProvider::ReportActionLatencyUMA(
    MediaAction action, const base::TimeDelta& action_duration) {
  switch (action) {
    case MediaAction::WEBMEDIAPLAYER_SEEK:
      UMA_HISTOGRAM_TIMES("Cobalt.Media.WebMediaPlayer.Seek.Timing",
                          action_duration);
      break;
    case MediaAction::SBPLAYER_CREATE:
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
          "Cobalt.Media.SbPlayer.Create.Timing", action_duration,
          base::TimeDelta::FromMicroseconds(100),
          base::TimeDelta::FromMicroseconds(1500), 50);
      break;
    case MediaAction::SBPLAYER_CREATE_URL_PLAYER:
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
          "Cobalt.Media.SbPlayer.CreateUrlPlayer.Timing", action_duration,
          base::TimeDelta::FromMicroseconds(100),
          base::TimeDelta::FromMicroseconds(1500), 50);
      break;
    case MediaAction::SBPLAYER_DESTROY:
      UMA_HISTOGRAM_CUSTOM_MICROSECONDS_TIMES(
          "Cobalt.Media.SbPlayer.Destroy.Timing", action_duration,
          base::TimeDelta::FromMicroseconds(500),
          base::TimeDelta::FromMicroseconds(40000), 50);
      break;
    case MediaAction::UNKNOWN_ACTION:
    default:
      break;
  }
}

}  // namespace media
}  // namespace cobalt
