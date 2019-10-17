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

#include "starboard/file.h"
#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_API_VERSION >= SB_FILE_ATOMIC_REPLACE_VERSION

namespace starboard {
namespace nplb {
namespace {

static const char kTestContents[] =
    "The quick brown fox jumps over the lazy dog.";
static const int kTestContentsLength = sizeof(kTestContents);

TEST(SbFileAtomicReplaceTest, ReplacesValidFile) {
  ScopedRandomFile random_file(ScopedRandomFile::kDefaultLength,
                               ScopedRandomFile::kCreate);
  const std::string& filename = random_file.filename();

  EXPECT_TRUE(SbFileExists(filename.c_str()));
  EXPECT_TRUE(SbFileAtomicReplace(filename.c_str(), kTestContents,
                                  kTestContentsLength));

  char result[kTestContentsLength];

  SbFileError error;
  SbFile file = SbFileOpen(filename.c_str(), kSbFileOpenOnly | kSbFileRead,
                           nullptr, &error);

  EXPECT_EQ(kSbFileOk, error);
  EXPECT_EQ(kTestContentsLength, SbFileRead(file, result, kTestContentsLength));
  EXPECT_EQ(0, SbStringCompare(kTestContents, result, kTestContentsLength));
  EXPECT_TRUE(SbFileClose(file));
}

TEST(SbFileAtomicReplaceTest, FailsWithNonExistentFile) {
  ScopedRandomFile random_file(ScopedRandomFile::kDontCreate);
  const std::string& filename = random_file.filename();

  EXPECT_FALSE(SbFileExists(filename.c_str()));
  EXPECT_FALSE(SbFileAtomicReplace(filename.c_str(), kTestContents,
                                   kTestContentsLength));
}

TEST(SbFileAtomicReplaceTest, FailsWithNoData) {
  ScopedRandomFile random_file(ScopedRandomFile::kCreate);
  const std::string& filename = random_file.filename();

  EXPECT_TRUE(SbFileExists(filename.c_str()));
  EXPECT_FALSE(SbFileAtomicReplace(filename.c_str(), nullptr,
                                   kTestContentsLength));
}

TEST(SbFileAtomicReplaceTest, FailsWithInvalidLength) {
  ScopedRandomFile random_file(ScopedRandomFile::kCreate);
  const std::string& filename = random_file.filename();

  EXPECT_TRUE(SbFileExists(filename.c_str()));
  EXPECT_FALSE(SbFileAtomicReplace(filename.c_str(), kTestContents, -1));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard

#endif  // SB_API_VERSION >= SB_FILE_ATOMIC_REPLACE_VERSION
