// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/common/paths.h"
#include "starboard/extension/crash_handler.h"
#include "starboard/nplb/nplb_evergreen_compat_tests/checks.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(IS_STARBOARD)
#error These tests apply only to Starboard platforms.
#endif

namespace nplb {

namespace {

TEST(CrashpadConfigTest, VerifyUploadCert) {
  EXPECT_FALSE(starboard::GetCACertificatesPath().empty());
}

TEST(CrashpadConfigTest, VerifyCrashHandlerExtension) {
  auto* crash_handler_extension =
      static_cast<const CobaltExtensionCrashHandlerApi*>(
          SbSystemGetExtension(kCobaltExtensionCrashHandlerName));
  ASSERT_TRUE(crash_handler_extension != nullptr);
  EXPECT_STREQ(crash_handler_extension->name, kCobaltExtensionCrashHandlerName);
  EXPECT_GE(crash_handler_extension->version, 4u);
  EXPECT_TRUE(crash_handler_extension->SetString != nullptr);
  EXPECT_TRUE(crash_handler_extension->RegisterSetStringCallback != nullptr);
  EXPECT_TRUE(crash_handler_extension->DumpWithoutCrashing != nullptr);
  EXPECT_TRUE(crash_handler_extension->RegisterDumpWithoutCrashingCallback !=
              nullptr);
}

}  // namespace
}  // namespace nplb
