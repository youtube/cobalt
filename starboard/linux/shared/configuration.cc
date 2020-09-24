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

#include "starboard/linux/shared/configuration.h"

#include "cobalt/extension/configuration.h"
#include "starboard/common/configuration_defaults.h"

// Omit namespace linux due to symbol name conflict.
namespace starboard {
namespace shared {

namespace {

int CobaltEglSwapInterval() {
  // This platform uses a compositor to present the rendering output, so
  // set the swap interval to update the buffer immediately. That buffer
  // will then be presented by the compositor on its own time.
  return 0;
}

const CobaltExtensionConfigurationApi kConfigurationApi = {
    kCobaltExtensionConfigurationName,
    2,
    &common::CobaltUserOnExitStrategyDefault,
    &common::CobaltRenderDirtyRegionOnlyDefault,
    &CobaltEglSwapInterval,
    &common::CobaltFallbackSplashScreenUrlDefault,
    &common::CobaltEnableQuicDefault,
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
}  // namespace starboard
