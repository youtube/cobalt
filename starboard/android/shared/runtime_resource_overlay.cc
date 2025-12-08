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
#include "cobalt/android/jni_headers/ResourceOverlay_jni.h"
// clang-format on

#include "starboard/android/shared/runtime_resource_overlay.h"

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "starboard/android/shared/starboard_bridge.h"
#include "starboard/common/log.h"
#include "starboard/common/once.h"
#include "starboard/common/string.h"

namespace starboard {

SB_ONCE_INITIALIZE_FUNCTION(RuntimeResourceOverlay,
                            RuntimeResourceOverlay::GetInstance)

RuntimeResourceOverlay::RuntimeResourceOverlay() {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> resource_overlay =
      StarboardBridge::GetInstance()->GetResourceOverlay(env);

  // Retrieve all Runtime Resource Overlay variables during initialization, so
  // synchronization isn't needed on access.
  min_audio_sink_buffer_size_in_frames_ =
      Java_ResourceOverlay_getMinAudioSinkBufferSizeInFrames(env,
                                                             resource_overlay);
  max_video_buffer_budget_ =
      Java_ResourceOverlay_getMaxVideoBufferBudget(env, resource_overlay);

  supports_spherical_videos_ =
      Java_ResourceOverlay_getSupportsSphericalVideos(env, resource_overlay);
  SB_LOG(INFO) << "Loaded RRO values: min_audio_sink_buffer_size_in_frames="
               << min_audio_sink_buffer_size_in_frames_
               << ", max_video_buffer_budget=" << max_video_buffer_budget_
               << ", supports_spherical_videos="
               << to_string(supports_spherical_videos_);
}

}  // namespace starboard
