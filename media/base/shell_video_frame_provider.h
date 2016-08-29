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

#ifndef MEDIA_BASE_SHELL_VIDEO_FRAME_PROVIDER_H_
#define MEDIA_BASE_SHELL_VIDEO_FRAME_PROVIDER_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "media/base/video_frame.h"

namespace media {

// TODO: Remove Shell prefix.
// The ShellVideoFrameProvider manages the backlog for video frames. It has the
// following functionalities:
// 1. It caches the video frames ready to be displayed.
// 2. It decides which frame to be displayed at the current time.
// 3. It removes frames that will no longer be displayed.
class ShellVideoFrameProvider
    : public base::RefCountedThreadSafe<ShellVideoFrameProvider> {
 public:
  explicit ShellVideoFrameProvider(scoped_refptr<VideoFrame> punch_out = NULL);

  typedef base::Callback<base::TimeDelta()> MediaTimeCB;
  // This class uses the media time to decide which frame is current.  It
  // retrieves the media time from the registered media_time_cb.  There can only
  // be one registered media_time_cb at a certain time, a call to
  // RegisterMediaTimeCB() will overwrite the previously registered callback.
  void RegisterMediaTimeCB(const MediaTimeCB& media_time_cb);
  // This function unregisters the media time callback if it hasn't been
  // overwritten by another callback.
  void UnregisterMediaTimeCB(const MediaTimeCB& media_time_cb);

  // Returns the current frame to be displayed if there is one. Otherwise it
  // returns NULL.
  const scoped_refptr<VideoFrame>& GetCurrentFrame();

  void AddFrame(const scoped_refptr<VideoFrame>& frame);
  // Flush will clear all cached frames except the current frame. So the current
  // frame can still be displayed during seek.
  void Flush();
  // Stop will clear all cached frames including the current frame.
  void Stop();
  size_t GetNumOfFramesCached() const;

  // Return true if VideoFrames have been released from the internal frames_
  // queue since the last time this was called.
  bool QueryAndResetHasConsumedFrames();

  // Return the value of |dropped_frames_| and reset it to 0.
  int ResetAndReturnDroppedFrames();

 private:
  base::TimeDelta GetMediaTime_Locked() const;

  scoped_refptr<VideoFrame> punch_out_;

  mutable base::Lock frames_lock_;
  MediaTimeCB media_time_cb_;
  std::vector<scoped_refptr<VideoFrame> > frames_;
  scoped_refptr<VideoFrame> current_frame_;
  bool has_consumed_frames_;
  int dropped_frames_;

#if !defined(__LB_SHELL__FOR_RELEASE__)
  int max_delay_in_microseconds_;
#endif  // !defined(__LB_SHELL__FOR_RELEASE__)

  DISALLOW_COPY_AND_ASSIGN(ShellVideoFrameProvider);
};

}  // namespace media

#endif  // MEDIA_BASE_SHELL_VIDEO_FRAME_PROVIDER_H_
