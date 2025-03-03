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

#include <sys/stat.h>

#include <string>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/configuration.h"
#include "starboard/configuration_constants.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !SB_IS(EVERGREEN_COMPATIBLE)
#error These tests apply only to EVERGREEN_COMPATIBLE platforms.
#endif

#if defined(ANDROID)
#error These tests are not applicable to AOSP
#endif

namespace starboard {
namespace nplb {
namespace nplb_evergreen_compat_tests {

namespace {

const char kFileName[] = "fonts.xml";

TEST(FontsTest, VerifySystemFontsDirectory) {
  std::vector<char> system_fonts_dir(kSbFileMaxPath);
  ASSERT_TRUE(SbSystemGetPath(kSbSystemPathFontDirectory,
                              system_fonts_dir.data(), kSbFileMaxPath));

  struct stat info;
  ASSERT_TRUE(stat(system_fonts_dir.data(), &info) == 0);
}

TEST(FontsTest, VerifySystemFontsConfigDirectory) {
  std::vector<char> system_fonts_conf_dir(kSbFileMaxPath);
  ASSERT_TRUE(SbSystemGetPath(kSbSystemPathFontConfigurationDirectory,
                              system_fonts_conf_dir.data(), kSbFileMaxPath));
  struct stat info;
  ASSERT_TRUE(stat(system_fonts_conf_dir.data(), &info) == 0);

  std::string fonts_descriptor_file = system_fonts_conf_dir.data();
  fonts_descriptor_file += kSbFileSepString;
  fonts_descriptor_file += kFileName;

  struct stat file_info;
  ASSERT_TRUE(stat(fonts_descriptor_file.c_str(), &file_info) == 0);
}

}  // namespace
}  // namespace nplb_evergreen_compat_tests
}  // namespace nplb
}  // namespace starboard
