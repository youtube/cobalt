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

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/time.h"

namespace starboard {
namespace android {
namespace shared {
namespace {

const SbTime kMaxAllowedSkew = 5000;

}  // namespace

SbTime VideoFrameTracker::seek_to_time() const {
  return seek_to_time_;
}

void VideoFrameTracker::OnInputBuffer(SbTime timestamp) {
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
  ScopedLock lock(rendered_frames_mutex_);
  rendered_frames_on_decoder_thread_.push_back(frame_timestamp);
}

void VideoFrameTracker::Seek(SbTime seek_to_time) {
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

void VideoFrameTracker::UpdateDroppedFrames() {
  SB_DCHECK(thread_checker_.CalledOnValidThread());

  {
    ScopedLock lock(rendered_frames_mutex_);
    rendered_frames_on_tracker_thread_.swap(rendered_frames_on_decoder_thread_);
  }

  while (frames_to_be_rendered_.front() < seek_to_time_) {
    // It is possible that the initial frame rendered time is before the
    // seek to time, when the platform decides to render a frame earlier
    // than the seek to time during preroll. This shouldn't be an issue
    // after we align seek time to the next video key frame.
    frames_to_be_rendered_.pop_front();
  }

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
