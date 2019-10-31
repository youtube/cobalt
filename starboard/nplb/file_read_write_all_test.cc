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

namespace starboard {
namespace nplb {
namespace {

class SbReadWriteAllTestWithBuffer
    : public ::testing::TestWithParam<int> {
 public:
  int GetBufferSize() { return GetParam(); }
};

TEST_P(SbReadWriteAllTestWithBuffer, ReadFile) {
  ScopedRandomFile random_file(0, ScopedRandomFile::kDontCreate);
  const std::string& filename = random_file.filename();

  SbFile file = SbFileOpen(
      filename.c_str(), kSbFileCreateAlways | kSbFileWrite, NULL, NULL);

  std::vector<char> file_contents;
  file_contents.reserve(GetBufferSize());
  for (int i = 0; i < GetBufferSize(); ++i) {
    file_contents.push_back(i % 255);
  }
  int bytes_written =
      SbFileWriteAll(file, file_contents.data(), file_contents.size());
  EXPECT_EQ(GetBufferSize(), bytes_written);

  SbFileClose(file);

  file = SbFileOpen(
      filename.c_str(), kSbFileOpenOnly | kSbFileRead, NULL, NULL);
  std::vector<char> read_contents(GetBufferSize());
  int bytes_read =
      SbFileReadAll(file, read_contents.data(), read_contents.size());
  EXPECT_EQ(GetBufferSize(), bytes_read);
  EXPECT_EQ(file_contents, read_contents);

  SbFileClose(file);
}

INSTANTIATE_TEST_CASE_P(
    SbReadAllTestSbReadWriteAllTest,
    SbReadWriteAllTestWithBuffer,
    ::testing::Values(0, 1, 1024, 16 * 1024, 128 * 1024, 1024 * 1024));

}  // namespace
}  // namespace nplb
}  // namespace starboard
