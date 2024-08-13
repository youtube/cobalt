// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include <fcntl.h>

#include "starboard/common/string.h"
#include "starboard/file.h"
#include "starboard/nplb/file_helpers.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

TEST(SbSystemGetErrorStringTest, SunnyDay) {
  // Opening a non-existent file should generate an error on all platforms.
  ScopedRandomFile random_file(ScopedRandomFile::kDontCreate);
  int file = open(random_file.filename().c_str(), O_RDONLY, S_IRUSR | S_IWUSR);

  SbSystemError error = SbSystemGetLastError();
  EXPECT_NE(0, error);
  {
    char name[128] = {0};
    int len = SbSystemGetErrorString(error, name, SB_ARRAY_SIZE_INT(name));
    EXPECT_LT(0, len);
    EXPECT_LT(0, strlen(name));
    if (len < SB_ARRAY_SIZE_INT(name)) {
      EXPECT_EQ(len, strlen(name));
    }
  }

  {
    char name[128] = {0};
    int len = SbSystemGetErrorString(error, name, 0);
    EXPECT_LT(0, len);
    EXPECT_EQ(0, strlen(name));
  }

  {
    int len = SbSystemGetErrorString(error, NULL, 0);
    EXPECT_LT(0, len);
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
