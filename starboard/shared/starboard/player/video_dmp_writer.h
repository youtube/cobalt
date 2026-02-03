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

#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/shared/starboard/player/video_dmp_common.h"

namespace starboard {

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
                             const SbMediaAudioStreamInfo* audio_stream_info);
  static void OnPlayerWriteSample(SbPlayer player,
                                  const InputBuffer& input_buffer);
  static void OnPlayerDestroy(SbPlayer player);

 private:
  void DumpConfigs(SbMediaVideoCodec video_codec,
                   SbMediaAudioCodec audio_codec,
                   const SbMediaAudioStreamInfo* audio_stream_info);
  void DumpAccessUnit(const InputBuffer& input_buffer);
  int WriteToFile(const void* buffer, int size);

  int file_;
  WriteCB write_cb_;
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_VIDEO_DMP_WRITER_H_
