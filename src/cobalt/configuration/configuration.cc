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

#include "cobalt/configuration/configuration.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "starboard/string.h"
#include "starboard/system.h"

namespace cobalt {
namespace configuration {

Configuration* Configuration::configuration_ = nullptr;

Configuration* Configuration::GetInstance() {
  return base::Singleton<Configuration>::get();
}

Configuration::Configuration() {
#if SB_API_VERSION < 11
  configuration_api_ = nullptr;
#else
  configuration_api_ = static_cast<const CobaltExtensionConfigurationApi*>(
      SbSystemGetExtension(kCobaltExtensionConfigurationName));
  if (configuration_api_) {
    // Verify it's the extension needed.
    if (SbStringCompareAll(configuration_api_->name,
                           kCobaltExtensionConfigurationName) != 0 ||
        configuration_api_->version < 1) {
      LOG(WARNING) << "Not using supplied cobalt configuration extension: "
                   << "'" << configuration_api_->name << "' ("
                   << configuration_api_->version << ")";
      configuration_api_ = nullptr;
    }
  }
#endif
}

const char* Configuration::CobaltUserOnExitStrategy() {
  if (configuration_api_) {
    return configuration_api_->CobaltUserOnExitStrategy();
  }
#if SB_API_VERSION >= SB_FEATURE_GYP_CONFIGURATION_VERSION
  return "stop";
#elif defined(COBALT_USER_ON_EXIT_STRATEGY)
  return COBALT_USER_ON_EXIT_STRATEGY;
#else
  return "stop";
#endif
}

bool Configuration::CobaltRenderDirtyRegionOnly() {
  if (configuration_api_) {
    return configuration_api_->CobaltRenderDirtyRegionOnly();
  }
#if SB_API_VERSION >= SB_FEATURE_GYP_CONFIGURATION_VERSION
  return false;
#elif defined(COBALT_RENDER_DIRTY_REGION_ONLY)
  return true;
#else
  return false;
#endif
}

int Configuration::CobaltEglSwapInterval() {
  if (configuration_api_) {
    return configuration_api_->CobaltEglSwapInterval();
  }
#if SB_API_VERSION >= SB_FEATURE_GYP_CONFIGURATION_VERSION
  return 1;
#elif defined(COBALT_EGL_SWAP_INTERVAL)
  return COBALT_EGL_SWAP_INTERVAL;
#else
  return 1;
#endif
}

const char* Configuration::CobaltFallbackSplashScreenUrl() {
  if (configuration_api_) {
    return configuration_api_->CobaltFallbackSplashScreenUrl();
  }
#if SB_API_VERSION >= SB_FEATURE_GYP_CONFIGURATION_VERSION
  return "none";
#elif defined(COBALT_FALLBACK_SPLASH_SCREEN_URL)
  return COBALT_FALLBACK_SPLASH_SCREEN_URL;
#else
  return "none";
#endif
}

bool Configuration::CobaltEnableQuic() {
  if (configuration_api_) {
    return configuration_api_->CobaltEnableQuic();
  }
#if SB_API_VERSION >= SB_FEATURE_GYP_CONFIGURATION_VERSION
  return true;
#elif defined(COBALT_ENABLE_QUIC)
  return true;
#else
  return false;
#endif
}

int Configuration::CobaltSkiaCacheSizeInBytes() {
  if (configuration_api_) {
    return configuration_api_->CobaltSkiaCacheSizeInBytes();
  }
#if SB_API_VERSION >= SB_FEATURE_GYP_CONFIGURATION_VERSION
  return 4 * 1024 * 1024;
#elif defined(COBALT_SKIA_CACHE_SIZE_IN_BYTES)
  return COBALT_SKIA_CACHE_SIZE_IN_BYTES;
#else
  return 4 * 1024 * 1024;
#endif
}

int Configuration::CobaltOffscreenTargetCacheSizeInBytes() {
  if (configuration_api_) {
    return configuration_api_->CobaltOffscreenTargetCacheSizeInBytes();
  }
#if SB_API_VERSION >= SB_FEATURE_GYP_CONFIGURATION_VERSION
  return -1;
#elif defined(COBALT_OFFSCREEN_TARGET_CACHE_SIZE_IN_BYTES)
  return COBALT_OFFSCREEN_TARGET_CACHE_SIZE_IN_BYTES;
#else
  return -1;
#endif
}

int Configuration::CobaltEncodedImageCacheSizeInBytes() {
  if (configuration_api_) {
    return configuration_api_->CobaltEncodedImageCacheSizeInBytes();
  }
#if SB_API_VERSION >= SB_FEATURE_GYP_CONFIGURATION_VERSION
  return 1024 * 1024;
#elif defined(COBALT_ENCODED_IMAGE_CACHE_SIZE_IN_BYTES)
  return COBALT_ENCODED_IMAGE_CACHE_SIZE_IN_BYTES;
#else
  return 1024 * 1024;
#endif
}

int Configuration::CobaltImageCacheSizeInBytes() {
  if (configuration_api_) {
    return configuration_api_->CobaltImageCacheSizeInBytes();
  }
#if SB_API_VERSION >= SB_FEATURE_GYP_CONFIGURATION_VERSION
  return -1;
#elif defined(COBALT_IMAGE_CACHE_SIZE_IN_BYTES)
  return COBALT_IMAGE_CACHE_SIZE_IN_BYTES;
#else
  return -1;
#endif
}

int Configuration::CobaltLocalTypefaceCacheSizeInBytes() {
  if (configuration_api_) {
    return configuration_api_->CobaltLocalTypefaceCacheSizeInBytes();
  }
#if SB_API_VERSION >= SB_FEATURE_GYP_CONFIGURATION_VERSION
  return 16 * 1024 * 1024;
#elif defined(COBALT_LOCAL_TYPEFACE_CACHE_SIZE_IN_BYTES)
  return COBALT_LOCAL_TYPEFACE_CACHE_SIZE_IN_BYTES;
#else
  return 16 * 1024 * 1024;
#endif
}

int Configuration::CobaltRemoteTypefaceCacheSizeInBytes() {
  if (configuration_api_) {
    return configuration_api_->CobaltRemoteTypefaceCacheSizeInBytes();
  }
#if SB_API_VERSION >= SB_FEATURE_GYP_CONFIGURATION_VERSION
  return 4 * 1024 * 1024;
#elif defined(COBALT_REMOTE_TYPEFACE_CACHE_SIZE_IN_BYTES)
  return COBALT_REMOTE_TYPEFACE_CACHE_SIZE_IN_BYTES;
#else
  return 4 * 1024 * 1024;
#endif
}

int Configuration::CobaltMeshCacheSizeInBytes() {
  if (configuration_api_) {
    return configuration_api_->CobaltMeshCacheSizeInBytes();
  }
#if SB_API_VERSION >= SB_FEATURE_GYP_CONFIGURATION_VERSION
  return 1024 * 1024;
#elif defined(COBALT_MESH_CACHE_SIZE_IN_BYTES)
  return COBALT_MESH_CACHE_SIZE_IN_BYTES;
#else
  return 1024 * 1024;
#endif
}

int Configuration::CobaltSoftwareSurfaceCacheSizeInBytes() {
  if (configuration_api_) {
    return configuration_api_->CobaltSoftwareSurfaceCacheSizeInBytes();
  }
#if SB_API_VERSION >= SB_FEATURE_GYP_CONFIGURATION_VERSION
  return 8 * 1024 * 1024;
#elif defined(COBALT_SOFTWARE_SURFACE_CACHE_SIZE_IN_BYTES)
  return COBALT_SOFTWARE_SURFACE_CACHE_SIZE_IN_BYTES;
#else
  return 8 * 1024 * 1024;
#endif
}

float Configuration::CobaltImageCacheCapacityMultiplierWhenPlayingVideo() {
  if (configuration_api_) {
    return configuration_api_
        ->CobaltImageCacheCapacityMultiplierWhenPlayingVideo();
  }
#if SB_API_VERSION >= SB_FEATURE_GYP_CONFIGURATION_VERSION
  return 1.0f;
#elif defined(COBALT_IMAGE_CACHE_CAPACITY_MULTIPLIER_WHEN_PLAYING_VIDEO)
  return COBALT_IMAGE_CACHE_CAPACITY_MULTIPLIER_WHEN_PLAYING_VIDEO;
#else
  return 1.0f;
#endif
}

int Configuration::CobaltJsGarbageCollectionThresholdInBytes() {
  if (configuration_api_) {
    return configuration_api_->CobaltJsGarbageCollectionThresholdInBytes();
  }
#if SB_API_VERSION >= SB_FEATURE_GYP_CONFIGURATION_VERSION
  return 8 * 1024 * 1024;
#elif defined(COBALT_JS_GARBAGE_COLLECTION_THRESHOLD_IN_BYTES)
  return COBALT_JS_GARBAGE_COLLECTION_THRESHOLD_IN_BYTES;
#else
  return 8 * 1024 * 1024;
#endif
}

int Configuration::CobaltSkiaGlyphAtlasWidth() {
  if (configuration_api_) {
    return configuration_api_->CobaltSkiaGlyphAtlasWidth();
  }
#if SB_API_VERSION >= SB_FEATURE_GYP_CONFIGURATION_VERSION
  return 2048;
#elif defined(COBALT_SKIA_GLYPH_ATLAS_WIDTH)
  return COBALT_SKIA_GLYPH_ATLAS_WIDTH;
#else
  return 2048;
#endif
}

int Configuration::CobaltSkiaGlyphAtlasHeight() {
  if (configuration_api_) {
    return configuration_api_->CobaltSkiaGlyphAtlasHeight();
  }
#if SB_API_VERSION >= SB_FEATURE_GYP_CONFIGURATION_VERSION
  return 2048;
#elif defined(COBALT_SKIA_GLYPH_ATLAS_HEIGHT)
  return COBALT_SKIA_GLYPH_ATLAS_HEIGHT;
#else
  return 2048;
#endif
}

int Configuration::CobaltReduceCpuMemoryBy() {
  if (configuration_api_) {
    return configuration_api_->CobaltReduceCpuMemoryBy();
  }
#if SB_API_VERSION >= SB_FEATURE_GYP_CONFIGURATION_VERSION
  return -1;
#elif defined(COBALT_REDUCE_CPU_MEMORY_BY)
  return COBALT_REDUCE_CPU_MEMORY_BY;
#else
  return -1;
#endif
}

int Configuration::CobaltReduceGpuMemoryBy() {
  if (configuration_api_) {
    return configuration_api_->CobaltReduceGpuMemoryBy();
  }
#if SB_API_VERSION >= SB_FEATURE_GYP_CONFIGURATION_VERSION
  return -1;
#elif defined(COBALT_REDUCE_GPU_MEMORY_BY)
  return COBALT_REDUCE_GPU_MEMORY_BY;
#else
  return -1;
#endif
}

bool Configuration::CobaltGcZeal() {
  if (configuration_api_) {
    return configuration_api_->CobaltGcZeal();
  }
#if SB_API_VERSION >= SB_FEATURE_GYP_CONFIGURATION_VERSION
  return false;
#elif defined(COBALT_GC_ZEAL)
  return true;
#else
  return false;
#endif
}

const char* Configuration::CobaltRasterizerType() {
  if (configuration_api_) {
    return configuration_api_->CobaltRasterizerType();
  }
#if SB_API_VERSION >= SB_FEATURE_GYP_CONFIGURATION_VERSION
  return "direct-gles";
#elif defined(COBALT_FORCE_STUB_RASTERIZER)
  return "stub";
#elif defined(COBALT_FORCE_DIRECT_GLES_RASTERIZER)
  return "direct-gles";
#else
  return "hardware";
#endif
}

bool Configuration::CobaltEnableJit() {
  if (configuration_api_) {
    return configuration_api_->CobaltEnableJit();
  }
#if SB_API_VERSION >= SB_FEATURE_GYP_CONFIGURATION_VERSION
  return false;
#elif defined(ENGINE_SUPPORTS_JIT)
  return true;
#elif defined(COBALT_DISABLE_JIT)
  return false;
#else
  return false;
#endif
}

}  // namespace configuration
}  // namespace cobalt
