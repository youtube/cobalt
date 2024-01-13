// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include <cmath>

#include "starboard/extension/configuration.h"
#include "starboard/extension/crash_handler.h"
#include "starboard/extension/cwrappers.h"
#include "starboard/extension/enhanced_audio.h"
#include "starboard/extension/font.h"
#include "starboard/extension/free_space.h"
#include "starboard/extension/graphics.h"
#include "starboard/extension/ifa.h"
#include "starboard/extension/installation_manager.h"
#include "starboard/extension/javascript_cache.h"
#include "starboard/extension/media_session.h"
#include "starboard/extension/memory_mapped_file.h"
#include "starboard/extension/platform_info.h"
#include "starboard/extension/platform_service.h"
#include "starboard/extension/time_zone.h"
#include "starboard/extension/updater_notification.h"
#include "starboard/extension/url_fetcher_observer.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace extension {

TEST(ExtensionTest, PlatformService) {
  typedef CobaltExtensionPlatformServiceApi ExtensionApi;
  const char* kExtensionName = kCobaltExtensionPlatformServiceName;

  const ExtensionApi* extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  if (!extension_api) {
    return;
  }

  EXPECT_STREQ(extension_api->name, kExtensionName);
  EXPECT_GE(extension_api->version, 1u);
  EXPECT_LE(extension_api->version, 3u);
  EXPECT_NE(extension_api->Has, nullptr);
  EXPECT_NE(extension_api->Open, nullptr);
  EXPECT_NE(extension_api->Close, nullptr);
  EXPECT_NE(extension_api->Send, nullptr);

  const ExtensionApi* second_extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  EXPECT_EQ(second_extension_api, extension_api)
      << "Extension struct should be a singleton";
}

TEST(ExtensionTest, Graphics) {
  typedef CobaltExtensionGraphicsApi ExtensionApi;
  const char* kExtensionName = kCobaltExtensionGraphicsName;

  const ExtensionApi* extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  if (!extension_api) {
    return;
  }

  EXPECT_STREQ(extension_api->name, kExtensionName);
  EXPECT_GE(extension_api->version, 1u);
  EXPECT_LE(extension_api->version, 6u);

  EXPECT_NE(extension_api->GetMaximumFrameIntervalInMilliseconds, nullptr);
  float maximum_frame_interval =
      extension_api->GetMaximumFrameIntervalInMilliseconds();
  EXPECT_FALSE(std::isnan(maximum_frame_interval));

  if (extension_api->version >= 2) {
    EXPECT_NE(extension_api->GetMinimumFrameIntervalInMilliseconds, nullptr);
    float minimum_frame_interval =
        extension_api->GetMinimumFrameIntervalInMilliseconds();
    EXPECT_GT(minimum_frame_interval, 0);
  }

  if (extension_api->version >= 3) {
    EXPECT_NE(extension_api->IsMapToMeshEnabled, nullptr);
  }

  if (extension_api->version >= 4) {
    EXPECT_NE(extension_api->ShouldClearFrameOnShutdown, nullptr);
    float clear_color_r, clear_color_g, clear_color_b, clear_color_a;
    if (extension_api->ShouldClearFrameOnShutdown(
            &clear_color_r, &clear_color_g, &clear_color_b, &clear_color_a)) {
      EXPECT_GE(clear_color_r, 0.0f);
      EXPECT_LE(clear_color_r, 1.0f);
      EXPECT_GE(clear_color_g, 0.0f);
      EXPECT_LE(clear_color_g, 1.0f);
      EXPECT_GE(clear_color_b, 0.0f);
      EXPECT_LE(clear_color_b, 1.0f);
      EXPECT_GE(clear_color_a, 0.0f);
      EXPECT_LE(clear_color_a, 1.0f);
    }
  }

  if (extension_api->version >= 5) {
    EXPECT_NE(extension_api->GetMapToMeshColorAdjustments, nullptr);
    EXPECT_NE(extension_api->GetRenderRootTransform, nullptr);
  }

  if (extension_api->version >= 6) {
    EXPECT_NE(extension_api->ReportFullyDrawn, nullptr);
  }

  const ExtensionApi* second_extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  EXPECT_EQ(second_extension_api, extension_api)
      << "Extension struct should be a singleton";
}

