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

#include "starboard/android/shared/exoplayer/exoplayer_bridge.h"

#include <jni.h>

#include "starboard/common/log.h"

namespace starboard {
namespace android {
namespace shared {

using starboard::android::shared::ScopedLocalJavaRef;

ExoPlayerBridge::ExoPlayerBridge() {
  JniEnvExt* env = JniEnvExt::Get();
  SB_LOG(INFO) << "About to create ExoPlayer";
  jobject j_exoplayer_bridge = env->CallStarboardObjectMethodOrAbort(
      "getExoPlayerBridge", "()Ldev/cobalt/media/ExoPlayerBridge;");
  if (!j_exoplayer_bridge) {
    SB_LOG(WARNING) << "Failed to create |j_exoplayer_bridge|.";
    return;
  }
  j_exoplayer_bridge_ = env->ConvertLocalRefToGlobalRef(j_exoplayer_bridge);
  env->CallVoidMethodOrAbort(j_exoplayer_bridge_, "createExoPlayer", "()V");
}

ExoPlayerBridge::~ExoPlayerBridge() {
  JniEnvExt* env = JniEnvExt::Get();
  SB_LOG(INFO) << "About to destroy ExoPlayer";
  env->CallVoidMethodOrAbort(j_exoplayer_bridge_, "destroyExoPlayer", "()V");
}

void ExoPlayerBridge::WriteSamples(
    SbMediaType sample_type,
    const SbPlayerSampleInfo* sample_infos,
    int number_of_sample_infos,
    JniEnvExt* env /*= JniEnvExt::Get()*/) const {}

bool ExoPlayerBridge::Play(JniEnvExt* env /*= JniEnvExt::Get()*/) const {
  return env->CallBooleanMethodOrAbort(j_exoplayer_bridge_, "play", "()Z");
}

bool ExoPlayerBridge::Pause(JniEnvExt* env /*= JniEnvExt::Get()*/) const {
  return env->CallBooleanMethodOrAbort(j_exoplayer_bridge_, "pause", "()Z");
}

bool ExoPlayerBridge::Stop(JniEnvExt* env /*= JniEnvExt::Get()*/) const {
  return env->CallBooleanMethodOrAbort(j_exoplayer_bridge_, "stop", "()Z");
}

bool ExoPlayerBridge::SetVolume(double volume,
                                JniEnvExt* env /*= JniEnvExt::Get()*/) const {
  return env->CallBooleanMethodOrAbort(j_exoplayer_bridge_, "setVolume", "(F)Z",
                                       static_cast<float>(volume));
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
