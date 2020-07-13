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

#include "cobalt/extension/configuration.h"
#include "cobalt/extension/graphics.h"
#include "cobalt/extension/installation_manager.h"
#include "cobalt/extension/platform_service.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_API_VERSION >= 11
namespace cobalt {
namespace extension {

TEST(ExtensionTest, PlatformService) {
  typedef CobaltExtensionPlatformServiceApi ExtensionApi;
  const char* kExtensionName = kCobaltExtensionPlatformServiceName;

  const ExtensionApi* extension_api = static_cast<const ExtensionApi*>(
      SbSystemGetExtension(kExtensionName));
  if (!extension_api) {
    return;
  }

  EXPECT_STREQ(extension_api->name, kExtensionName);
  EXPECT_TRUE(extension_api->version == 1 ||
              extension_api->version == 2 ||
              extension_api->version == 3) << "Invalid version";
  EXPECT_TRUE(extension_api->Has != NULL);
  EXPECT_TRUE(extension_api->Open != NULL);
  EXPECT_TRUE(extension_api->Close != NULL);
  EXPECT_TRUE(extension_api->Send != NULL);

  const ExtensionApi* second_extension_api = static_cast<const ExtensionApi*>(
      SbSystemGetExtension(kExtensionName));
  EXPECT_EQ(second_extension_api, extension_api)
      << "Extension struct should be a singleton";
}

TEST(ExtensionTest, Graphics) {
  typedef CobaltExtensionGraphicsApi ExtensionApi;
  const char* kExtensionName = kCobaltExtensionGraphicsName;

  const ExtensionApi* extension_api = static_cast<const ExtensionApi*>(
      SbSystemGetExtension(kExtensionName));
  if (!extension_api) {
    return;
  }

  EXPECT_STREQ(extension_api->name, kExtensionName);
  EXPECT_TRUE(extension_api->version == 1 ||
              extension_api->version == 2 ||
              extension_api->version == 3) << "Invalid version";
  EXPECT_TRUE(extension_api->GetMaximumFrameIntervalInMilliseconds != NULL);
  if (extension_api->version >= 2) {
    EXPECT_TRUE(extension_api->GetMinimumFrameIntervalInMilliseconds != NULL);
  }
  if (extension_api->version >= 3) {
    EXPECT_TRUE(extension_api->IsMapToMeshEnabled != NULL);
  }

  float maximum_frame_interval =
      extension_api->GetMaximumFrameIntervalInMilliseconds();
  EXPECT_FALSE(std::isnan(maximum_frame_interval));

  if (extension_api->version >= 2) {
    float minimum_frame_interval =
      extension_api->GetMinimumFrameIntervalInMilliseconds();
    EXPECT_GT(minimum_frame_interval, 0);
  }
  const ExtensionApi* second_extension_api = static_cast<const ExtensionApi*>(
      SbSystemGetExtension(kExtensionName));
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
  EXPECT_TRUE(extension_api->version == 1 ||
              extension_api->version == 2 ||
              extension_api->version == 3) << "Invalid version";
  EXPECT_TRUE(extension_api->GetCurrentInstallationIndex != NULL);
  EXPECT_TRUE(extension_api->MarkInstallationSuccessful != NULL);
  EXPECT_TRUE(extension_api->RequestRollForwardToInstallation != NULL);
  EXPECT_TRUE(extension_api->GetInstallationPath != NULL);
  EXPECT_TRUE(extension_api->SelectNewInstallationIndex != NULL);
  EXPECT_TRUE(extension_api->GetAppKey != NULL);
  EXPECT_TRUE(extension_api->GetMaxNumberInstallations != NULL);
  EXPECT_TRUE(extension_api->ResetInstallation != NULL);
  EXPECT_TRUE(extension_api->Reset != NULL);
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
  EXPECT_TRUE(extension_api->version == 1);
  EXPECT_TRUE(extension_api->CobaltUserOnExitStrategy != NULL);
  EXPECT_TRUE(extension_api->CobaltRenderDirtyRegionOnly != NULL);
  EXPECT_TRUE(extension_api->CobaltEglSwapInterval != NULL);
  EXPECT_TRUE(extension_api->CobaltFallbackSplashScreenUrl != NULL);
  EXPECT_TRUE(extension_api->CobaltEnableQuic != NULL);
  EXPECT_TRUE(extension_api->CobaltSkiaCacheSizeInBytes != NULL);
  EXPECT_TRUE(extension_api->CobaltOffscreenTargetCacheSizeInBytes != NULL);
  EXPECT_TRUE(extension_api->CobaltEncodedImageCacheSizeInBytes != NULL);
  EXPECT_TRUE(extension_api->CobaltImageCacheSizeInBytes != NULL);
  EXPECT_TRUE(extension_api->CobaltLocalTypefaceCacheSizeInBytes != NULL);
  EXPECT_TRUE(extension_api->CobaltRemoteTypefaceCacheSizeInBytes != NULL);
  EXPECT_TRUE(extension_api->CobaltMeshCacheSizeInBytes != NULL);
  EXPECT_TRUE(extension_api->CobaltSoftwareSurfaceCacheSizeInBytes != NULL);
  EXPECT_TRUE(extension_api->CobaltImageCacheCapacityMultiplierWhenPlayingVideo != NULL);
  EXPECT_TRUE(extension_api->CobaltSkiaGlyphAtlasWidth != NULL);
  EXPECT_TRUE(extension_api->CobaltSkiaGlyphAtlasHeight != NULL);
  EXPECT_TRUE(extension_api->CobaltJsGarbageCollectionThresholdInBytes != NULL);
  EXPECT_TRUE(extension_api->CobaltReduceCpuMemoryBy != NULL);
  EXPECT_TRUE(extension_api->CobaltReduceGpuMemoryBy != NULL);
  EXPECT_TRUE(extension_api->CobaltGcZeal != NULL);

  const ExtensionApi* second_extension_api =
      static_cast<const ExtensionApi*>(SbSystemGetExtension(kExtensionName));
  EXPECT_EQ(second_extension_api, extension_api)
      << "Extension struct should be a singleton";
}
}  // namespace extension
}  // namespace cobalt
#endif  // SB_API_VERSION >= 11