TEST(ExtensionTest, InstallationManager) {
  typedef CobaltExtensionInstallationManagerApi ExtensionApi;
  const char* kExtensionName = kCobaltExtensionInstallationManagerName;

  const ExtensionApi* extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  if (!extension_api) {
    return;
  }

  EXPECT_STREQ(extension_api->name, kExtensionName);
  EXPECT_GE(extension_api->version, 1u);
  EXPECT_LE(extension_api->version, 3u);
  EXPECT_NE(extension_api->GetCurrentInstallationIndex, nullptr);
  EXPECT_NE(extension_api->MarkInstallationSuccessful, nullptr);
  EXPECT_NE(extension_api->RequestRollForwardToInstallation, nullptr);
  EXPECT_NE(extension_api->GetInstallationPath, nullptr);
  EXPECT_NE(extension_api->SelectNewInstallationIndex, nullptr);
  EXPECT_NE(extension_api->GetAppKey, nullptr);
  EXPECT_NE(extension_api->GetMaxNumberInstallations, nullptr);
  EXPECT_NE(extension_api->ResetInstallation, nullptr);
  EXPECT_NE(extension_api->Reset, nullptr);
  const ExtensionApi* second_extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  EXPECT_EQ(second_extension_api, extension_api)
      << "Extension struct should be a singleton";
}

TEST(ExtensionTest, Configuration) {
  typedef CobaltExtensionConfigurationApi ExtensionApi;
  const char* kExtensionName = kCobaltExtensionConfigurationName;

  const ExtensionApi* extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  if (!extension_api) {
    return;
  }

  EXPECT_STREQ(extension_api->name, kExtensionName);
  EXPECT_GE(extension_api->version, 1u);
  EXPECT_LE(extension_api->version, 3u);
  EXPECT_NE(extension_api->CobaltUserOnExitStrategy, nullptr);
  EXPECT_NE(extension_api->CobaltRenderDirtyRegionOnly, nullptr);
  EXPECT_NE(extension_api->CobaltEglSwapInterval, nullptr);
  EXPECT_NE(extension_api->CobaltFallbackSplashScreenUrl, nullptr);
  EXPECT_NE(extension_api->CobaltEnableQuic, nullptr);
  EXPECT_NE(extension_api->CobaltSkiaCacheSizeInBytes, nullptr);
  EXPECT_NE(extension_api->CobaltOffscreenTargetCacheSizeInBytes, nullptr);
  EXPECT_NE(extension_api->CobaltEncodedImageCacheSizeInBytes, nullptr);
  EXPECT_NE(extension_api->CobaltImageCacheSizeInBytes, nullptr);
  EXPECT_NE(extension_api->CobaltLocalTypefaceCacheSizeInBytes, nullptr);
  EXPECT_NE(extension_api->CobaltRemoteTypefaceCacheSizeInBytes, nullptr);
  EXPECT_NE(extension_api->CobaltMeshCacheSizeInBytes, nullptr);
  EXPECT_NE(extension_api->CobaltSoftwareSurfaceCacheSizeInBytes, nullptr);
  EXPECT_NE(extension_api->CobaltImageCacheCapacityMultiplierWhenPlayingVideo,
            nullptr);
  EXPECT_NE(extension_api->CobaltSkiaGlyphAtlasWidth, nullptr);
  EXPECT_NE(extension_api->CobaltSkiaGlyphAtlasHeight, nullptr);
  EXPECT_NE(extension_api->CobaltJsGarbageCollectionThresholdInBytes, nullptr);
  EXPECT_NE(extension_api->CobaltReduceCpuMemoryBy, nullptr);
  EXPECT_NE(extension_api->CobaltReduceGpuMemoryBy, nullptr);
  EXPECT_NE(extension_api->CobaltGcZeal, nullptr);
  if (extension_api->version >= 2) {
    EXPECT_NE(extension_api->CobaltFallbackSplashScreenTopics, nullptr);
  }

  if (extension_api->version >= 3) {
    EXPECT_NE(extension_api->CobaltCanStoreCompiledJavascript, nullptr);
  }

  const ExtensionApi* second_extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  EXPECT_EQ(second_extension_api, extension_api)
      << "Extension struct should be a singleton";
}

TEST(ExtensionTest, MediaSession) {
  typedef CobaltExtensionMediaSessionApi ExtensionApi;
  const char* kExtensionName = kCobaltExtensionMediaSessionName;

  const ExtensionApi* extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  if (!extension_api) {
    return;
  }

  EXPECT_STREQ(extension_api->name, kExtensionName);
  EXPECT_EQ(extension_api->version, 1u);
  EXPECT_NE(extension_api->OnMediaSessionStateChanged, nullptr);

  const ExtensionApi* second_extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  EXPECT_EQ(second_extension_api, extension_api)
      << "Extension struct should be a singleton";
}

