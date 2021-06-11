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

#include "starboard/common/file.h"
#include "starboard/common/log.h"
#include "starboard/configuration.h"
#include "starboard/memory.h"
#include "starboard/nplb/nplb_evergreen_compat_tests/checks.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

#if SB_IS(EVERGREEN_COMPATIBLE)

namespace starboard {
namespace nplb {
namespace nplb_evergreen_compat_tests {

namespace {

const char kFileName[] = "test_file.data";
const size_t kBufSize = 96 * 1024 * 1024;  // 96 MB

class StorageTest : public ::testing::Test {
 protected:
  StorageTest() {}
  ~StorageTest() {}
};

void WriteBuffer(const char* file_path,
                 const char* buffer,
                 size_t buffer_size) {
  SbFileError error;
  ScopedFile file(file_path, kSbFileOpenAlways | kSbFileWrite, nullptr, &error);
  ASSERT_EQ(kSbFileOk, error) << "Failed to open file for writing";
  int bytes_written = file.WriteAll(buffer, buffer_size);
  ASSERT_EQ(kBufSize, bytes_written);
}

void ReadBuffer(const char* file_path, char* buffer, size_t buffer_size) {
  SbFileError error;
  ScopedFile file(file_path, kSbFileOpenOnly | kSbFileRead, nullptr, &error);
  ASSERT_EQ(kSbFileOk, error) << "Failed to open file for reading";
  int count = file.ReadAll(buffer, buffer_size);
  ASSERT_EQ(kBufSize, count);
}

TEST_F(StorageTest, VerifyStorageDirectory) {
  std::vector<char> storage_dir(kSbFileMaxPath);
  ASSERT_TRUE(SbSystemGetPath(kSbSystemPathStorageDirectory, storage_dir.data(),
                              kSbFileMaxPath));

  std::string file_path = storage_dir.data();
  file_path += kSbFileSepString;
  file_path += kFileName;
  SB_LOG(INFO) << "file: " << file_path;

  std::vector<char> buf(kBufSize);
  memset(buf.data(), 'A', kBufSize);

  WriteBuffer(file_path.c_str(), buf.data(), kBufSize);

  memset(buf.data(), 0, kBufSize);

  ReadBuffer(file_path.c_str(), buf.data(), kBufSize);

  for (int i = 0; i < kBufSize; i++) {
    ASSERT_EQ('A', buf[i]);
  }

  ASSERT_TRUE(SbFileDelete(file_path.data()));
  ASSERT_FALSE(SbFileExists(file_path.data()));
}

}  // namespace
}  // namespace nplb_evergreen_compat_tests
}  // namespace nplb
}  // namespace starboard

#endif  // SB_IS(EVERGREEN_COMPATIBLE)
