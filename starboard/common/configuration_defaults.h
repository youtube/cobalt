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

#ifndef STARBOARD_COMMON_CONFIGURATION_DEFAULTS_H_
#define STARBOARD_COMMON_CONFIGURATION_DEFAULTS_H_

namespace starboard {

const char* CobaltUserOnExitStrategyDefault();

int CobaltEglSwapIntervalDefault();

bool CobaltRenderDirtyRegionOnlyDefault();

const char* CobaltFallbackSplashScreenUrlDefault();

const char* CobaltFallbackSplashScreenTopicsDefault();

bool CobaltEnableQuicDefault();

int CobaltSkiaCacheSizeInBytesDefault();

int CobaltOffscreenTargetCacheSizeInBytesDefault();

int CobaltEncodedImageCacheSizeInBytesDefault();

int CobaltImageCacheSizeInBytesDefault();

int CobaltLocalTypefaceCacheSizeInBytesDefault();

int CobaltRemoteTypefaceCacheSizeInBytesDefault();

int CobaltMeshCacheSizeInBytesDefault();

int CobaltSoftwareSurfaceCacheSizeInBytesDefault();

float CobaltImageCacheCapacityMultiplierWhenPlayingVideoDefault();

int CobaltSkiaGlyphAtlasWidthDefault();

int CobaltSkiaGlyphAtlasHeightDefault();

// Deprecated.
int CobaltJsGarbageCollectionThresholdInBytesDefault();

int CobaltReduceCpuMemoryByDefault();

int CobaltReduceGpuMemoryByDefault();

bool CobaltGcZealDefault();

const char* CobaltRasterizerTypeDefault();

bool CobaltEnableJitDefault();

bool CobaltCanStoreCompiledJavascriptDefault();

// Aliases not to break CI tests.
// See https://paste.googleplex.com/4527409416241152
// TODO: b/441955897 - Update CI test to use flattened namespace
namespace common {
using starboard::CobaltCanStoreCompiledJavascriptDefault;
using starboard::CobaltEglSwapIntervalDefault;
using starboard::CobaltEnableJitDefault;
using starboard::CobaltEncodedImageCacheSizeInBytesDefault;
using starboard::CobaltFallbackSplashScreenTopicsDefault;
using starboard::CobaltFallbackSplashScreenUrlDefault;
using starboard::CobaltGcZealDefault;
using starboard::CobaltImageCacheCapacityMultiplierWhenPlayingVideoDefault;
using starboard::CobaltImageCacheSizeInBytesDefault;
using starboard::CobaltJsGarbageCollectionThresholdInBytesDefault;
using starboard::CobaltLocalTypefaceCacheSizeInBytesDefault;
using starboard::CobaltMeshCacheSizeInBytesDefault;
using starboard::CobaltOffscreenTargetCacheSizeInBytesDefault;
using starboard::CobaltRasterizerTypeDefault;
using starboard::CobaltReduceCpuMemoryByDefault;
using starboard::CobaltReduceGpuMemoryByDefault;
using starboard::CobaltRemoteTypefaceCacheSizeInBytesDefault;
using starboard::CobaltRenderDirtyRegionOnlyDefault;
using starboard::CobaltSkiaCacheSizeInBytesDefault;
using starboard::CobaltSkiaGlyphAtlasHeightDefault;
using starboard::CobaltSkiaGlyphAtlasWidthDefault;
using starboard::CobaltSoftwareSurfaceCacheSizeInBytesDefault;
}  // namespace common
}  // namespace starboard

#endif  // STARBOARD_COMMON_CONFIGURATION_DEFAULTS_H_
