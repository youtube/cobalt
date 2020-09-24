// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/android/shared/configuration.h"

#include "cobalt/extension/configuration.h"
#include "starboard/common/configuration_defaults.h"

namespace starboard {
namespace android {
namespace shared {

namespace {

const char* CobaltUserOnExitStrategy() {
  // On Android, we almost never want to actually terminate the process, so
  // instead indicate that we would instead like to be suspended when users
  // initiate an "exit".
  return "suspend";
}

int CobaltEglSwapInterval() {
  // Switch Android's SurfaceFlinger queue to "async mode" so that we don't
  // queue up rendered frames which would interfere with frame timing and
  // more importantly lead to input latency.
  return 0;
}

bool CobaltEnableQuic() {
  return 0;
}

const CobaltExtensionConfigurationApi kConfigurationApi = {
    kCobaltExtensionConfigurationName,
    2,
    &CobaltUserOnExitStrategy,
    &common::CobaltRenderDirtyRegionOnlyDefault,
    &CobaltEglSwapInterval,
    &common::CobaltFallbackSplashScreenUrlDefault,
    &CobaltEnableQuic,
    &common::CobaltSkiaCacheSizeInBytesDefault,
    &common::CobaltOffscreenTargetCacheSizeInBytesDefault,
    &common::CobaltEncodedImageCacheSizeInBytesDefault,
    &common::CobaltImageCacheSizeInBytesDefault,
    &common::CobaltLocalTypefaceCacheSizeInBytesDefault,
    &common::CobaltRemoteTypefaceCacheSizeInBytesDefault,
    &common::CobaltMeshCacheSizeInBytesDefault,
    &common::CobaltSoftwareSurfaceCacheSizeInBytesDefault,
    &common::CobaltImageCacheCapacityMultiplierWhenPlayingVideoDefault,
    &common::CobaltSkiaGlyphAtlasWidthDefault,
    &common::CobaltSkiaGlyphAtlasHeightDefault,
    &common::CobaltJsGarbageCollectionThresholdInBytesDefault,
    &common::CobaltReduceCpuMemoryByDefault,
    &common::CobaltReduceGpuMemoryByDefault,
    &common::CobaltGcZealDefault,
    &common::CobaltRasterizerTypeDefault,
    &common::CobaltEnableJitDefault,
    &common::CobaltFallbackSplashScreenTopicsDefault,
};

}  // namespace

const void* GetConfigurationApi() {
  return &kConfigurationApi;
}

}  // namespace shared
}  // namespace android
}  // namespace starboard
