// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_TVOS_SHARED_MEDIA_VPX_LOOP_FILTER_MANAGER_H_
#define STARBOARD_TVOS_SHARED_MEDIA_VPX_LOOP_FILTER_MANAGER_H_

#include "starboard/common/atomic.h"
#include "starboard/shared/starboard/player/input_buffer_internal.h"
#include "starboard/types.h"

namespace starboard {
namespace shared {
namespace uikit {

class VpxLoopFilterManager {
 public:
  VpxLoopFilterManager(bool is_4k_supported,
                       size_t minimum_preroll_frame_count,
                       size_t disable_loop_filter_low_watermark,
                       size_t disable_loop_filter_high_watermark);

  // The following functions are called from the vpx_video_decoder thread.
  void Update(starboard::player::InputBuffer* input_buffer,
              int number_of_frames_in_backlog,
              int pending_buffers_total_size_in_bytes,
              int pending_buffer_count);
  bool is_loop_filter_disabled() const;

  // The following function is called from the player_worker thread, when all
  // decoder threads are torn down.
  void Reset();

  // This function can be called from any thread.
  size_t GetPrerollFrameCount() const { return preroll_frame_count_.load(); }

 private:
  // Any fps below this can use the |minimum_preroll_frame_count_| as the
  // preroll frame count, otherwise |disable_loop_filter_low_watermark_| is used
  // for GetPrerollFrameCount().
  static const int kMinimumPrerollFrameCountFpsThreshold = 25;

  // Any bitrate below this can use a reduced preroll frame count, otherwise
  // |disable_loop_filter_low_watermark_| is used for GetPrerollFrameCount().
  static const int kMinimumPrerollFrameCountBitrateThreshold = 15 * 1024 * 1024;

  const bool is_4k_supported_;
  const size_t minimum_preroll_frame_count_;
  const size_t disable_loop_filter_low_watermark_;
  const size_t disable_loop_filter_high_watermark_;

  atomic_int32_t preroll_frame_count_;

  int max_number_of_frames_in_backlog_ = 0;
  int input_buffer_written_ = 0;
  int64_t first_input_buffer_timestamp_ = 0;
  bool is_loop_filter_disabled_ = false;
};

}  // namespace uikit
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_TVOS_SHARED_MEDIA_VPX_LOOP_FILTER_MANAGER_H_
