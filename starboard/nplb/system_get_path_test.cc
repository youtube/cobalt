// Copyright 2015 Google Inc. All Rights Reserved.
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

#include <string.h>

#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

// Size of appropriate path buffer.
const size_t kPathSize = SB_FILE_MAX_PATH + 1;

void BasicTest(SbSystemPathId id,
               bool expect_result,
               bool expected_result,
               int line) {
#define LOCAL_CONTEXT "Context : id=" << id << ", line=" << line;
  char path[kPathSize] = {0};
  memset(path, 0xCD, kPathSize);
  bool result = SbSystemGetPath(id, path, kPathSize);
  if (expect_result) {
    EXPECT_EQ(expected_result, result) << LOCAL_CONTEXT;
  }
  if (result) {
    EXPECT_NE('\xCD', path[0]) << LOCAL_CONTEXT;
    int len = static_cast<int>(strlen(path));
    EXPECT_GT(len, 0) << LOCAL_CONTEXT;
  } else {
    EXPECT_EQ('\xCD', path[0]) << LOCAL_CONTEXT;
  }
#undef LOCAL_CONTEXT
}

TEST(SbSystemGetPathTest, ReturnsRequiredPaths) {
  BasicTest(kSbSystemPathContentDirectory, true, true, __LINE__);
}

TEST(SbSystemGetPathTest, FailsGracefullyZeroBufferLength) {
  char path[kPathSize] = {0};
  bool result = SbSystemGetPath(kSbSystemPathContentDirectory, path, 0);
  EXPECT_FALSE(result);
}

TEST(SbSystemGetPathTest, FailsGracefullyNullBuffer) {
  char path[kPathSize] = {0};
  bool result = SbSystemGetPath(kSbSystemPathContentDirectory, NULL, kPathSize);
  EXPECT_FALSE(result);
}

TEST(SbSystemGetPathTest, FailsGracefullyNullBufferAndZeroLength) {
  bool result = SbSystemGetPath(kSbSystemPathContentDirectory, NULL, 0);
  EXPECT_FALSE(result);
}

TEST(SbSystemGetPathTest, FailsGracefullyBogusId) {
  BasicTest(static_cast<SbSystemPathId>(999), true, false, __LINE__);
}

TEST(SbSystemGetPathTest, DoesNotBlowUpForDefinedIds) {
  BasicTest(kSbSystemPathDebugOutputDirectory, false, false, __LINE__);
  BasicTest(kSbSystemPathSourceDirectory, false, false, __LINE__);
  BasicTest(kSbSystemPathTempDirectory, false, false, __LINE__);
  BasicTest(kSbSystemPathTestOutputDirectory, false, false, __LINE__);
#if SB_API_VERSION >= 4
  BasicTest(kSbSystemPathFontDirectory, false, false, __LINE__);
  BasicTest(kSbSystemPathFontConfigurationDirectory, false, false, __LINE__);
#endif  // SB_API_VERSION >= 4
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
