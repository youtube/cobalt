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

#include "starboard/tvos/shared/media/vpx_loop_filter_manager.h"

#include <algorithm>

namespace starboard {
namespace shared {
namespace uikit {

namespace {

bool IsMaxResolution(bool is_4k_supported, int frame_width, int frame_height) {
  if (is_4k_supported) {
    return frame_width > 2560 || frame_height > 1440;
  }
  return false;
}

}  // namespace

VpxLoopFilterManager::VpxLoopFilterManager(
    bool is_4k_supported,
    size_t minimum_preroll_frame_count,
    size_t disable_loop_filter_low_watermark,
    size_t disable_loop_filter_high_watermark)
    : is_4k_supported_(is_4k_supported),
      minimum_preroll_frame_count_(minimum_preroll_frame_count),
      disable_loop_filter_low_watermark_(disable_loop_filter_low_watermark),
      disable_loop_filter_high_watermark_(disable_loop_filter_high_watermark) {
  SB_DCHECK(disable_loop_filter_low_watermark_ <=
            disable_loop_filter_high_watermark_);
  preroll_frame_count_.store(disable_loop_filter_high_watermark);
}

void VpxLoopFilterManager::Update(starboard::player::InputBuffer* input_buffer,
                                  int number_of_frames_in_backlog,
                                  int pending_buffers_total_size_in_bytes,
                                  int pending_buffer_count) {
  SB_DCHECK(number_of_frames_in_backlog >= 0);
  max_number_of_frames_in_backlog_ =
      std::max(number_of_frames_in_backlog, max_number_of_frames_in_backlog_);
  // Don't skip loop filter when the value of |max_number_of_frames_in_backlog_|
  // is still going up, even if the number of frames in the backlog is below
  // the low watermark.
  if (number_of_frames_in_backlog < disable_loop_filter_low_watermark_ &&
      number_of_frames_in_backlog < max_number_of_frames_in_backlog_ - 2) {
    is_loop_filter_disabled_ = true;
  }
  if (is_loop_filter_disabled_ &&
      number_of_frames_in_backlog > disable_loop_filter_high_watermark_) {
    is_loop_filter_disabled_ = false;
  }

  ++input_buffer_written_;
  if (input_buffer_written_ > 2) {
    // We need at least a certain number of pending buffers to calculate the
    // bitrate, as otherwise the calculated bitrate is unrepresentative.
    static const size_t kMinimumNumberOfBuffersForBitrateCalculation = 16;
    if (number_of_frames_in_backlog > minimum_preroll_frame_count_ &&
        pending_buffer_count >= kMinimumNumberOfBuffersForBitrateCalculation) {
      // Calculate the bitrate of the input buffers cached, assuming the video
      // is 30 fps.
      auto bitrate =
          pending_buffers_total_size_in_bytes * 8 * 30 / pending_buffer_count;
      if (bitrate <= kMinimumPrerollFrameCountBitrateThreshold &&
          preroll_frame_count_.load() > number_of_frames_in_backlog) {
        SB_LOG(INFO) << "Set preroll frame count to "
                     << number_of_frames_in_backlog
                     << " because bit rate is at " << bitrate;
        preroll_frame_count_.store(number_of_frames_in_backlog);
      }
    }
    return;
  }

  if (input_buffer_written_ == 1) {
    SB_DCHECK(input_buffer->video_sample_info().is_key_frame);
    first_input_buffer_timestamp_ = input_buffer->timestamp();
    auto frame_width = input_buffer->video_stream_info().frame_width;
    auto frame_height = input_buffer->video_stream_info().frame_height;
    if (!IsMaxResolution(is_4k_supported_, frame_width, frame_height)) {
      SB_LOG(INFO) << "Set preroll frame count to "
                   << minimum_preroll_frame_count_
                   << " because video resolution is at " << frame_width << " * "
                   << frame_height;
      preroll_frame_count_.store(minimum_preroll_frame_count_);
    }

    return;
  }

  auto timestamp = input_buffer->timestamp();
  SB_DCHECK(timestamp >= first_input_buffer_timestamp_);
  if (timestamp > first_input_buffer_timestamp_) {
    auto fps = 1000000 / (timestamp - first_input_buffer_timestamp_);
    if (fps <= kMinimumPrerollFrameCountFpsThreshold) {
      SB_LOG(INFO) << "Set preroll frame count to "
                   << minimum_preroll_frame_count_
                   << " because video fps is at " << fps;
      preroll_frame_count_.store(minimum_preroll_frame_count_);
    }
  }
}

bool VpxLoopFilterManager::is_loop_filter_disabled() const {
  return is_loop_filter_disabled_;
}

void VpxLoopFilterManager::Reset() {
  preroll_frame_count_.store(
      static_cast<int32_t>(disable_loop_filter_high_watermark_));

  max_number_of_frames_in_backlog_ = 0;
  input_buffer_written_ = 0;
  first_input_buffer_timestamp_ = 0;
  is_loop_filter_disabled_ = false;
}

}  // namespace uikit
}  // namespace shared
}  // namespace starboard
