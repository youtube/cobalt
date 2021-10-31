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

#include "starboard/common/configuration_defaults.h"

namespace starboard {
namespace common {

const char* CobaltUserOnExitStrategyDefault() {
  return "stop";
}

bool CobaltRenderDirtyRegionOnlyDefault() {
  return false;
}

int CobaltEglSwapIntervalDefault() {
  return 1;
}

const char* CobaltFallbackSplashScreenUrlDefault() {
  return "h5vcc-embedded://black_splash_screen.html";
}

const char* CobaltFallbackSplashScreenTopicsDefault() {
  return "";
}

bool CobaltEnableQuicDefault() {
  return true;
}

int CobaltSkiaCacheSizeInBytesDefault() {
  return 4 * 1024 * 1024;
}

int CobaltOffscreenTargetCacheSizeInBytesDefault() {
  return -1;
}

int CobaltEncodedImageCacheSizeInBytesDefault() {
  return 1024 * 1024;
}

int CobaltImageCacheSizeInBytesDefault() {
  return -1;
}

int CobaltLocalTypefaceCacheSizeInBytesDefault() {
  return 16 * 1024 * 1024;
}

int CobaltRemoteTypefaceCacheSizeInBytesDefault() {
  return 4 * 1024 * 1024;
}

int CobaltMeshCacheSizeInBytesDefault() {
  return 1024 * 1024;
}

int CobaltSoftwareSurfaceCacheSizeInBytesDefault() {
  return 8 * 1024 * 1024;
}

float CobaltImageCacheCapacityMultiplierWhenPlayingVideoDefault() {
  return 1.0f;
}

int CobaltSkiaGlyphAtlasWidthDefault() {
  return -1;
}

int CobaltSkiaGlyphAtlasHeightDefault() {
  return -1;
}

int CobaltJsGarbageCollectionThresholdInBytesDefault() {
  return 8 * 1024 * 1024;
}

int CobaltReduceCpuMemoryByDefault() {
  return -1;
}

int CobaltReduceGpuMemoryByDefault() {
  return -1;
}

bool CobaltGcZealDefault() {
  return false;
}

const char* CobaltRasterizerTypeDefault() {
  return "direct-gles";
}

bool CobaltEnableJitDefault() {
  return true;
}

}  // namespace common
}  // namespace starboard
