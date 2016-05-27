// Copyright 2016 Google Inc. All Rights Reserved.
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

// Provides definitions useful for working with SbOnce implemented using
// SbAtomic32.

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_INPUT_BUFFER_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_INPUT_BUFFER_INTERNAL_H_

#include "starboard/log.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/internal_only.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {

class InputBuffer {
 public:
  InputBuffer(SbPlayerDeallocateSampleFunc deallocate_sample_func,
              SbPlayer player,
              void* context,
              const void* sample_buffer,
              int sample_buffer_size,
              SbMediaTime sample_pts,
              const SbMediaVideoSampleInfo* video_sample_info,
              const SbDrmSampleInfo* sample_drm_info)
      : deallocate_sample_func_(deallocate_sample_func),
        player_(player),
        context_(context),
        data_(reinterpret_cast<const uint8_t*>(sample_buffer)),
        size_(sample_buffer_size),
        pts_(sample_pts),
        has_video_sample_info_(video_sample_info != NULL),
        has_drm_info_(sample_drm_info != NULL) {
    SB_DCHECK(deallocate_sample_func);
    if (has_video_sample_info_) {
      video_sample_info_ = *video_sample_info;
    }
    if (has_drm_info_) {
      drm_info_ = *sample_drm_info;
    }
  }
  ~InputBuffer() {
    deallocate_sample_func_(player_, context_, const_cast<uint8_t*>(data_));
  }

  const uint8_t* data() const { return data_; }
  int size() const { return size_; }
  SbMediaTime pts() const { return pts_; }
  const SbMediaVideoSampleInfo* video_sample_info() const {
    return has_video_sample_info_ ? &video_sample_info_ : NULL;
  }
  const SbDrmSampleInfo* drm_info() const {
    return has_drm_info_ ? &drm_info_ : NULL;
  }

 private:
  SbPlayerDeallocateSampleFunc deallocate_sample_func_;
  SbPlayer player_;
  void* context_;
  const uint8_t* data_;
  int size_;
  SbMediaTime pts_;
  bool has_video_sample_info_;
  SbMediaVideoSampleInfo video_sample_info_;
  bool has_drm_info_;
  SbDrmSampleInfo drm_info_;
};

}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_INPUT_BUFFER_INTERNAL_H_
