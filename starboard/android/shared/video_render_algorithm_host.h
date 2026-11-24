// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_ANDROID_SHARED_VIDEO_RENDER_ALGORITHM_HOST_H_
#define STARBOARD_ANDROID_SHARED_VIDEO_RENDER_ALGORITHM_HOST_H_

#include <jni.h>

#include <cstdint>

#include "starboard/player.h"
#include "third_party/jni_zero/jni_zero.h"

namespace starboard {

// Narrow interface implemented by MediaCodecVideoDecoder so video render
// algorithms can be tested deterministically without instantiating a codec.
class VideoRenderAlgorithmHost {
 public:
  virtual ~VideoRenderAlgorithmHost() = default;

  virtual void SetPlaybackRate(double playback_rate) = 0;
  virtual SbPlayerOutputMode GetOutputMode() const = 0;
  virtual jni_zero::ScopedJavaLocalRef<jobject> AcquireJavaSurface() = 0;
  virtual uint64_t GetSurfaceGeneration() const { return 0; }
  // Pass <= 0 to clear any custom wait override and restore default polling.
  virtual void SetNextTickWaitDurationUs(int64_t wait_duration_us) = 0;
  virtual bool is_playback_paused() const = 0;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_VIDEO_RENDER_ALGORITHM_HOST_H_
