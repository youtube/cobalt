// Copyright 2018 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_VIDEO_DMP_WRITER_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_VIDEO_DMP_WRITER_H_

#include "starboard/file.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/starboard/player/video_dmp_common.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace video_dmp {

// This class saves video data according to the format specified inside
// video_dmp_common.h.
class VideoDmpWriter {
 public:
  VideoDmpWriter();
  ~VideoDmpWriter();

  static void OnPlayerCreate(SbPlayer player,
                             SbMediaVideoCodec video_codec,
                             SbMediaAudioCodec audio_codec,
                             SbDrmSystem drm_system,
                             const SbMediaAudioHeader* audio_header);
  static void OnPlayerWriteSample(
      SbPlayer player,
      SbMediaType sample_type,
      const void* const* sample_buffers,
      const int* sample_buffer_sizes,
      int number_of_sample_buffers,
      SbTime sample_timestamp,
      const SbMediaVideoSampleInfo* video_sample_info,
      const SbDrmSampleInfo* drm_sample_info);
  static void OnPlayerDestroy(SbPlayer player);

 private:
  void DumpConfigs(SbMediaVideoCodec video_codec,
                   SbMediaAudioCodec audio_codec,
                   const SbMediaAudioHeader* audio_header);
  void DumpAccessUnit(SbMediaType sample_type,
                      const void* const* sample_buffers,
                      const int* sample_buffer_sizes,
                      int number_of_sample_buffers,
                      SbTime sample_timestamp,
                      const SbMediaVideoSampleInfo* video_sample_info,
                      const SbDrmSampleInfo* drm_sample_info);
  int WriteToFile(const void* buffer, int size);

  SbFile file_;
  WriteCB write_cb_;
};

}  // namespace video_dmp
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_VIDEO_DMP_WRITER_H_