TEST(ExtensionTest, CrashHandler) {
  typedef CobaltExtensionCrashHandlerApi ExtensionApi;
  const char* kExtensionName = kCobaltExtensionCrashHandlerName;

  const ExtensionApi* extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  if (!extension_api) {
    return;
  }

  EXPECT_STREQ(extension_api->name, kExtensionName);
  EXPECT_GE(extension_api->version, 1u);
  EXPECT_LE(extension_api->version, 2u);
  EXPECT_NE(extension_api->OverrideCrashpadAnnotations, nullptr);

  if (extension_api->version >= 2) {
    EXPECT_NE(extension_api->SetString, nullptr);
  }

  const ExtensionApi* second_extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  EXPECT_EQ(second_extension_api, extension_api)
      << "Extension struct should be a singleton";
}

TEST(ExtensionTest, CWrappers) {
  typedef CobaltExtensionCWrappersApi ExtensionApi;
  const char* kExtensionName = kCobaltExtensionCWrappersName;

  const ExtensionApi* extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  if (!extension_api) {
    return;
  }

  EXPECT_STREQ(extension_api->name, kExtensionName);
  EXPECT_EQ(extension_api->version, 1u);
  EXPECT_NE(extension_api->PowWrapper, nullptr);

  const ExtensionApi* second_extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  EXPECT_EQ(second_extension_api, extension_api)
      << "Extension struct should be a singleton";
}

TEST(ExtensionTest, Font) {
  typedef CobaltExtensionFontApi ExtensionApi;
  const char* kExtensionName = kCobaltExtensionFontName;

  const ExtensionApi* extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  if (!extension_api) {
    return;
  }

  EXPECT_STREQ(extension_api->name, kExtensionName);
  EXPECT_EQ(extension_api->version, 1u);
  EXPECT_NE(extension_api->GetPathFallbackFontDirectory, nullptr);

  const ExtensionApi* second_extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  EXPECT_EQ(second_extension_api, extension_api)
      << "Extension struct should be a singleton";
}

TEST(ExtensionTest, JavaScriptCache) {
  typedef CobaltExtensionJavaScriptCacheApi ExtensionApi;
  const char* kExtensionName = kCobaltExtensionJavaScriptCacheName;

  const ExtensionApi* extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  if (!extension_api) {
    return;
  }

  EXPECT_STREQ(extension_api->name, kExtensionName);
  EXPECT_EQ(extension_api->version, 1u);
  EXPECT_NE(extension_api->GetCachedScript, nullptr);
  EXPECT_NE(extension_api->ReleaseCachedScriptData, nullptr);
  EXPECT_NE(extension_api->StoreCachedScript, nullptr);

  const ExtensionApi* second_extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  EXPECT_EQ(second_extension_api, extension_api)
      << "Extension struct should be a singleton";
}

TEST(ExtensionTest, UrlFetcherObserver) {
  typedef CobaltExtensionUrlFetcherObserverApi ExtensionApi;
  const char* kExtensionName = kCobaltExtensionUrlFetcherObserverName;

  const ExtensionApi* extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  if (!extension_api) {
    return;
  }

  EXPECT_STREQ(extension_api->name, kExtensionName);
  EXPECT_EQ(extension_api->version, 1u);
  EXPECT_NE(extension_api->FetcherCreated, nullptr);
  EXPECT_NE(extension_api->FetcherDestroyed, nullptr);
  EXPECT_NE(extension_api->StartURLRequest, nullptr);

  const ExtensionApi* second_extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  EXPECT_EQ(second_extension_api, extension_api)
      << "Extension struct should be a singleton";
}

TEST(ExtensionTest, UpdaterNotification) {
  typedef CobaltExtensionUpdaterNotificationApi ExtensionApi;
  const char* kExtensionName = kCobaltExtensionUpdaterNotificationName;

  const ExtensionApi* extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  if (!extension_api) {
    return;
  }

  EXPECT_STREQ(extension_api->name, kExtensionName);
  EXPECT_EQ(extension_api->version, 1u);
  EXPECT_NE(extension_api->UpdaterState, nullptr);

  const ExtensionApi* second_extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  EXPECT_EQ(second_extension_api, extension_api)
      << "Extension struct should be a singleton";
}

