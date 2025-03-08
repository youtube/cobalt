// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_ANDROID_SHARED_EXOPLAYER_EXOPLAYER_BRIDGE_H_
#define STARBOARD_ANDROID_SHARED_EXOPLAYER_EXOPLAYER_BRIDGE_H_

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/android/shared/jni_utils.h"
#include "starboard/media.h"
#include "starboard/player.h"

namespace starboard {
namespace android {
namespace shared {

using starboard::android::shared::JniEnvExt;

class ExoPlayerBridge final {
 public:
  ExoPlayerBridge();
  ~ExoPlayerBridge();

  void WriteSamples(SbMediaType sample_type,
                    const SbPlayerSampleInfo* sample_infos,
                    int number_of_sample_infos,
                    JniEnvExt* env = JniEnvExt::Get()) const;
  bool Play(JniEnvExt* env = JniEnvExt::Get()) const;
  bool Pause(JniEnvExt* env = JniEnvExt::Get()) const;
  bool Stop(JniEnvExt* env = JniEnvExt::Get()) const;
  bool SetVolume(double volume, JniEnvExt* env = JniEnvExt::Get()) const;

  bool is_valid() const { return j_exoplayer_bridge_ != nullptr; }

 private:
  jobject j_exoplayer_bridge_ = nullptr;
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_EXOPLAYER_EXOPLAYER_BRIDGE_H_
