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
#include "cobalt/media/base/chrome_media.h"
#include "media/base/pipeline_status.h"
#include "media/base/video_codecs.h"

namespace cobalt {
namespace media {

class PlaybackStatistics {
 public:
  explicit PlaybackStatistics(const std::string& pipeline_identifier);
  ~PlaybackStatistics();

  // TODO: Move to private
  void UpdateVideoConfig(::media::VideoCodec video_codec, int video_width,
                         int video_height, bool is_encrypted);

  template <typename VideoDecoderConfig>
  void UpdateVideoConfig(const VideoDecoderConfig& video_config) {
    UpdateVideoConfig(static_cast<::media::VideoCodec>(video_config.codec()),
                      video_config.natural_size().width(),
                      video_config.natural_size().height(),
                      video_config.is_encrypted());
  }

  // TODO: Move to private
  void OnPresenting(::media::VideoCodec video_codec);

  template <typename VideoDecoderConfig>
  void OnPresenting(const VideoDecoderConfig& video_config) {
    OnPresenting(static_cast<::media::VideoCodec>(video_config.codec()));
  }

  void OnSeek(const base::TimeDelta& seek_time);

  // TODO: Move to private
  void OnAudioAU(bool end_of_stream, base::TimeDelta timestamp);

  template <typename DecoderBuffer>
  void OnAudioAU(const scoped_refptr<DecoderBuffer>& buffer) {
    bool end_of_stream = buffer->end_of_stream();
    OnAudioAU(end_of_stream,
              end_of_stream ? base::TimeDelta() : buffer->timestamp());
  }

  // TODO: Move to private
  void OnVideoAU(bool end_of_stream, base::TimeDelta timestamp);

  template <typename DecoderBuffer>
  void OnVideoAU(const scoped_refptr<DecoderBuffer>& buffer) {
    bool end_of_stream = buffer->end_of_stream();
    OnVideoAU(end_of_stream,
              end_of_stream ? base::TimeDelta() : buffer->timestamp());
  }

  void OnError(::media_m114::PipelineStatus status,
               const std::string& error_message);

  // TODO: Move to private
  std::string GetStatistics(::media::VideoCodec video_codec,
                            bool is_encrypted) const;

  template <typename VideoDecoderConfig>
  std::string GetStatistics(
      const VideoDecoderConfig& current_video_config) const {
    return GetStatistics(
        static_cast<::media::VideoCodec>(current_video_config.codec()),
        current_video_config.is_encrypted());
  }

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
  base::CVal<::media_m114::PipelineStatus> pipeline_status_;
  base::CVal<std::string> error_message_;
  base::CVal<std::string> current_video_codec_;
  bool has_active_instance_ = false;
  bool is_first_audio_buffer_written_ = false;
  bool is_first_video_buffer_written_ = false;
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_PLAYBACK_STATISTICS_H_
