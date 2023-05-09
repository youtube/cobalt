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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_DMP_DEMUXER_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_DMP_DEMUXER_H_

#include <map>
#include <utility>

#include "starboard/shared/starboard/player/video_dmp_reader.h"
#include "starboard/time.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace video_dmp {

class DmpDemuxer {
 public:
  typedef std::map<SbMediaType, VideoDmpReader*> MediaTypeToDmpReaderMap;

  // Construct a new dmp demuxer by assigning dmp readers to each media track.
  explicit DmpDemuxer(const MediaTypeToDmpReaderMap& dmp_readers);
  ~DmpDemuxer() = default;

  int GetCurrentIndex(SbMediaType type) const;
  SbTime GetDuration() const { return duration_; }
  VideoDmpReader* GetDmpReader(SbMediaType type) const;
  SbMediaAudioCodec GetAudioCodec() const;
  SbMediaVideoCodec GetVideoCodec() const;
  void GetReadRange(SbTime* start, SbTime* end) const;
  const shared::starboard::media::AudioStreamInfo* GetAudioStreamInfo() const;
  bool HasMediaTrack(SbMediaType type) const {
    return dmp_readers_.count(type) > 0;
  }

  // Read a media sample without changing the reading index.
  // Return true if read is successful; otherwise, return false.
  bool PeekSample(SbMediaType type, SbPlayerSampleInfo* sample_info);
  bool ReadSample(SbMediaType type, SbPlayerSampleInfo* sample_info);

  bool Seek(SbTime time_to_seek_to);

  bool SetReadRange(SbTime start, SbTime end);

  DmpDemuxer() = delete;
  DmpDemuxer(const DmpDemuxer& other) = delete;
  DmpDemuxer& operator=(const DmpDemuxer& other) = delete;

 private:
  void Init();

  SbTime duration_ = kSbTimeMax;

  MediaTypeToDmpReaderMap dmp_readers_;

  std::map<SbMediaType, int> sample_index_;
  std::map<SbMediaType, int> num_of_samples_;

  std::pair<SbTime, SbTime> read_range_;
};

}  // namespace video_dmp
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_DMP_DEMUXER_H_
