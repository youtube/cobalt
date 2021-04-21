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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_VIDEO_DMP_WRITER_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_VIDEO_DMP_WRITER_H_

#include "starboard/file.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
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
                             SbMediaAudioCodec audio_codec,
                             SbMediaVideoCodec video_codec,
                             SbDrmSystem drm_system,
                             const SbMediaAudioSampleInfo* audio_sample_info);
  static void OnPlayerWriteSample(
      SbPlayer player,
      const scoped_refptr<InputBuffer>& input_buffer);
  static void OnPlayerDestroy(SbPlayer player);

 private:
  void DumpConfigs(SbMediaVideoCodec video_codec,
                   SbMediaAudioCodec audio_codec,
                   const SbMediaAudioSampleInfo* audio_sample_info);
  void DumpAccessUnit(const scoped_refptr<InputBuffer>& input_buffer);
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
