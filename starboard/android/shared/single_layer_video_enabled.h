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

#ifndef STARBOARD_ANDROID_SHARED_SINGLE_LAYER_VIDEO_ENABLED_H_
#define STARBOARD_ANDROID_SHARED_SINGLE_LAYER_VIDEO_ENABLED_H_

namespace starboard {

// Get single_layer_video_enabled setting set via
// SetSingleLayerVideoEnabledForCurrentThread(). Returns false if not set.
bool GetSingleLayerVideoEnabledForCurrentThread();

// This is used to indicate whether single-layer video is enabled (i.e. the UI
// plane is removed during fullscreen video) for any subsequently created
// SbPlayer on the current calling thread.
void SetSingleLayerVideoEnabledForCurrentThread(
    bool single_layer_video_enabled);

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_SINGLE_LAYER_VIDEO_ENABLED_H_
