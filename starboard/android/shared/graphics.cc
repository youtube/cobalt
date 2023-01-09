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

#include "starboard/android/shared/application_android.h"
#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/extension/graphics.h"

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

bool DefaultShouldClearFrameOnShutdown(float* clear_color_red,
                                       float* clear_color_green,
                                       float* clear_color_blue,
                                       float* clear_color_alpha) {
  *clear_color_red = 0.0f;
  *clear_color_green = 0.0f;
  *clear_color_blue = 0.0f;
  *clear_color_alpha = 1.0f;
  return true;
}

bool DefaultGetMapToMeshColorAdjustments(
    CobaltExtensionGraphicsMapToMeshColorAdjustment* color_adjustment) {
  return false;
}

bool DefaultGetRenderRootTransform(float* m00,
                                   float* m01,
                                   float* m02,
                                   float* m10,
                                   float* m11,
                                   float* m12,
                                   float* m20,
                                   float* m21,
                                   float* m22) {
  return false;
}

void ReportFullyDrawn() {
  JniEnvExt::Get()->CallStarboardVoidMethodOrAbort("reportFullyDrawn", "()V");
}

const CobaltExtensionGraphicsApi kGraphicsApi = {
    kCobaltExtensionGraphicsName,
    6,
    &GetMaximumFrameIntervalInMilliseconds,
    &GetMinimumFrameIntervalInMilliseconds,
    &IsMapToMeshEnabled,
    &DefaultShouldClearFrameOnShutdown,
    &DefaultGetMapToMeshColorAdjustments,
    &DefaultGetRenderRootTransform,
    &ReportFullyDrawn,
};

}  // namespace

const void* GetGraphicsApi() {
  return &kGraphicsApi;
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