TEST(ExtensionTest, MemoryMappedFile) {
  typedef CobaltExtensionMemoryMappedFileApi ExtensionApi;
  const char* kExtensionName = kCobaltExtensionMemoryMappedFileName;

  const ExtensionApi* extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  if (!extension_api) {
    return;
  }

  EXPECT_STREQ(extension_api->name, kExtensionName);
  EXPECT_EQ(extension_api->version, 1u);
  EXPECT_NE(extension_api->MemoryMapFile, nullptr);

  const ExtensionApi* second_extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  EXPECT_EQ(second_extension_api, extension_api)
      << "Extension struct should be a singleton";
}

TEST(ExtensionTest, FreeSpace) {
  typedef CobaltExtensionFreeSpaceApi ExtensionApi;
  const char* kExtensionName = kCobaltExtensionFreeSpaceName;

  const ExtensionApi* extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  if (!extension_api) {
    return;
  }

  EXPECT_STREQ(extension_api->name, kExtensionName);
  EXPECT_EQ(extension_api->version, 1u);
  EXPECT_NE(extension_api->MeasureFreeSpace, nullptr);

  const ExtensionApi* second_extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  EXPECT_EQ(second_extension_api, extension_api)
      << "Extension struct should be a singleton";
}

TEST(ExtensionTest, EnhancedAudio) {
  typedef CobaltExtensionEnhancedAudioApi ExtensionApi;
  const char* kExtensionName = kCobaltExtensionEnhancedAudioName;

  const ExtensionApi* extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  if (!extension_api) {
    return;
  }

#if SB_API_VERSION >= 15
  ASSERT_FALSE(extension_api)
      << "EnhancedAudio extension shouldn't be used under SB_API_VERSION "
      << SB_API_VERSION
      << ", the features are supported by the current SbPlayer api by default."
      << "\nTo upgrade the EnhancedAudio extension based implementation from"
      << " a previous Starboard version, please rename the `PlayerWriteSamples`"
      << " implementation to `SbPlayerWriteSample()` (as they are compatible"
      << " at abi level) and disable the EnhancedAudio extension from"
      << " `SbSystemGetExtension()`.";
#endif  // SB_API_VERSION >= 15

  EXPECT_STREQ(extension_api->name, kExtensionName);
  EXPECT_EQ(extension_api->version, 1u);
  EXPECT_NE(extension_api->PlayerWriteSamples, nullptr);

  const ExtensionApi* second_extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  EXPECT_EQ(second_extension_api, extension_api)
      << "Extension struct should be a singleton";
}

TEST(ExtensionTest, PlatformInfo) {
  typedef CobaltExtensionPlatformInfoApi ExtensionApi;
  const char* kExtensionName = kCobaltExtensionPlatformInfoName;

  const ExtensionApi* extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  if (!extension_api) {
    return;
  }

  EXPECT_STREQ(extension_api->name, kExtensionName);
  EXPECT_EQ(extension_api->version, 1u);
  EXPECT_NE(extension_api->GetFirmwareVersionDetails, nullptr);
  EXPECT_NE(extension_api->GetOsExperience, nullptr);

  const ExtensionApi* second_extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  EXPECT_EQ(second_extension_api, extension_api)
      << "Extension struct should be a singleton";
}

TEST(ExtensionTest, TimeZone) {
  typedef StarboardExtensionTimeZoneApi ExtensionApi;
  const char* kExtensionName = kStarboardExtensionTimeZoneName;

  const ExtensionApi* extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  if (!extension_api) {
    return;
  }

  EXPECT_STREQ(extension_api->name, kExtensionName);
  EXPECT_EQ(extension_api->version, 1u);
  EXPECT_NE(extension_api->SetTimeZone, nullptr);

  const ExtensionApi* second_extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  EXPECT_EQ(second_extension_api, extension_api)
      << "Extension struct should be a singleton";
}

TEST(ExtensionTest, Ifa) {
  typedef StarboardExtensionIfaApi ExtensionApi;
  const char* kExtensionName = kStarboardExtensionIfaName;

  const ExtensionApi* extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  if (!extension_api) {
    return;
  }

  EXPECT_STREQ(extension_api->name, kExtensionName);
  EXPECT_EQ(extension_api->version, 1u);
  EXPECT_NE(extension_api->GetAdvertisingId, nullptr);
  EXPECT_NE(extension_api->GetLimitAdTracking, nullptr);

  const ExtensionApi* second_extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  EXPECT_EQ(second_extension_api, extension_api)
      << "Extension struct should be a singleton";
}

}  // namespace extension
}  // namespace starboard
