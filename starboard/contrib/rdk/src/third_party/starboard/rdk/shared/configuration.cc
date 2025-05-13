// Copyright 2020 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
//
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

#include "third_party/starboard/rdk/shared/configuration.h"

#include "starboard/common/configuration_defaults.h"
#include "starboard/extension/configuration.h"

#include "third_party/starboard/rdk/shared/libcobalt.h"

namespace third_party {
namespace starboard {
namespace rdk {
namespace shared {

namespace {

bool CobaltEnableQuic() {
  return false;
}

const char* CobaltUserOnExitStrategy() {
  return SbRdkGetCobaltExitStrategy();
}

const CobaltExtensionConfigurationApi kConfigurationApi = {
    kCobaltExtensionConfigurationName,
    2,
    &CobaltUserOnExitStrategy,
    &::starboard::common::CobaltRenderDirtyRegionOnlyDefault,
    &::starboard::common::CobaltEglSwapIntervalDefault,
    &::starboard::common::CobaltFallbackSplashScreenUrlDefault,
    &CobaltEnableQuic,
    &::starboard::common::CobaltSkiaCacheSizeInBytesDefault,
    &::starboard::common::CobaltOffscreenTargetCacheSizeInBytesDefault,
    &::starboard::common::CobaltEncodedImageCacheSizeInBytesDefault,
    &::starboard::common::CobaltImageCacheSizeInBytesDefault,
    &::starboard::common::CobaltLocalTypefaceCacheSizeInBytesDefault,
    &::starboard::common::CobaltRemoteTypefaceCacheSizeInBytesDefault,
    &::starboard::common::CobaltMeshCacheSizeInBytesDefault,
    &::starboard::common::CobaltSoftwareSurfaceCacheSizeInBytesDefault,
    &::starboard::common::CobaltImageCacheCapacityMultiplierWhenPlayingVideoDefault,
    &::starboard::common::CobaltSkiaGlyphAtlasWidthDefault,
    &::starboard::common::CobaltSkiaGlyphAtlasHeightDefault,
    &::starboard::common::CobaltJsGarbageCollectionThresholdInBytesDefault,
    &::starboard::common::CobaltReduceCpuMemoryByDefault,
    &::starboard::common::CobaltReduceGpuMemoryByDefault,
    &::starboard::common::CobaltGcZealDefault,
    &::starboard::common::CobaltRasterizerTypeDefault,
    &::starboard::common::CobaltEnableJitDefault,
    &::starboard::common::CobaltFallbackSplashScreenTopicsDefault,
    &::starboard::common::CobaltCanStoreCompiledJavascriptDefault,
};

}  // namespace

const void* GetConfigurationApi() {
  return &kConfigurationApi;
}

}  // namespace shared
}  // namespace rdk
}  // namespace starboard
}  // namespace third_party
