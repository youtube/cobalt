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
    return configuration_api_->CobaltUserOnExitStrategy();
  }
  return "stop";
}

bool Configuration::CobaltRenderDirtyRegionOnly() {
  if (configuration_api_) {
    return configuration_api_->CobaltRenderDirtyRegionOnly();
  }
  return false;
}

int Configuration::CobaltEglSwapInterval() {
  if (configuration_api_) {
    return configuration_api_->CobaltEglSwapInterval();
  }
  return 1;
}

const char* Configuration::CobaltFallbackSplashScreenUrl() {
  if (configuration_api_) {
    return configuration_api_->CobaltFallbackSplashScreenUrl();
  }
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
    return configuration_api_->CobaltEnableQuic();
  }
  return true;
}

int Configuration::CobaltSkiaCacheSizeInBytes() {
  if (configuration_api_) {
    return configuration_api_->CobaltSkiaCacheSizeInBytes();
  }
  return 4 * 1024 * 1024;
}

int Configuration::CobaltOffscreenTargetCacheSizeInBytes() {
  if (configuration_api_) {
    return configuration_api_->CobaltOffscreenTargetCacheSizeInBytes();
  }
  return -1;
}

int Configuration::CobaltEncodedImageCacheSizeInBytes() {
  if (configuration_api_) {
    return configuration_api_->CobaltEncodedImageCacheSizeInBytes();
  }
  return 1024 * 1024;
}

int Configuration::CobaltImageCacheSizeInBytes() {
  if (configuration_api_) {
    return configuration_api_->CobaltImageCacheSizeInBytes();
  }
  return -1;
}

int Configuration::CobaltLocalTypefaceCacheSizeInBytes() {
  if (configuration_api_) {
    return configuration_api_->CobaltLocalTypefaceCacheSizeInBytes();
  }
  return 16 * 1024 * 1024;
}

int Configuration::CobaltRemoteTypefaceCacheSizeInBytes() {
  if (configuration_api_) {
    return configuration_api_->CobaltRemoteTypefaceCacheSizeInBytes();
  }
  return 4 * 1024 * 1024;
}

int Configuration::CobaltMeshCacheSizeInBytes() {
  if (configuration_api_) {
    return configuration_api_->CobaltMeshCacheSizeInBytes();
  }
  return 1024 * 1024;
}

// Deprecated, only retained as config API placeholder.
int Configuration::CobaltSoftwareSurfaceCacheSizeInBytes() { return -1; }

float Configuration::CobaltImageCacheCapacityMultiplierWhenPlayingVideo() {
  if (configuration_api_) {
    return configuration_api_
        ->CobaltImageCacheCapacityMultiplierWhenPlayingVideo();
  }
  return 1.0f;
}

int Configuration::CobaltSkiaGlyphAtlasWidth() {
  if (configuration_api_) {
    return configuration_api_->CobaltSkiaGlyphAtlasWidth();
  }
  return 2048;
}

int Configuration::CobaltSkiaGlyphAtlasHeight() {
  if (configuration_api_) {
    return configuration_api_->CobaltSkiaGlyphAtlasHeight();
  }
  return 2048;
}

int Configuration::CobaltReduceCpuMemoryBy() {
  if (configuration_api_) {
    return configuration_api_->CobaltReduceCpuMemoryBy();
  }
  return -1;
}

int Configuration::CobaltReduceGpuMemoryBy() {
  if (configuration_api_) {
    return configuration_api_->CobaltReduceGpuMemoryBy();
  }
  return -1;
}

bool Configuration::CobaltGcZeal() {
  if (configuration_api_) {
    return configuration_api_->CobaltGcZeal();
  }
  return false;
}

const char* Configuration::CobaltRasterizerType() {
  if (configuration_api_) {
    return configuration_api_->CobaltRasterizerType();
  }
  return "direct-gles";
}

bool Configuration::CobaltEnableJit() {
  if (configuration_api_) {
    return configuration_api_->CobaltEnableJit();
  }
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
