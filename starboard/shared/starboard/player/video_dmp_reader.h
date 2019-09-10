// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_VIDEO_DMP_READER_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_VIDEO_DMP_READER_H_

#include <string>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/common/ref_counted.h"
#include "starboard/file.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/video_dmp_common.h"

#if SB_HAS(PLAYER_FILTER_TESTS)
namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace video_dmp {

class VideoDmpReader {
 public:
  class AccessUnit {
   public:
    AccessUnit(SbTime timestamp,
               const SbDrmSampleInfoWithSubSampleMapping* drm_sample_info,
               std::vector<uint8_t> data)
        : timestamp_(timestamp),
          drm_sample_info_present_(drm_sample_info != NULL),
          drm_sample_info_(drm_sample_info == NULL
                               ? SbDrmSampleInfoWithSubSampleMapping()
                               : *drm_sample_info),
          data_(std::move(data)) {}
    SbTime timestamp() const { return timestamp_; }
    const SbDrmSampleInfo* drm_sample_info() const {
      return drm_sample_info_present_ ? &drm_sample_info_ : NULL;
    }
    const std::vector<uint8_t>& data() const { return data_; }

   private:
    SbTime timestamp_;
    bool drm_sample_info_present_;
    SbDrmSampleInfoWithSubSampleMapping drm_sample_info_;
    std::vector<uint8_t> data_;
  };

  class AudioAccessUnit : public AccessUnit {
   public:
    AudioAccessUnit(SbTime timestamp,
                    const SbDrmSampleInfoWithSubSampleMapping* drm_sample_info,
                    std::vector<uint8_t> data,
                    const SbMediaAudioSampleInfoWithConfig& audio_sample_info)
        : AccessUnit(timestamp, drm_sample_info, std::move(data)),
          audio_sample_info_(audio_sample_info) {}
    const SbMediaAudioSampleInfo& audio_sample_info() const {
      return audio_sample_info_;
    }

   private:
    SbMediaAudioSampleInfoWithConfig audio_sample_info_;
  };

  class VideoAccessUnit : public AccessUnit {
   public:
    VideoAccessUnit(SbTime timestamp,
                    const SbDrmSampleInfoWithSubSampleMapping* drm_sample_info,
                    std::vector<uint8_t> data,
                    const SbMediaVideoSampleInfoWithOptionalColorMetadata&
                        video_sample_info)
        : AccessUnit(timestamp, drm_sample_info, std::move(data)),
          video_sample_info_(video_sample_info) {}
    const SbMediaVideoSampleInfo& video_sample_info() const {
      return video_sample_info_;
    }

   private:
    SbMediaVideoSampleInfoWithOptionalColorMetadata video_sample_info_;
  };

  explicit VideoDmpReader(const char* filename);
  ~VideoDmpReader();

  SbMediaAudioCodec audio_codec() const { return audio_codec_; }
  const SbMediaAudioSampleInfo& audio_sample_info() const {
    return audio_sample_info_;
  }
  int64_t audio_bitrate() const { return audio_bitrate_; }

  SbMediaVideoCodec video_codec() const { return video_codec_; }
  int64_t video_bitrate() const { return video_bitrate_; }
  int video_fps() const { return video_fps_; }

  size_t number_of_audio_buffers() const { return audio_access_units_.size(); }

  size_t number_of_video_buffers() const { return video_access_units_.size(); }

  SbPlayerSampleInfo GetPlayerSampleInfo(SbMediaType type, size_t index) const;
  SbMediaAudioSampleInfo GetAudioSampleInfo(size_t index) const;

 private:
  VideoDmpReader(const VideoDmpReader&) = delete;
  VideoDmpReader& operator=(const VideoDmpReader&) = delete;

  void Parse();
  AudioAccessUnit ReadAudioAccessUnit();
  VideoAccessUnit ReadVideoAccessUnit();
  int ReadFromFile(ScopedFile* file, void* buffer, int bytes_to_read);

  ReadCB read_cb_;

  bool reverse_byte_order_;

  SbMediaAudioCodec audio_codec_ = kSbMediaAudioCodecNone;
  SbMediaAudioSampleInfoWithConfig audio_sample_info_;
  int64_t audio_bitrate_ = 0;

  SbMediaVideoCodec video_codec_ = kSbMediaVideoCodecNone;
  int64_t video_bitrate_ = 0;
  int video_fps_ = 0;

  std::vector<AudioAccessUnit> audio_access_units_;
  std::vector<VideoAccessUnit> video_access_units_;
};

}  // namespace video_dmp
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // SB_HAS(PLAYER_FILTER_TESTS)
#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_VIDEO_DMP_READER_H_
