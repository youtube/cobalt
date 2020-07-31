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

#include <string>
#include <vector>

#include "starboard/common/log.h"
#include "starboard/configuration.h"
#include "starboard/file.h"
#include "starboard/nplb/nplb_evergreen_compat_tests/checks.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_IS(EVERGREEN_COMPATIBLE)

namespace starboard {
namespace nplb {
namespace nplb_evergreen_compat_tests {

namespace {

const char kDirName[] = "icu";

TEST(IcuTest, VerifyIcuDirectory) {
  std::vector<char> storage_dir(kSbFileMaxPath);
  ASSERT_TRUE(SbSystemGetPath(kSbSystemPathStorageDirectory, storage_dir.data(),
                              kSbFileMaxPath));

  std::string icu_path = storage_dir.data();
  icu_path += kSbFileSepString;
  icu_path += kDirName;
  ASSERT_TRUE(SbFileExists(icu_path.c_str()));
  SbFileInfo info;
  ASSERT_TRUE(SbFileGetPathInfo(icu_path.c_str(), &info));
  ASSERT_TRUE(info.is_directory);
}

}  // namespace
}  // namespace nplb_evergreen_compat_tests
}  // namespace nplb
}  // namespace starboard

#endif  // SB_IS(EVERGREEN_COMPATIBLE)
