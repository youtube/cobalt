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

#ifndef COBALT_MEDIA_BASE_METRICS_PROVIDER_H_
#define COBALT_MEDIA_BASE_METRICS_PROVIDER_H_

#include <map>
#include <string>

#include "base/containers/small_map.h"
#include "base/time/default_tick_clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "starboard/common/mutex.h"
#include "starboard/types.h"
#include "third_party/chromium/media/base/audio_codecs.h"
#include "third_party/chromium/media/base/container_names.h"
#include "third_party/chromium/media/base/pipeline_status.h"
#include "third_party/chromium/media/base/timestamp_constants.h"
#include "third_party/chromium/media/base/video_codecs.h"

namespace cobalt {
namespace media {

using AudioCodec = ::media::AudioCodec;
using VideoCodec = ::media::VideoCodec;
using PipelineStatus = ::media::PipelineStatus;
using VideoDecoderType = ::media::VideoDecoderType;

enum class WebMediaPlayerAction : uint8_t {
  UNKNOWN_ACTION,
  SEEK,
};

class MediaMetricsProvider {
 public:
  MediaMetricsProvider(
      const base::TickClock* clock = base::DefaultTickClock::GetInstance())
      : clock_(clock) {}
  ~MediaMetricsProvider();

 private:
  struct PipelineInfo {
    PipelineInfo() = default;
    ~PipelineInfo() = default;
    bool has_ever_played = false;
    bool has_reached_have_enough = false;
    bool has_audio = false;
    bool has_video = false;
    bool is_eme = false;
    bool video_decoder_changed = false;
    ::media::AudioCodec audio_codec;
    ::media::VideoCodec video_codec;
    ::media::PipelineStatus last_pipeline_status =
        ::media::PipelineStatus::PIPELINE_OK;
  };

  struct MediaInfo {
    explicit MediaInfo(bool is_mse) : is_mse{is_mse} {};
    const bool is_mse;
  };


 public:
  // based on mojom::MediaMetricsProvider
  void Initialize(bool is_mse);
  void OnError(const ::media::PipelineStatus status);
  void SetHasAudio(::media::AudioCodec audio_codec);
  void SetHasVideo(::media::VideoCodec video_codec);
  void SetHasPlayed();
  void SetHaveEnough();
  void SetIsEME();

  void ReportPipelineUMA();

  // Used to record the latency of an action in the WebMediaPlayer.
  void StartTrackingAction(WebMediaPlayerAction action);
  void EndTrackingAction(WebMediaPlayerAction action);
  bool IsActionCurrentlyTracked(WebMediaPlayerAction action);

 private:
  std::string GetUMANameForAVStream(const PipelineInfo& player_info) const;
  bool IsInitialized() const;

  void ReportActionLatencyUMA(WebMediaPlayerAction action,
                              const base::TimeDelta& action_duration);

 private:
  // Media player action latency data.
  const base::TickClock* clock_;
  base::small_map<std::map<WebMediaPlayerAction, base::TimeTicks>, 2>
      tracked_actions_start_times_;

  // UMA pipeline packaged data
  PipelineInfo uma_info_;

  // The values below are only set if `Initialize` has been called.
  absl::optional<MediaInfo> media_info_;

  starboard::Mutex mutex_;
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_METRICS_PROVIDER_H_
