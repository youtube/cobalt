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

#include "starboard/tvos/shared/configuration.h"

#include "starboard/common/configuration_defaults.h"
#include "starboard/extension/configuration.h"

namespace starboard {
namespace shared {
namespace uikit {

namespace {

const char* CobaltUserOnExitStrategy() {
  // This URL points Kabuki to the splash screen's location when it's not
  // found in the cache. This fixes the first-launch / OOB experience which
  // would otherwise show a blank screen.
  return "suspend";
}

const char* CobaltFallbackSplashScreenUrl() {
  return "file:///internal/cobalt/browser/splash_screen/"
         "youtube_splash_screen.html";
}

int CobaltEncodedImageCacheSizeInBytes() {
  return 8 * 1024 * 1024;
}

int CobaltImageCacheSizeInBytes() {
  return 64 * 1024 * 1024;
}

bool CobaltEnableJit() {
  return false;
}

const CobaltExtensionConfigurationApi kConfigurationApi = {
    kCobaltExtensionConfigurationName,
    3,
    &CobaltUserOnExitStrategy,
    &common::CobaltRenderDirtyRegionOnlyDefault,
    &common::CobaltEglSwapIntervalDefault,
    &CobaltFallbackSplashScreenUrl,
    &common::CobaltEnableQuicDefault,
    &common::CobaltSkiaCacheSizeInBytesDefault,
    &common::CobaltOffscreenTargetCacheSizeInBytesDefault,
    &CobaltEncodedImageCacheSizeInBytes,
    &CobaltImageCacheSizeInBytes,
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
    &CobaltEnableJit,
    &common::CobaltFallbackSplashScreenTopicsDefault,
    &common::CobaltCanStoreCompiledJavascriptDefault,
};

}  // namespace

const void* GetConfigurationApi() {
  return &kConfigurationApi;
}

}  // namespace uikit
}  // namespace shared
}  // namespace starboard
