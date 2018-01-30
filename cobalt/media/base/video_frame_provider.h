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

#ifndef COBALT_MEDIA_BASE_VIDEO_FRAME_PROVIDER_H_
#define COBALT_MEDIA_BASE_VIDEO_FRAME_PROVIDER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/time.h"
#include "starboard/decode_target.h"

namespace cobalt {
namespace media {

// The VideoFrameProvider manages the backlog for video frames. It has the
// following functionalities:
// 1. It caches the video frames ready to be displayed.
// 2. It decides which frame to be displayed at the current time.
// 3. It removes frames that will no longer be displayed.
class VideoFrameProvider
    : public base::RefCountedThreadSafe<VideoFrameProvider> {
 public:
  enum OutputMode {
    kOutputModePunchOut,
    kOutputModeDecodeToTexture,
    kOutputModeInvalid,
  };

  VideoFrameProvider() : output_mode_(kOutputModeInvalid) {}

  typedef base::Callback<SbDecodeTarget()> GetCurrentSbDecodeTargetFunction;

  void SetOutputMode(OutputMode output_mode) {
    base::AutoLock auto_lock(lock_);
    output_mode_ = output_mode;
  }

  VideoFrameProvider::OutputMode GetOutputMode() const {
    base::AutoLock auto_lock(lock_);
    return output_mode_;
  }

  // For Starboard platforms that have a decode-to-texture player, we enable
  // this VideoFrameProvider to act as a bridge for Cobalt code to query
  // for the current SbDecodeTarget.  In effect, we bypass all of
  // VideoFrameProvider's logic in this case, instead relying on the
  // Starboard implementation to provide us with the current video frame when
  // needed.
  void SetGetCurrentSbDecodeTargetFunction(
      GetCurrentSbDecodeTargetFunction function) {
    base::AutoLock auto_lock(lock_);
    get_current_sb_decode_target_function_ = function;
  }

  void ResetGetCurrentSbDecodeTargetFunction() {
    base::AutoLock auto_lock(lock_);
    get_current_sb_decode_target_function_.Reset();
  }

  SbDecodeTarget GetCurrentSbDecodeTarget() const {
    base::AutoLock auto_lock(lock_);
    if (get_current_sb_decode_target_function_.is_null()) {
      return kSbDecodeTargetInvalid;
    } else {
      return get_current_sb_decode_target_function_.Run();
    }
  }

 private:
  mutable base::Lock lock_;

  OutputMode output_mode_;
  GetCurrentSbDecodeTargetFunction get_current_sb_decode_target_function_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameProvider);
};

}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_BASE_VIDEO_FRAME_PROVIDER_H_
