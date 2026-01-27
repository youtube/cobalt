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

// clang-format off
#include "starboard/android/shared/runtime_resource_overlay.h"
// clang-format on

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/common/log.h"
#include "starboard/common/once.h"

namespace starboard::android::shared {

SB_ONCE_INITIALIZE_FUNCTION(RuntimeResourceOverlay,
                            RuntimeResourceOverlay::GetInstance);

RuntimeResourceOverlay::RuntimeResourceOverlay() {
  JniEnvExt* env = JniEnvExt::Get();
  jobject resource_overlay = env->CallStarboardObjectMethodOrAbort(
      "getResourceOverlay", "()Ldev/cobalt/coat/ResourceOverlay;");

  // Retrieve all Runtime Resource Overlay variables during initialization, so
  // synchronization isn't needed on access.
  min_audio_sink_buffer_size_in_frames_ = env->GetIntFieldOrAbort(
      resource_overlay, "min_audio_sink_buffer_size_in_frames", "I");
  max_video_buffer_budget_ =
      env->GetIntFieldOrAbort(resource_overlay, "max_video_buffer_budget", "I");

  SB_LOG(INFO) << "Loaded RRO values\n\tmin_audio_sink_buffer_size_in_frames: "
               << min_audio_sink_buffer_size_in_frames_
               << "\n\tmax_video_buffer_budget: " << max_video_buffer_budget_;
}

}  // namespace starboard::android::shared
