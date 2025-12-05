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

#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "starboard/common/file.h"
#include "starboard/common/log.h"
#include "starboard/common/ref_counted.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/media/media_util.h"
#include "starboard/shared/starboard/player/file_cache_reader.h"
#include "starboard/shared/starboard/player/video_dmp_common.h"

namespace starboard {

class VideoDmpReader {
 public:
  enum ReadOnDemandOptions {
    kDisableReadOnDemand,
    kEnableReadOnDemand,
  };

  class AccessUnit {
   public:
    AccessUnit(int64_t timestamp,
               const SbDrmSampleInfoWithSubSampleMapping* drm_sample_info,
               std::vector<uint8_t> data)
        : timestamp_(timestamp),
          drm_sample_info_present_(drm_sample_info != NULL),
          drm_sample_info_(drm_sample_info == NULL
                               ? SbDrmSampleInfoWithSubSampleMapping()
                               : *drm_sample_info),
          data_(std::move(data)) {}
    int64_t timestamp() const { return timestamp_; }
    const SbDrmSampleInfo* drm_sample_info() const {
      return drm_sample_info_present_ ? &drm_sample_info_ : NULL;
    }
    const std::vector<uint8_t>& data() const { return data_; }

   private:
    int64_t timestamp_;  // microseconds
    bool drm_sample_info_present_;
    SbDrmSampleInfoWithSubSampleMapping drm_sample_info_;
    std::vector<uint8_t> data_;
  };

  class AudioAccessUnit : public AccessUnit {
   public:
    AudioAccessUnit(int64_t timestamp,
                    const SbDrmSampleInfoWithSubSampleMapping* drm_sample_info,
                    std::vector<uint8_t> data,
                    AudioSampleInfo audio_sample_info)
        : AccessUnit(timestamp, drm_sample_info, std::move(data)),
          audio_sample_info_(std::move(audio_sample_info)) {}
    const AudioSampleInfo& audio_sample_info() const {
      return audio_sample_info_;
    }

   private:
    AudioSampleInfo audio_sample_info_;
  };

  class VideoAccessUnit : public AccessUnit {
   public:
    VideoAccessUnit(int64_t timestamp,
                    const SbDrmSampleInfoWithSubSampleMapping* drm_sample_info,
                    std::vector<uint8_t> data,
                    VideoSampleInfo video_sample_info)
        : AccessUnit(timestamp, drm_sample_info, std::move(data)),
          video_sample_info_(std::move(video_sample_info)) {}
    const VideoSampleInfo& video_sample_info() const {
      return video_sample_info_;
    }

   private:
    VideoSampleInfo video_sample_info_;
  };

  explicit VideoDmpReader(
      const char* filename,
      ReadOnDemandOptions read_on_demand_options = kDisableReadOnDemand);
  ~VideoDmpReader();

  SbMediaAudioCodec audio_codec() const { return dmp_info_.audio_codec; }
  const AudioStreamInfo& audio_stream_info() const {
    return dmp_info_.audio_sample_info.stream_info;
  }
  const VideoStreamInfo& video_stream_info() {
    EnsureSampleLoaded(kSbMediaTypeVideo, 0);
    SB_DCHECK(!video_access_units_.empty());
    return video_access_units_[0].video_sample_info().stream_info;
  }
  int64_t audio_bitrate() const { return dmp_info_.audio_bitrate; }
  std::string audio_mime_type() const;
  int64_t audio_duration() const { return dmp_info_.audio_duration; }

  SbMediaVideoCodec video_codec() const { return dmp_info_.video_codec; }
  int64_t video_bitrate() const { return dmp_info_.video_bitrate; }
  int video_fps() const { return dmp_info_.video_fps; }
  std::string video_mime_type();
  int64_t video_duration() const { return dmp_info_.video_duration; }

  size_t number_of_audio_buffers() const {
    return dmp_info_.audio_access_units_size;
  }
  size_t number_of_video_buffers() const {
    return dmp_info_.video_access_units_size;
  }

  SbPlayerSampleInfo GetPlayerSampleInfo(SbMediaType type, size_t index);
  const AudioSampleInfo& GetAudioSampleInfo(size_t index);

 private:
  struct DmpInfo {
    SbMediaAudioCodec audio_codec = kSbMediaAudioCodecNone;
    AudioSampleInfo audio_sample_info;
    size_t audio_access_units_size = 0;
    int64_t audio_bitrate = 0;
    int audio_duration = 0;

    SbMediaVideoCodec video_codec = kSbMediaVideoCodecNone;
    size_t video_access_units_size = 0;
    int64_t video_bitrate = 0;
    int video_fps = 0;
    int video_duration = 0;
  };

  class Registry {
   public:
    bool GetDmpInfo(const std::string& filename, DmpInfo* dmp_info) const;
    void Register(const std::string& filename, const DmpInfo& dmp_info);

   private:
    mutable std::mutex mutex_;
    std::map<std::string, DmpInfo> dmp_infos_;
  };

  VideoDmpReader(const VideoDmpReader&) = delete;
  VideoDmpReader& operator=(const VideoDmpReader&) = delete;

  void ParseHeader(uint32_t* dmp_writer_version);
  bool ParseOneRecord();
  void Parse();
  void EnsureSampleLoaded(SbMediaType type, size_t index);

  AudioAccessUnit ReadAudioAccessUnit();
  VideoAccessUnit ReadVideoAccessUnit();
  static Registry* GetRegistry();

  const bool allow_read_on_demand_;

  FileCacheReader file_reader_;
  ReadCB read_cb_;
  DmpInfo dmp_info_;

  std::optional<bool> reverse_byte_order_;

  std::vector<AudioAccessUnit> audio_access_units_;
  std::vector<VideoAccessUnit> video_access_units_;
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_VIDEO_DMP_READER_H_
