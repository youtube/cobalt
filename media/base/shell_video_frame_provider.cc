/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "media/base/shell_video_frame_provider.h"

#include "base/logging.h"
#if SB_API_VERSION >= 4
#include "starboard/decode_target.h"
#endif  // #if SB_API_VERSION >= 4

namespace media {

ShellVideoFrameProvider::ShellVideoFrameProvider(
    scoped_refptr<VideoFrame> punch_out)
    : punch_out_(punch_out), has_consumed_frames_(false), dropped_frames_(0),
      output_mode_(kOutputModeInvalid) {
#if !defined(__LB_SHELL__FOR_RELEASE__)
  max_delay_in_microseconds_ = 0;
#endif  // !defined(__LB_SHELL__FOR_RELEASE__)
}

void ShellVideoFrameProvider::RegisterMediaTimeAndSeekingStateCB(
    const MediaTimeAndSeekingStateCB& media_time_and_seeking_state_cb) {
  DCHECK(!media_time_and_seeking_state_cb.is_null());
  base::AutoLock auto_lock(frames_lock_);
  media_time_and_seeking_state_cb_ = media_time_and_seeking_state_cb;
}

void ShellVideoFrameProvider::UnregisterMediaTimeAndSeekingStateCB(
    const MediaTimeAndSeekingStateCB& media_time_and_seeking_state_cb) {
  base::AutoLock auto_lock(frames_lock_);
  // It is possible that the register of a new callback happens earlier than the
  // unregister of the previous callback.  Always ensure that the callback
  // passed in is the current one before resetting.
  if (media_time_and_seeking_state_cb_.Equals(
          media_time_and_seeking_state_cb)) {
    media_time_and_seeking_state_cb_.Reset();
  }
}

const scoped_refptr<VideoFrame>& ShellVideoFrameProvider::GetCurrentFrame() {
  if (punch_out_) {
    current_frame_ = punch_out_;
    return current_frame_;
  }

  const int kEpsilonInMicroseconds =
      base::Time::kMicrosecondsPerSecond / 60 / 2;

  base::AutoLock auto_lock(frames_lock_);

  base::TimeDelta media_time;
  bool is_seeking;
  GetMediaTimeAndSeekingState_Locked(&media_time, &is_seeking);
  while (!frames_.empty()) {
    int64_t frame_time = frames_[0]->GetTimestamp().InMicroseconds();
    if (frame_time >= media_time.InMicroseconds())
      break;
    if (current_frame_ != frames_[0] &&
        frame_time + kEpsilonInMicroseconds >= media_time.InMicroseconds())
      break;

    if (current_frame_ != frames_[0] && !is_seeking) {
      ++dropped_frames_;

#if !defined(__LB_SHELL__FOR_RELEASE__)
      if (media_time.InMicroseconds() - frame_time > max_delay_in_microseconds_)
        max_delay_in_microseconds_ = media_time.InMicroseconds() - frame_time;
      const bool kLogFrameDrops ALLOW_UNUSED = false;
      LOG_IF(WARNING, kLogFrameDrops)
          << "dropped one frame with timestamp "
          << frames_[0]->GetTimestamp().InMicroseconds() << " at media time "
          << media_time.InMicroseconds() << " total dropped " << dropped_frames_
          << " frames with a max delay of " << max_delay_in_microseconds_
          << " ms";
#endif  // !defined(__LB_SHELL__FOR_RELEASE__)
    }

    if (frames_.size() == 1) {
      current_frame_ = frames_[0];
    }

    frames_.erase(frames_.begin());
    has_consumed_frames_ = true;
  }
  if (!frames_.empty()) {
    current_frame_ = frames_[0];
  }
  return current_frame_;
}

void ShellVideoFrameProvider::SetOutputMode(OutputMode output_mode) {
  base::AutoLock auto_lock(frames_lock_);
  output_mode_ = output_mode;
}

ShellVideoFrameProvider::OutputMode ShellVideoFrameProvider::GetOutputMode()
    const {
  base::AutoLock auto_lock(frames_lock_);
  return output_mode_;
}

#if SB_API_VERSION >= 4

void ShellVideoFrameProvider::SetGetCurrentSbDecodeTargetFunction(
    GetCurrentSbDecodeTargetFunction function) {
  base::AutoLock auto_lock(frames_lock_);
  get_current_sb_decode_target_function_ = function;
}

void ShellVideoFrameProvider::ResetGetCurrentSbDecodeTargetFunction() {
  base::AutoLock auto_lock(frames_lock_);
  get_current_sb_decode_target_function_.Reset();
}

SbDecodeTarget ShellVideoFrameProvider::GetCurrentSbDecodeTarget() const {
  base::AutoLock auto_lock(frames_lock_);
  if (get_current_sb_decode_target_function_.is_null()) {
    return kSbDecodeTargetInvalid;
  } else {
    return get_current_sb_decode_target_function_.Run();
  }
}

#endif  // #if SB_API_VERSION >= 4

void ShellVideoFrameProvider::AddFrame(const scoped_refptr<VideoFrame>& frame) {
  base::AutoLock auto_lock(frames_lock_);
  frames_.push_back(frame);
}

void ShellVideoFrameProvider::Flush() {
  base::AutoLock auto_lock(frames_lock_);
  frames_.clear();
}

void ShellVideoFrameProvider::Stop() {
  base::AutoLock auto_lock(frames_lock_);
  frames_.clear();
  current_frame_ = NULL;
  dropped_frames_ = 0;
}

size_t ShellVideoFrameProvider::GetNumOfFramesCached() const {
  base::AutoLock auto_lock(frames_lock_);
  return frames_.size();
}

void ShellVideoFrameProvider::GetMediaTimeAndSeekingState_Locked(
    base::TimeDelta* media_time,
    bool* is_seeking) const {
  DCHECK(media_time);
  DCHECK(is_seeking);
  frames_lock_.AssertAcquired();
  if (media_time_and_seeking_state_cb_.is_null()) {
    *media_time = base::TimeDelta();
    *is_seeking = false;
    return;
  }
  media_time_and_seeking_state_cb_.Run(media_time, is_seeking);
}

bool ShellVideoFrameProvider::QueryAndResetHasConsumedFrames() {
  base::AutoLock auto_lock(frames_lock_);
  bool previous_value = has_consumed_frames_;
  has_consumed_frames_ = false;
  return previous_value;
}

int ShellVideoFrameProvider::ResetAndReturnDroppedFrames() {
  base::AutoLock auto_lock(frames_lock_);
  int dropped_frames = dropped_frames_;
  dropped_frames_ = 0;
  return dropped_frames;
}

}  // namespace media
