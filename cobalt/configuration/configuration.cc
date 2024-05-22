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

#include <string.h>

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "starboard/common/configuration_defaults.h"
#include "starboard/string.h"
#include "starboard/system.h"

namespace cobalt {
namespace configuration {

Configuration* Configuration::configuration_ = nullptr;

Configuration* Configuration::GetInstance() {
  return base::Singleton<Configuration,
                         base::LeakySingletonTraits<Configuration>>::get();
}

Configuration::Configuration() {
  configuration_api_ = static_cast<const CobaltExtensionConfigurationApi*>(
      SbSystemGetExtension(kCobaltExtensionConfigurationName));
  if (configuration_api_) {
    // Verify it's the extension needed.
    if (strcmp(configuration_api_->name, kCobaltExtensionConfigurationName) !=
            0 ||
        configuration_api_->version < 1) {
      LOG(WARNING) << "Not using supplied cobalt configuration extension: "
                   << "'" << configuration_api_->name << "' ("
                   << configuration_api_->version << ")";
      configuration_api_ = nullptr;
    }
  }
}

const char* Configuration::CobaltUserOnExitStrategy() {
  if (configuration_api_) {
#if defined(COBALT_USER_ON_EXIT_STRATEGY)
    LOG(ERROR) << "COBALT_USER_ON_EXIT_STRATEGY and "
                  "CobaltExtensionConfigurationApi::CobaltUserOnExitStrategy() "
                  "are both defined. Remove 'cobalt_user_on_exit_strategy' "
                  "from your \"gyp_configuration.gypi\" file in favor of "
                  "using CobaltUserOnExitStrategy().";
#endif
    return configuration_api_->CobaltUserOnExitStrategy();
  }
#if defined(COBALT_USER_ON_EXIT_STRATEGY)
#error "COBALT_USER_ON_EXIT_STRATEGY is deprecated after Starboard version 12."
#error "Implement CobaltExtensionConfigurationApi::CobaltUserOnExitStrategy()"
#error "instead."
#endif
  return "stop";
}

bool Configuration::CobaltRenderDirtyRegionOnly() {
  if (configuration_api_) {
#if defined(COBALT_RENDER_DIRTY_REGION_ONLY)
    LOG(ERROR)
        << "COBALT_RENDER_DIRTY_REGION_ONLY and "
           "CobaltExtensionConfigurationApi::CobaltRenderDirtyRegionOnly() "
           "are both defined. Remove 'render_dirty_region_only' "
           "from your \"gyp_configuration.gypi\" file in favor of "
           "using CobaltRenderDirtyRegionOnly().";
#endif
    return configuration_api_->CobaltRenderDirtyRegionOnly();
  }
#if defined(COBALT_RENDER_DIRTY_REGION_ONLY)
#error \
    "COBALT_RENDER_DIRTY_REGION_ONLY is deprecated after Starboard version 12."
#error \
    "Implement CobaltExtensionConfigurationApi::CobaltRenderDirtyRegionOnly()"
#error "instead."
#endif
  return false;
}

int Configuration::CobaltEglSwapInterval() {
  if (configuration_api_) {
#if defined(COBALT_EGL_SWAP_INTERVAL)
    LOG(ERROR) << "COBALT_EGL_SWAP_INTERVAL and "
                  "CobaltExtensionConfigurationApi::CobaltEglSwapInterval() "
                  "are both defined. Remove 'cobalt_egl_swap_interval' "
                  "from your \"gyp_configuration.gypi\" file in favor of "
                  "using CobaltEglSwapInterval().";
#endif
    return configuration_api_->CobaltEglSwapInterval();
  }
#if defined(COBALT_EGL_SWAP_INTERVAL)
#error "COBALT_EGL_SWAP_INTERVAL is deprecated after Starboard version 12."
#error "Implement CobaltExtensionConfigurationApi::CobaltEglSwapInterval()"
#error "instead."
#endif
  return 1;
}

const char* Configuration::CobaltFallbackSplashScreenUrl() {
  if (configuration_api_) {
#if defined(COBALT_FALLBACK_SPLASH_SCREEN_URL)
    LOG(ERROR)
        << "COBALT_FALLBACK_SPLASH_SCREEN_URL and "
           "CobaltExtensionConfigurationApi::CobaltFallbackSplashScreenUrl() "
           "are both defined. Remove 'fallback_splash_screen_url' "
           "from your \"gyp_configuration.gypi\" file in favor of "
           "using CobaltFallbackSplashScreenUrl().";
#endif
    return configuration_api_->CobaltFallbackSplashScreenUrl();
  }
#if defined(COBALT_FALLBACK_SPLASH_SCREEN_URL)
// NOLINTNEXTLINE(whitespace/line_length)
#error \
    "COBALT_FALLBACK_SPLASH_SCREEN_URL is deprecated after Starboard version 12."
// NOLINTNEXTLINE(whitespace/line_length)
#error \
    "Implement CobaltExtensionConfigurationApi::CobaltFallbackSplashScreenUrl()"
#error "instead."
#endif
  return "none";
}

const char* Configuration::CobaltFallbackSplashScreenTopics() {
  if (configuration_api_ && configuration_api_->version >= 2) {
    return configuration_api_->CobaltFallbackSplashScreenTopics();
  }
  return "";
}

bool Configuration::CobaltEnableQuic() {
  if (configuration_api_) {
#if defined(COBALT_ENABLE_QUIC)
    LOG(ERROR) << "COBALT_ENABLE_QUIC and "
                  "CobaltExtensionConfigurationApi::CobaltEnableQuic() "
                  "are both defined. Remove 'cobalt_enable_quic' "
                  "from your \"gyp_configuration.gypi\" file in favor of "
                  "using CobaltEnableQuic().";
#endif
    return configuration_api_->CobaltEnableQuic();
  }
#if defined(COBALT_ENABLE_QUIC)
#error "COBALT_ENABLE_QUIC is deprecated after Starboard version 12."
#error "Implement CobaltExtensionConfigurationApi::CobaltEnableQuic()"
#error "instead."
#endif
  return true;
}

int Configuration::CobaltSkiaCacheSizeInBytes() {
  if (configuration_api_) {
#if defined(COBALT_SKIA_CACHE_SIZE_IN_BYTES)
    LOG(ERROR)
        << "COBALT_SKIA_CACHE_SIZE_IN_BYTES and "
           "CobaltExtensionConfigurationApi::CobaltSkiaCacheSizeInBytes() "
           "are both defined. Remove 'skia_cache_size_in_bytes' "
           "from your \"gyp_configuration.gypi\" file in favor of "
           "using CobaltSkiaCacheSizeInBytes().";
#endif
    return configuration_api_->CobaltSkiaCacheSizeInBytes();
  }
#if defined(COBALT_SKIA_CACHE_SIZE_IN_BYTES)
#error \
    "COBALT_SKIA_CACHE_SIZE_IN_BYTES is deprecated after Starboard version 12."
#error "Implement CobaltExtensionConfigurationApi::CobaltSkiaCacheSizeInBytes()"
#error "instead."
#endif
  return 4 * 1024 * 1024;
}

int Configuration::CobaltOffscreenTargetCacheSizeInBytes() {
  if (configuration_api_) {
#if defined(COBALT_OFFSCREEN_TARGET_CACHE_SIZE_IN_BYTES)
    LOG(ERROR)
        << "COBALT_OFFSCREEN_TARGET_CACHE_SIZE_IN_BYTES and "
           "CobaltExtensionConfigurationApi::"
           "CobaltOffscreenTargetCacheSizeInBytes() "
           "are both defined. Remove 'offscreen_target_cache_size_in_bytes' "
           "from your \"gyp_configuration.gypi\" file in favor of "
           "using CobaltOffscreenTargetCacheSizeInBytes().";
#endif
    return configuration_api_->CobaltOffscreenTargetCacheSizeInBytes();
  }
#if defined(COBALT_OFFSCREEN_TARGET_CACHE_SIZE_IN_BYTES)
// NOLINTNEXTLINE(whitespace/line_length)
#error \
    "COBALT_OFFSCREEN_TARGET_CACHE_SIZE_IN_BYTES is deprecated after Starboard version 12."
// NOLINTNEXTLINE(whitespace/line_length)
#error \
    "Implement CobaltExtensionConfigurationApi::CobaltOffscreenTargetCacheSizeInBytes()"
#error "instead."
#endif
  return -1;
}

int Configuration::CobaltEncodedImageCacheSizeInBytes() {
  if (configuration_api_) {
#if defined(COBALT_ENCODED_IMAGE_CACHE_SIZE_IN_BYTES)
    LOG(ERROR)
        << "COBALT_ENCODED_IMAGE_CACHE_SIZE_IN_BYTES and "
           "CobaltExtensionConfigurationApi::"
           "CobaltEncodedImageCacheSizeInBytes() "
           "are both defined. Remove 'encoded_image_cache_size_in_bytes' "
           "from your \"gyp_configuration.gypi\" file in favor of "
           "using CobaltEncodedImageCacheSizeInBytes().";
#endif
    return configuration_api_->CobaltEncodedImageCacheSizeInBytes();
  }
#if defined(COBALT_ENCODED_IMAGE_CACHE_SIZE_IN_BYTES)
// NOLINTNEXTLINE(whitespace/line_length)
#error \
    "COBALT_ENCODED_IMAGE_CACHE_SIZE_IN_BYTES is deprecated after Starboard version 12."
// NOLINTNEXTLINE(whitespace/line_length)
#error \
    "Implement CobaltExtensionConfigurationApi::CobaltEncodedImageCacheSizeInBytes()"
#error "instead."
#endif
  return 1024 * 1024;
}

int Configuration::CobaltImageCacheSizeInBytes() {
  if (configuration_api_) {
#if defined(COBALT_IMAGE_CACHE_SIZE_IN_BYTES)
    LOG(ERROR)
        << "COBALT_IMAGE_CACHE_SIZE_IN_BYTES and "
           "CobaltExtensionConfigurationApi::CobaltImageCacheSizeInBytes() "
           "are both defined. Remove 'image_cache_size_in_bytes' "
           "from your \"gyp_configuration.gypi\" file in favor of "
           "using CobaltImageCacheSizeInBytes().";
#endif
    return configuration_api_->CobaltImageCacheSizeInBytes();
  }
#if defined(COBALT_IMAGE_CACHE_SIZE_IN_BYTES)
#error \
    "COBALT_IMAGE_CACHE_SIZE_IN_BYTES is deprecated after Starboard version 12."
#error \
    "Implement CobaltExtensionConfigurationApi::CobaltImageCacheSizeInBytes()"
#error "instead."
#endif
  return -1;
}

int Configuration::CobaltLocalTypefaceCacheSizeInBytes() {
  if (configuration_api_) {
#if defined(COBALT_LOCAL_TYPEFACE_CACHE_SIZE_IN_BYTES)
    LOG(ERROR) << "COBALT_LOCAL_TYPEFACE_CACHE_SIZE_IN_BYTES and "
                  "CobaltExtensionConfigurationApi::"
                  "CobaltLocalTypefaceCacheSizeInBytes() "
                  "are both defined. Remove 'local_font_cache_size_in_bytes' "
                  "from your \"gyp_configuration.gypi\" file in favor of "
                  "using CobaltLocalTypefaceCacheSizeInBytes().";
#endif
    return configuration_api_->CobaltLocalTypefaceCacheSizeInBytes();
  }
#if defined(COBALT_LOCAL_TYPEFACE_CACHE_SIZE_IN_BYTES)
// NOLINTNEXTLINE(whitespace/line_length)
#error \
    "COBALT_LOCAL_TYPEFACE_CACHE_SIZE_IN_BYTES is deprecated after Starboard version 12."
// NOLINTNEXTLINE(whitespace/line_length)
#error \
    "Implement CobaltExtensionConfigurationApi::CobaltLocalTypefaceCacheSizeInBytes()"
#error "instead."
#endif
  return 16 * 1024 * 1024;
}

int Configuration::CobaltRemoteTypefaceCacheSizeInBytes() {
  if (configuration_api_) {
#if defined(COBALT_REMOTE_TYPEFACE_CACHE_SIZE_IN_BYTES)
    LOG(ERROR) << "COBALT_REMOTE_TYPEFACE_CACHE_SIZE_IN_BYTES and "
                  "CobaltExtensionConfigurationApi::"
                  "CobaltRemoteTypefaceCacheSizeInBytes() "
                  "are both defined. Remove 'remote_font_cache_size_in_bytes' "
                  "from your \"gyp_configuration.gypi\" file in favor of "
                  "using CobaltRemoteTypefaceCacheSizeInBytes().";
#endif
    return configuration_api_->CobaltRemoteTypefaceCacheSizeInBytes();
  }
#if defined(COBALT_REMOTE_TYPEFACE_CACHE_SIZE_IN_BYTES)
// NOLINTNEXTLINE(whitespace/line_length)
#error \
    "COBALT_REMOTE_TYPEFACE_CACHE_SIZE_IN_BYTES is deprecated after Starboard version 12."
// NOLINTNEXTLINE(whitespace/line_length)
#error \
    "Implement CobaltExtensionConfigurationApi::CobaltRemoteTypefaceCacheSizeInBytes()"
#error "instead."
#endif
  return 4 * 1024 * 1024;
}

int Configuration::CobaltMeshCacheSizeInBytes() {
  if (configuration_api_) {
#if defined(COBALT_MESH_CACHE_SIZE_IN_BYTES)
    LOG(ERROR)
        << "COBALT_MESH_CACHE_SIZE_IN_BYTES and "
           "CobaltExtensionConfigurationApi::CobaltMeshCacheSizeInBytes() "
           "are both defined. Remove 'mesh_cache_size_in_bytes' "
           "from your \"gyp_configuration.gypi\" file in favor of "
           "using CobaltMeshCacheSizeInBytes().";
#endif
    return configuration_api_->CobaltMeshCacheSizeInBytes();
  }
#if defined(COBALT_MESH_CACHE_SIZE_IN_BYTES)
#error \
    "COBALT_MESH_CACHE_SIZE_IN_BYTES is deprecated after Starboard version 12."
#error "Implement CobaltExtensionConfigurationApi::CobaltMeshCacheSizeInBytes()"
#error "instead."
#endif
  return 1024 * 1024;
}

// Deprecated, only retained as config API placeholder.
int Configuration::CobaltSoftwareSurfaceCacheSizeInBytes() { return -1; }

float Configuration::CobaltImageCacheCapacityMultiplierWhenPlayingVideo() {
  if (configuration_api_) {
#if defined(COBALT_IMAGE_CACHE_CAPACITY_MULTIPLIER_WHEN_PLAYING_VIDEO)
    LOG(ERROR)
        << "COBALT_IMAGE_CACHE_CAPACITY_MULTIPLIER_WHEN_PLAYING_VIDEO and "
           "CobaltExtensionConfigurationApi::"
           "CobaltImageCacheCapacityMultiplierWhenPlayingVideo() "
           "are both defined. Remove "
           "'image_cache_capacity_multiplier_when_playing_video' "
           "from your \"gyp_configuration.gypi\" file in favor of "
           "using CobaltImageCacheCapacityMultiplierWhenPlayingVideo().";
#endif
    return configuration_api_
        ->CobaltImageCacheCapacityMultiplierWhenPlayingVideo();
  }
#if defined(COBALT_IMAGE_CACHE_CAPACITY_MULTIPLIER_WHEN_PLAYING_VIDEO)
// NOLINTNEXTLINE(whitespace/line_length)
#error \
    "COBALT_IMAGE_CACHE_CAPACITY_MULTIPLIER_WHEN_PLAYING_VIDEO is deprecated after Starboard version 12."
// NOLINTNEXTLINE(whitespace/line_length)
#error \
    "Implement CobaltExtensionConfigurationApi::CobaltImageCacheCapacityMultiplierWhenPlayingVideo()"
#error "instead."
#endif
  return 1.0f;
}

int Configuration::CobaltSkiaGlyphAtlasWidth() {
  if (configuration_api_) {
#if defined(COBALT_SKIA_GLYPH_ATLAS_WIDTH)
    LOG(ERROR)
        << "COBALT_SKIA_GLYPH_ATLAS_WIDTH and "
           "CobaltExtensionConfigurationApi::CobaltSkiaGlyphAtlasWidth() "
           "are both defined. Remove 'skia_glyph_atlas_width' "
           "from your \"gyp_configuration.gypi\" file in favor of "
           "using CobaltSkiaGlyphAtlasWidth().";
#endif
    return configuration_api_->CobaltSkiaGlyphAtlasWidth();
  }
#if defined(COBALT_SKIA_GLYPH_ATLAS_WIDTH)
#error "COBALT_SKIA_GLYPH_ATLAS_WIDTH is deprecated after Starboard version 12."
#error "Implement CobaltExtensionConfigurationApi::CobaltSkiaGlyphAtlasWidth()"
#error "instead."
#endif
  return 2048;
}

int Configuration::CobaltSkiaGlyphAtlasHeight() {
  if (configuration_api_) {
#if defined(COBALT_SKIA_GLYPH_ATLAS_HEIGHT)
    LOG(ERROR)
        << "COBALT_SKIA_GLYPH_ATLAS_HEIGHT and "
           "CobaltExtensionConfigurationApi::CobaltSkiaGlyphAtlasHeight() "
           "are both defined. Remove 'skia_glyph_atlas_height' "
           "from your \"gyp_configuration.gypi\" file in favor of "
           "using CobaltSkiaGlyphAtlasHeight().";
#endif
    return configuration_api_->CobaltSkiaGlyphAtlasHeight();
  }
#if defined(COBALT_SKIA_GLYPH_ATLAS_HEIGHT)
#error \
    "COBALT_SKIA_GLYPH_ATLAS_HEIGHT is deprecated after Starboard version 12."
#error "Implement CobaltExtensionConfigurationApi::CobaltSkiaGlyphAtlasHeight()"
#error "instead."
#endif
  return 2048;
}

int Configuration::CobaltReduceCpuMemoryBy() {
  if (configuration_api_) {
#if defined(COBALT_REDUCE_CPU_MEMORY_BY)
    LOG(ERROR) << "COBALT_REDUCE_CPU_MEMORY_BY and "
                  "CobaltExtensionConfigurationApi::CobaltReduceCpuMemoryBy() "
                  "are both defined. Remove 'reduce_cpu_memory_by' "
                  "from your \"gyp_configuration.gypi\" file in favor of "
                  "using CobaltReduceCpuMemoryBy().";
#endif
    return configuration_api_->CobaltReduceCpuMemoryBy();
  }
#if defined(COBALT_REDUCE_CPU_MEMORY_BY)
#error "COBALT_REDUCE_CPU_MEMORY_BY is deprecated after Starboard version 12."
#error "Implement CobaltExtensionConfigurationApi::CobaltReduceCpuMemoryBy()"
#error "instead."
#endif
  return -1;
}

int Configuration::CobaltReduceGpuMemoryBy() {
  if (configuration_api_) {
#if defined(COBALT_REDUCE_GPU_MEMORY_BY)
    LOG(ERROR) << "COBALT_REDUCE_GPU_MEMORY_BY and "
                  "CobaltExtensionConfigurationApi::CobaltReduceGpuMemoryBy() "
                  "are both defined. Remove 'reduce_gpu_memory_by' "
                  "from your \"gyp_configuration.gypi\" file in favor of "
                  "using CobaltReduceGpuMemoryBy().";
#endif
    return configuration_api_->CobaltReduceGpuMemoryBy();
  }
#if defined(COBALT_REDUCE_GPU_MEMORY_BY)
#error "COBALT_REDUCE_GPU_MEMORY_BY is deprecated after Starboard version 12."
#error "Implement CobaltExtensionConfigurationApi::CobaltReduceGpuMemoryBy()"
#error "instead."
#endif
  return -1;
}

bool Configuration::CobaltGcZeal() {
  if (configuration_api_) {
#if defined(COBALT_GC_ZEAL)
    LOG(ERROR) << "COBALT_GC_ZEAL and "
                  "CobaltExtensionConfigurationApi::CobaltGcZeal() "
                  "are both defined. Remove 'cobalt_gc_zeal' "
                  "from your \"gyp_configuration.gypi\" file in favor of "
                  "using CobaltGcZeal().";
#endif
    return configuration_api_->CobaltGcZeal();
  }
#if defined(COBALT_GC_ZEAL)
#error "COBALT_GC_ZEAL is deprecated after Starboard version 12."
#error "Implement CobaltExtensionConfigurationApi::CobaltGcZeal()"
#error "instead."
#endif
  return false;
}

const char* Configuration::CobaltRasterizerType() {
  if (configuration_api_) {
#if defined(COBALT_FORCE_STUB_RASTERIZER) || \
    defined(COBALT_FORCE_DIRECT_GLES_RASTERIZER)
    LOG(ERROR) << "COBALT_FORCE_STUB_RASTERIZER or "
                  "COBALT_FORCE_DIRECT_GLES_RASTERIZER and "
                  "CobaltExtensionConfigurationApi::CobaltRasterizerType() "
                  "are both defined. Remove 'rasterizer_type' "
                  "from your \"gyp_configuration.gypi\" file in favor of "
                  "using CobaltRasterizerType().";
#endif
    return configuration_api_->CobaltRasterizerType();
  }
#if defined(COBALT_FORCE_STUB_RASTERIZER) || \
    defined(COBALT_FORCE_DIRECT_GLES_RASTERIZER)
#error "COBALT_FORCE_STUB_RASTERIZER and COBALT_FORCE_DIRECT_GLES_RASTERIZER"
#error "are deprecated after Starboard version 12. Implement"
#error "CobaltExtensionConfigurationApi::CobaltRasterizerType() instead."
#endif
  return "direct-gles";
}

bool Configuration::CobaltEnableJit() {
  if (configuration_api_) {
#if defined(ENGINE_SUPPORTS_JIT) || defined(COBALT_DISABLE_JIT)
    LOG(ERROR) << "ENGINE_SUPPORTS_JIT or COBALT_DISABLE_JIT and "
                  "CobaltExtensionConfigurationApi::CobaltEnableJit() "
                  "are both defined. Remove 'cobalt_enable_jit' "
                  "from your \"gyp_configuration.gypi\" file in favor of "
                  "using CobaltEnableJit().";
#endif
    return configuration_api_->CobaltEnableJit();
  }
#if defined(ENGINE_SUPPORTS_JIT) || defined(COBALT_DISABLE_JIT)
#error "ENGINE_SUPPORTS_JIT and COBALT_DISABLE_JIT are deprecated after"
#error "Starboard version 12."
#error "Implement CobaltExtensionConfigurationApi::CobaltEnableJit()"
#error "instead."
#endif
  return false;
}

bool Configuration::CobaltCanStoreCompiledJavascript() {
  if (configuration_api_ && configuration_api_->version >= 3) {
    return configuration_api_->CobaltCanStoreCompiledJavascript();
  }
  return starboard::common::CobaltCanStoreCompiledJavascriptDefault();
}

}  // namespace configuration
}  // namespace cobalt
