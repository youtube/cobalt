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

#include <sys/stat.h>

#include <string>
#include <vector>

#include "starboard/configuration_constants.h"
#include "starboard/extension/crash_handler.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_IS(EVERGREEN_COMPATIBLE)

namespace starboard {
namespace nplb {
namespace nplb_evergreen_compat_tests {

namespace {

class CrashpadConfigTest : public ::testing::Test {
 protected:
  CrashpadConfigTest() {}
  ~CrashpadConfigTest() {}
};

// These tests are not applicable to AOSP
#if !defined(ANDROID)

TEST_F(CrashpadConfigTest, VerifyUploadCert) {
  std::vector<char> buffer(kSbFileMaxPath);
  ASSERT_TRUE(SbSystemGetPath(kSbSystemPathContentDirectory, buffer.data(),
                              buffer.size()));

  std::string cert_location(buffer.data());
  cert_location.append(std::string(kSbFileSepString) + "app" +
                       kSbFileSepString + "cobalt" + kSbFileSepString +
                       "content" + kSbFileSepString + "ssl" + kSbFileSepString +
                       "certs");
  struct stat info;
  ASSERT_TRUE(stat(cert_location.c_str(), &info) == 0);
}

#endif  // !defined(ANDROID)

// N.B. No need to test SbSystemGetExtension(kCobaltExtensionCrashHandlerName),
// since that is done in ExtensionTest.CrashHandler.

}  // namespace
}  // namespace nplb_evergreen_compat_tests
}  // namespace nplb
}  // namespace starboard

#endif  // SB_IS(EVERGREEN_COMPATIBLE)
