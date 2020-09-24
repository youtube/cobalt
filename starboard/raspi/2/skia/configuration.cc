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

#include "starboard/raspi/2/skia/configuration.h"

#include "cobalt/extension/configuration.h"
#include "starboard/common/configuration_defaults.h"

namespace starboard {
namespace raspi {
namespace skia {

namespace {

// This atlas size works better than the auto-mem setting.
int CobaltSkiaGlyphAtlasWidth() {
  return 2048;
}
int CobaltSkiaGlyphAtlasHeight() {
  return 2048;
}

const char* CobaltRasterizerType() {
  // Use the skia hardware rasterizer.
  return "hardware";
}

const CobaltExtensionConfigurationApi kConfigurationApi = {
    kCobaltExtensionConfigurationName,
    2,
    &common::CobaltUserOnExitStrategyDefault,
    &common::CobaltRenderDirtyRegionOnlyDefault,
    &common::CobaltEglSwapIntervalDefault,
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
    &CobaltSkiaGlyphAtlasWidth,
    &CobaltSkiaGlyphAtlasHeight,
    &common::CobaltJsGarbageCollectionThresholdInBytesDefault,
    &common::CobaltReduceCpuMemoryByDefault,
    &common::CobaltReduceGpuMemoryByDefault,
    &common::CobaltGcZealDefault,
    &CobaltRasterizerType,
    &common::CobaltEnableJitDefault,
    &common::CobaltFallbackSplashScreenTopicsDefault,
};

}  // namespace

const void* GetConfigurationApi() {
  return &kConfigurationApi;
}

}  // namespace skia
}  // namespace raspi
}  // namespace starboard
