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

#include "starboard/xb1/shared/configuration.h"

#include "starboard/common/configuration_defaults.h"
#include "starboard/extension/configuration.h"

namespace starboard {
namespace xb1 {
namespace shared {

namespace {

const char* CobaltFallbackSplashScreenUrl() {
  return "file:///splash_screen/youtube_splash_screen.html";
}

int CobaltLocalTypefaceCacheSizeInBytes() {
  // 24MB font cache size is recommended when using the |expanded|
  // cobalt font package.
  return 24 * 1024 * 1024;
}

const CobaltExtensionConfigurationApi kConfigurationApi = {
    kCobaltExtensionConfigurationName,
    3,
    &common::CobaltUserOnExitStrategyDefault,
    &common::CobaltRenderDirtyRegionOnlyDefault,
    &common::CobaltEglSwapIntervalDefault,
    &CobaltFallbackSplashScreenUrl,
    &common::CobaltEnableQuicDefault,
    &common::CobaltSkiaCacheSizeInBytesDefault,
    &common::CobaltOffscreenTargetCacheSizeInBytesDefault,
    &common::CobaltEncodedImageCacheSizeInBytesDefault,
    &common::CobaltImageCacheSizeInBytesDefault,
    &CobaltLocalTypefaceCacheSizeInBytes,
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
    &common::CobaltCanStoreCompiledJavascriptDefault,
};

}  // namespace

const void* GetConfigurationApi() {
  return &kConfigurationApi;
}

}  // namespace shared
}  // namespace xb1
}  // namespace starboard
