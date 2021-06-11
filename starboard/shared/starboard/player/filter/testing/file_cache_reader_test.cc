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

#include "starboard/shared/starboard/player/file_cache_reader.h"

#include <algorithm>
#include <string>
#include <vector>

#include "starboard/common/file.h"
#include "starboard/configuration_constants.h"
#include "starboard/memory.h"
#include "starboard/shared/starboard/player/filter/testing/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {
namespace testing {
namespace {

const char kTestFilename[] = "beneath_the_canopy_aac_stereo.dmp";

class FileCacheReaderTest : public ::testing::Test {
 public:
  FileCacheReaderTest()
      : file_cache_reader_(ResolveTestFileName(kTestFilename).c_str(),
                           1024 * 1024) {}

 protected:
  FileCacheReader file_cache_reader_;
};

TEST_F(FileCacheReaderTest, FileCacheReader) {
  const std::vector<int> sizes_to_read = {0, 1, 2, 3, 4, 0, 5, 8, 13};

  std::vector<char> true_contents;
  {
    // Use a 5MB snippet from the true file to compare against.
    const size_t kTestSize = 5 * 1024 * 1024;
    ScopedFile file(ResolveTestFileName(kTestFilename).c_str(),
                    kSbFileOpenOnly | kSbFileRead);
    true_contents.resize(kTestSize);
    int bytes_read = file.ReadAll(true_contents.data(), kTestSize);
    SB_CHECK(bytes_read == kTestSize);
  }

  // Output buffer for file reading, when it is read in chunks.
  std::vector<char> read_contents;

  int true_offset = 0;
  int size_index = 0;
  while (true_offset < true_contents.size()) {
    auto size_to_read =
        std::min(sizes_to_read[size_index],
                 static_cast<int>(true_contents.size()) - true_offset);
    size_index = (size_index + 1) % sizes_to_read.size();

    read_contents.resize(size_to_read);
    int bytes_read =
        file_cache_reader_.Read(read_contents.data(), size_to_read);

    ASSERT_EQ(bytes_read, size_to_read);
    // Compare the true content of these files, with the chunked contents of
    // these files to ensure that file I/O is working as expected.
    int result = memcmp(
        read_contents.data(), true_contents.data() + true_offset, bytes_read);
    ASSERT_EQ(0, result);
    true_offset += bytes_read;
  }
  EXPECT_EQ(true_offset, true_contents.size());
}

}  // namespace
}  // namespace testing
}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
