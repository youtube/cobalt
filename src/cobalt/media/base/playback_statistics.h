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

#ifndef COBALT_MEDIA_BASE_PLAYBACK_STATISTICS_H_
#define COBALT_MEDIA_BASE_PLAYBACK_STATISTICS_H_

#include <string>

#include "base/optional.h"
#include "base/time/time.h"
#include "cobalt/media/base/video_decoder_config.h"

namespace cobalt {
namespace media {

class PlaybackStatistics {
 public:
  ~PlaybackStatistics();

  void UpdateVideoConfig(const VideoDecoderConfig& video_config);
  void OnPresenting(const VideoDecoderConfig& video_config);
  void OnSeek(const base::TimeDelta& seek_time);
  void OnAudioAU(const base::TimeDelta& timestamp);
  void OnVideoAU(const base::TimeDelta& timestamp);

  std::string GetStatistics(
      const VideoDecoderConfig& current_video_config) const;

 private:
  base::TimeDelta seek_time_;
  base::Optional<base::TimeDelta> first_written_audio_timestamp_;
  base::Optional<base::TimeDelta> first_written_video_timestamp_;
  base::TimeDelta last_written_audio_timestamp_;
  base::TimeDelta last_written_video_timestamp_;
  bool is_initial_config_ = true;
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_PLAYBACK_STATISTICS_H_
