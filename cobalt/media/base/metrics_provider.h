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

#include "third_party/chromium/media/base/container_names.h"
#include "third_party/chromium/media/base/pipeline_status.h"
#include "third_party/chromium/media/base/audio_codecs.h"
#include "third_party/chromium/media/base/video_codecs.h"
#include "third_party/chromium/media/base/timestamp_constants.h"

namespace cobalt {
namespace media {

using VideoPipelineInfo = ::media::VideoPipelineInfo;
using AudioCodec = ::media::AudioCodec;
using VideoCodec = ::media::VideoCodec;
using VideoPipelineInfo = ::media::VideoPipelineInfo;
using AudioPipelineInfo = ::media::AudioPipelineInfo;
using PipelineStatus = ::media::PipelineStatus;
using VideoDecoderType = ::media::VideoDecoderType;

class MediaMetricsProvider {

 public:

  MediaMetricsProvider();
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
    ::media::VideoPipelineInfo video_pipeline_info;
    ::media::AudioPipelineInfo audio_pipeline_info;
    ::media::PipelineStatus last_pipeline_status = ::media::PipelineStatus::PIPELINE_OK;
  };

  struct MediaInfo {
    const bool is_mse;
    // const mojom::MediaURLScheme url_scheme;
    // const mojom::MediaStreamType media_stream_type;
  };


  // based on mojom::MediaMetricsProvider
  void Initialize(bool is_mse);
  //void Initialize(bool is_mse,
                  //mojom::MediaURLScheme url_scheme,
                  //mojom::MediaStreamType media_stream_type);
  void OnError(const ::media::PipelineStatus& status);
  void SetAudioPipelineInfo(const ::media::AudioPipelineInfo& info);
  void SetVideoPipelineInfo(const ::media::VideoPipelineInfo& info);

  void SetHasAudio(::media::AudioCodec audio_codec);
  void SetHasVideo(::media::VideoCodec video_codec);
  void SetHasPlayed();
  void SetHaveEnough();
  void SetIsEME();

  void ReportPipelineUMA();

  std::string GetUMANameForAVStream(const PipelineInfo& player_info);

  bool IsInitialized() const;


 private:
  // UMA pipeline packaged data
  PipelineInfo uma_info_;

  // The values below are only set if `Initialize` has been called.
  absl::optional<MediaInfo> media_info_;

};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_METRICS_PROVIDER_H_
