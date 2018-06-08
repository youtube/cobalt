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

#include "starboard/player.h"

#include "starboard/log.h"
#include "starboard/shared/starboard/player/player_internal.h"
#if SB_PLAYER_ENABLE_VIDEO_DUMPER && SB_HAS(PLAYER_FILTER_TESTS)
#include "starboard/shared/starboard/player/video_dmp_writer.h"
#endif  // SB_PLAYER_ENABLE_VIDEO_DUMPER && SB_HAS(PLAYER_FILTER_TESTS)

#if SB_API_VERSION >= SB_DEPRECATE_SB_MEDIA_TIME_API_VERSION
void SbPlayerWriteSample2(SbPlayer player,
                          SbMediaType sample_type,
                          const void* const* sample_buffers,
                          const int* sample_buffer_sizes,
                          int number_of_sample_buffers,
                          SbTime sample_timestamp,
                          const SbMediaVideoSampleInfo* video_sample_info,
                          const SbDrmSampleInfo* sample_drm_info) {
  if (!SbPlayerIsValid(player)) {
    SB_DLOG(WARNING) << "player is invalid.";
    return;
  }

  if (number_of_sample_buffers < 1) {
    SB_DLOG(WARNING) << "SbPlayerWriteSample() doesn't support"
                     << " |number_of_sample_buffers| less than one.";
    return;
  }

  if (sample_buffers == NULL) {
    SB_DLOG(WARNING) << "|sample_buffers| cannot be NULL";
    return;
  }

  if (sample_buffer_sizes == NULL) {
    SB_DLOG(WARNING) << "|sample_buffer_sizes| cannot be NULL";
    return;
  }

#if SB_PLAYER_ENABLE_VIDEO_DUMPER && SB_HAS(PLAYER_FILTER_TESTS)
  using ::starboard::shared::starboard::player::video_dmp::VideoDmpWriter;
  VideoDmpWriter::OnPlayerWriteSample(
      player, sample_type, sample_buffers, sample_buffer_sizes,
      number_of_sample_buffers, sample_timestamp, video_sample_info,
      sample_drm_info);
#endif  // SB_PLAYER_ENABLE_VIDEO_DUMPER && SB_HAS(PLAYER_FILTER_TESTS)

  player->WriteSample(sample_type, sample_buffers, sample_buffer_sizes,
                      number_of_sample_buffers, sample_timestamp,
                      video_sample_info, sample_drm_info);
}
#endif  // SB_API_VERSION >= SB_DEPRECATE_SB_MEDIA_TIME_API_VERSION
