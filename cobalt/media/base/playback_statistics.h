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
#include "cobalt/base/c_val.h"
#include "cobalt/media/base/decoder_buffer.h"
#include "cobalt/media/base/pipeline_status.h"
#include "cobalt/media/base/video_decoder_config.h"

namespace cobalt {
namespace media {

class PlaybackStatistics {
 public:
  explicit PlaybackStatistics(const std::string& pipeline_identifier);
  ~PlaybackStatistics();

  void UpdateVideoConfig(const VideoDecoderConfig& video_config);
  void OnPresenting(const VideoDecoderConfig& video_config);
  void OnSeek(const base::TimeDelta& seek_time);
  void OnAudioAU(const scoped_refptr<DecoderBuffer>& buffer);
  void OnVideoAU(const scoped_refptr<DecoderBuffer>& buffer);
  void OnError(PipelineStatus status, const std::string& error_message);

  std::string GetStatistics(
      const VideoDecoderConfig& current_video_config) const;

 private:
  base::CVal<base::TimeDelta> seek_time_;
  base::CVal<base::TimeDelta> first_written_audio_timestamp_;
  base::CVal<base::TimeDelta> first_written_video_timestamp_;
  base::CVal<base::TimeDelta> last_written_audio_timestamp_;
  base::CVal<base::TimeDelta> last_written_video_timestamp_;
  base::CVal<int> video_width_;
  base::CVal<int> video_height_;
  base::CVal<bool> is_audio_eos_written_;
  base::CVal<bool> is_video_eos_written_;
  base::CVal<PipelineStatus> pipeline_status_;
  base::CVal<std::string> error_message_;
  bool has_active_instance_ = false;
  bool is_first_audio_buffer_written_ = false;
  bool is_first_video_buffer_written_ = false;
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_PLAYBACK_STATISTICS_H_
