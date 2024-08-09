// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_ANDROID_SHARED_MEDIA_CODEC_BRIDGE_ERADICATOR_H_
#define STARBOARD_ANDROID_SHARED_MEDIA_CODEC_BRIDGE_ERADICATOR_H_

#include <jni.h>
#include <atomic>
#include <set>

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/common/condition_variable.h"
#include "starboard/common/mutex.h"

namespace starboard {
namespace android {
namespace shared {

/**
 * This class is a singleton utility that runs the MediaCodecBridge destructor
 * on a separate thread to prevent Application Not Responding (ANR) errors in
 * Android.
 */
class MediaCodecBridgeEradicator {
 public:
  static MediaCodecBridgeEradicator* GetInstance();

  ~MediaCodecBridgeEradicator() { WaitForPendingDestructions(); }

  bool Destroy(jobject j_media_codec_bridge,
               jobject j_reused_get_output_format_result);
  // Returns false if the wait for pending destructions has timed out.
  bool WaitForPendingDestructions();
  bool IsEnabled() const { return is_enabled_.load(); }
  void SetEnabled(bool enabled) { is_enabled_.store(enabled); }
  int GetTimeoutSeconds() const { return timeout_seconds_; }
  void SetTimeoutSeconds(int timeout_seconds) {
    timeout_seconds_ = timeout_seconds;
  }

 private:
  static void* DestroyMediaCodecBridge(void* context);

  std::atomic_bool is_enabled_{false};
  // Maximum wait time when creating a new MediaCodecBridge if the background
  // cleanup thread (MediaCodecBridgeEradicator::DestroyMediaCodecBridge) is
  // unresponsive.
  int timeout_seconds_ = 2;
  Mutex mutex_;
  ConditionVariable condition_variable_{mutex_};
  std::set<jobject> j_media_codec_bridge_set_;
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_MEDIA_CODEC_BRIDGE_ERADICATOR_H_
