// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/video_frame_tracker.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/common/mutex.h"

namespace starboard {
namespace android {
namespace shared {
namespace {

const int64_t kMaxAllowedSkew = 5'000;  // 5ms

}  // namespace

int64_t VideoFrameTracker::seek_to_time() const {
  return seek_to_time_;
}

void VideoFrameTracker::OnInputBuffer(int64_t timestamp) {
  SB_LOG(INFO) << __func__ << "> timestamp=" << timestamp;
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  if (frames_to_be_rendered_.empty()) {
    frames_to_be_rendered_.push_back(timestamp);
    return;
  }

  if (frames_to_be_rendered_.size() > max_pending_frames_size_) {
    // OnFrameRendered() is only available after API level 23.  Cap the size
    // of |frames_to_be_rendered_| in case OnFrameRendered() is not available.
    frames_to_be_rendered_.pop_front();
  }

  // Sort by |timestamp|, because |timestamp| won't be monotonic if there are
  // B frames.
  for (auto it = frames_to_be_rendered_.end();
       it != frames_to_be_rendered_.begin();) {
    it--;
    if (*it < timestamp) {
      frames_to_be_rendered_.emplace(++it, timestamp);
      return;
    } else if (*it == timestamp) {
      SB_LOG(WARNING) << "feed video AU with same time stamp " << timestamp;
      return;
    }
  }

  frames_to_be_rendered_.emplace_front(timestamp);
}

void VideoFrameTracker::OnFrameRendered(int64_t frame_timestamp) {
  SB_LOG(INFO) << __func__ << "> frame_timestamp=" << frame_timestamp;
  ScopedLock lock(rendered_frames_mutex_);
  rendered_frames_on_decoder_thread_.push_back(frame_timestamp);
}

void VideoFrameTracker::Seek(int64_t seek_to_time) {
  SB_LOG(INFO) << __func__ << "> seek_to_time=" << seek_to_time;
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  // Ensure that all dropped frames before seeking are captured.
  UpdateDroppedFrames();

  frames_to_be_rendered_.clear();
  seek_to_time_ = seek_to_time;
}

int VideoFrameTracker::UpdateAndGetDroppedFrames() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  UpdateDroppedFrames();
  return dropped_frames_;
}

void VideoFrameTracker::RemoveInvalidRenderedFrames() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());
  auto& rendered_frames = rendered_frames_on_tracker_thread_;

  if (rendered_frames.empty()) {
    return;
  }
  const int64_t min_valid_rendered_frame =
      std::max(0LL, frames_to_be_rendered_.front() - kMaxAllowedSkew);
  const int64_t max_valid_rendered_frame =
      frames_to_be_rendered_.back() + kMaxAllowedSkew;
  auto is_valid = [min_valid_rendered_frame,
                   max_valid_rendered_frame](int64_t timestamp) {
    return min_valid_rendered_frame <= timestamp &&
           timestamp <= max_valid_rendered_frame;
  };
  // Since rendered frames is sorted, all rendered frames are valid, if first
  // and last frame is valid.
  if (is_valid(rendered_frames.front()) && is_valid(rendered_frames.back())) {
    return;
  }

  int removed_rendered_frames = 0;
  auto it = rendered_frames.begin();
  while (it != rendered_frames.end()) {
    if (is_valid(*it)) {
      ++it;
      continue;
    }

    ++removed_rendered_frames;
    it = rendered_frames.erase(it);
  }

  SB_LOG(WARNING) << "Removed invalid timestamps. This can happen when "
                     "MediaCodec::flush() is called on Android SDK 14+. See "
                     "b/401790323#comment13: count="
                  << removed_rendered_frames;
}

void VideoFrameTracker::UpdateDroppedFrames() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  {
    ScopedLock lock(rendered_frames_mutex_);
    rendered_frames_on_tracker_thread_.swap(rendered_frames_on_decoder_thread_);
  }

  // Sort by |timestamp)

  while (!frames_to_be_rendered_.empty() &&
         frames_to_be_rendered_.front() < seek_to_time_) {
    // It is possible that the initial frame rendered time is before the
    // seek to time, when the platform decides to render a frame earlier
    // than the seek to time during preroll. This shouldn't be an issue
    // after we align seek time to the next video key frame.
    frames_to_be_rendered_.pop_front();
  }
  if (frames_to_be_rendered_.empty()) {
    rendered_frames_on_tracker_thread_.clear();
    return;
  }

  RemoveInvalidRenderedFrames();

  // Loop over all timestamps from OnFrameRendered and compare against ones from
  // OnInputBuffer.
  for (auto rendered_timestamp : rendered_frames_on_tracker_thread_) {
    auto to_render_timestamp = frames_to_be_rendered_.begin();
    // Loop over all frames to render until we've caught up to the timestamp of
    // the last rendered frame.
    while (to_render_timestamp != frames_to_be_rendered_.end() &&
           !(*to_render_timestamp - rendered_timestamp > kMaxAllowedSkew)) {
      if (std::abs(*to_render_timestamp - rendered_timestamp) <=
          kMaxAllowedSkew) {
        // This frame was rendered, remove it from frames_to_be_rendered_.
        to_render_timestamp = frames_to_be_rendered_.erase(to_render_timestamp);
      } else if (rendered_timestamp - *to_render_timestamp > kMaxAllowedSkew) {
        // The rendered frame is too far ahead. The to_render_timestamp frame
        // was dropped.
        SB_LOG(WARNING) << "Video frame dropped:" << *to_render_timestamp
                        << ", current frame timestamp:" << rendered_timestamp
                        << ", frames in the backlog:"
                        << frames_to_be_rendered_.size();
        ++dropped_frames_;
        to_render_timestamp = frames_to_be_rendered_.erase(to_render_timestamp);
      } else {
        // The rendered frame is too early to match the next frame to render.
        // This could happen if a frame is reported to be rendered twice or if
        // it is rendered more than kMaxAllowedSkew early. In the latter
        // scenario the frame will be reported dropped in the next iteration of
        // the outer loop.
        ++to_render_timestamp;
      }
    }
  }

  rendered_frames_on_tracker_thread_.clear();
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
