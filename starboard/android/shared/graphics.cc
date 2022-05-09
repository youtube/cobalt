// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/graphics.h"

#include "starboard/common/log.h"

#include "cobalt/extension/graphics.h"
#include "starboard/android/shared/application_android.h"

namespace starboard {
namespace android {
namespace shared {

namespace {

float GetMaximumFrameIntervalInMilliseconds() {
  return -1.0f;
}

float GetMinimumFrameIntervalInMilliseconds() {
  return 16.0f;
}

bool IsMapToMeshEnabled() {
  bool supports_spherical_videos =
      starboard::android::shared::ApplicationAndroid::Get()
          ->GetOverlayedBoolValue("supports_spherical_videos");
  return supports_spherical_videos;
}

const CobaltExtensionGraphicsApi kGraphicsApi = {
    kCobaltExtensionGraphicsName,
    3,
    &GetMaximumFrameIntervalInMilliseconds,
    &GetMinimumFrameIntervalInMilliseconds,
    &IsMapToMeshEnabled,
};

}  // namespace

const void* GetGraphicsApi() {
  return &kGraphicsApi;
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
