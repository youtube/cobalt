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

#ifndef STARBOARD_ANDROID_SHARED_RUNTIME_RESOURCE_OVERLAY_H_
#define STARBOARD_ANDROID_SHARED_RUNTIME_RESOURCE_OVERLAY_H_

namespace starboard {
namespace android {
namespace shared {

// Caches the Runtime Resource Overlay variables.
// All RRO variables which can be retrieved here must be defined
// in cobalt/android/apk/app/src/main/res/values/rro_variables.xml and be loaded
// in cobalt/android/apk/app/src/main/java/dev/cobalt/coat/ResourceOverlay.java.
class RuntimeResourceOverlay {
 public:
  static RuntimeResourceOverlay* GetInstance();

  int min_audio_sink_buffer_size_in_frames() const {
    return min_audio_sink_buffer_size_in_frames_;
  }

  int max_video_buffer_budget() const { return max_video_buffer_budget_; }

 private:
  RuntimeResourceOverlay();

  int min_audio_sink_buffer_size_in_frames_;
  int max_video_buffer_budget_;
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_RUNTIME_RESOURCE_OVERLAY_H_
