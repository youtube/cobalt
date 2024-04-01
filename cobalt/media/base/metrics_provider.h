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

#include <string>

#include "media/base/audio_codecs.h"
#include "media/base/container_names.h"
#include "media/base/pipeline_status.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_codecs.h"
#include "starboard/common/mutex.h"

namespace cobalt {
namespace media {

using AudioCodec = ::media::AudioCodec;
using VideoCodec = ::media::VideoCodec;
using PipelineStatus = ::media::PipelineStatus;
using VideoDecoderType = ::media::VideoDecoderType;

class MediaMetricsProvider {
 public:
  MediaMetricsProvider() = default;
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

 private:
  std::string GetUMANameForAVStream(const PipelineInfo& player_info) const;
  bool IsInitialized() const;

 private:
  // UMA pipeline packaged data
  PipelineInfo uma_info_;

  // The values below are only set if `Initialize` has been called.
  absl::optional<MediaInfo> media_info_;

  starboard::Mutex mutex_;
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_METRICS_PROVIDER_H_
