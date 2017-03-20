// Copyright 2015 Google Inc. All Rights Reserved.
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

#ifndef COBALT_MEDIA_BASE_SHELL_VIDEO_FRAME_PROVIDER_H_
#define COBALT_MEDIA_BASE_SHELL_VIDEO_FRAME_PROVIDER_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/time.h"
#include "starboard/decode_target.h"

namespace media {

// TODO: The following class is tentative to make the new media stack work.
//       We should consider remove VideoFrame as it is no longer useful.
class VideoFrame : public base::RefCountedThreadSafe<VideoFrame> {
 public:
  int texture_id() const { return 0; }
  base::TimeDelta GetTimestamp() const { return base::TimeDelta(); }
};

// TODO: Remove Shell prefix.
// The ShellVideoFrameProvider manages the backlog for video frames. It has the
// following functionalities:
// 1. It caches the video frames ready to be displayed.
// 2. It decides which frame to be displayed at the current time.
// 3. It removes frames that will no longer be displayed.
class ShellVideoFrameProvider
    : public base::RefCountedThreadSafe<ShellVideoFrameProvider> {
 public:
  enum OutputMode {
    kOutputModePunchOut,
    kOutputModeDecodeToTexture,
    kOutputModeInvalid,
  };

  ShellVideoFrameProvider() : output_mode_(kOutputModePunchOut) {}

#if SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION
  typedef base::Callback<SbDecodeTarget()> GetCurrentSbDecodeTargetFunction;
#endif  // SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION

  scoped_refptr<VideoFrame> GetCurrentFrame() { return NULL; }

  void SetOutputMode(OutputMode output_mode) { output_mode_ = output_mode; }
  OutputMode GetOutputMode() const { return output_mode_; }

#if SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION
  // For Starboard platforms that have a decode-to-texture player, we enable
  // this ShellVideoFrameProvider to act as a bridge for Cobalt code to query
  // for the current SbDecodeTarget.  In effect, we bypass all of
  // ShellVideoFrameProvider's logic in this case, instead relying on the
  // Starboard implementation to provide us with the current video frame when
  // needed.
  void SetGetCurrentSbDecodeTargetFunction(
      GetCurrentSbDecodeTargetFunction function) {
    get_current_sb_decode_target_function_ = function;
  }

  void ResetGetCurrentSbDecodeTargetFunction() {
    get_current_sb_decode_target_function_.Reset();
  }

  SbDecodeTarget GetCurrentSbDecodeTarget() const {
    if (get_current_sb_decode_target_function_.is_null()) {
      return kSbDecodeTargetInvalid;
    } else {
      return get_current_sb_decode_target_function_.Run();
    }
  }
#endif  // SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION

 private:
  OutputMode output_mode_;
#if SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION
  GetCurrentSbDecodeTargetFunction get_current_sb_decode_target_function_;
#endif  // SB_API_VERSION >= SB_PLAYER_DECODE_TO_TEXTURE_API_VERSION

  DISALLOW_COPY_AND_ASSIGN(ShellVideoFrameProvider);
};

}  // namespace media

#endif  // COBALT_MEDIA_BASE_SHELL_VIDEO_FRAME_PROVIDER_H_
