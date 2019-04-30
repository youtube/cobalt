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

#include "cobalt/extension/platform_service.h"
#include "starboard/log.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_API_VERSION >= SB_EXTENSIONS_API_VERSION
namespace starboard {
namespace nplb {
namespace {

TEST(SbSystemGetExtension, MadeUpExtension) {
  void* fing_longer = SbSystemGetExtension("dev.cobalt.extension.FingLonger");
  EXPECT_TRUE(fing_longer == NULL);
}

TEST(SbSystemGetExtension, WebPlatformServices) {
  const char* kExtensionName = kCobaltExtensionPlatformServiceName;
  CobaltExtensionPlatformServiceApi* platform_service_api =
      static_cast<CobaltExtensionPlatformServiceApi*>(
          SbSystemGetExtension(kExtensionName));
  if (!platform_service_api) {
    return;
  }
  EXPECT_STREQ(platform_service_api->kName, kExtensionName);
  EXPECT_EQ(platform_service_api->kVersion, 1)
      << "PlatfomServiceApi version invalid";
  EXPECT_TRUE(platform_service_api->Has != NULL);
  EXPECT_TRUE(platform_service_api->Open != NULL);
  EXPECT_TRUE(platform_service_api->Close != NULL);
  EXPECT_TRUE(platform_service_api->Send != NULL);

  CobaltExtensionPlatformServiceApi* second_platform_service_api =
      static_cast<CobaltExtensionPlatformServiceApi*>(
          SbSystemGetExtension(kExtensionName));

  EXPECT_EQ(second_platform_service_api, platform_service_api)
      << "Extension struct should be a singleton";
}
}  // namespace
}  // namespace nplb
}  // namespace starboard
#endif  // SB_API_VERSION >= SB_EXTENSIONS_API_VERSION
