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

#include "starboard/elf_loader/lz4_file_impl.h"

#include "starboard/configuration_constants.h"
#include "starboard/string.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace elf_loader {
namespace {

// The expected result of decompressing the test file is 64 bytes of 'a'. The
// data is compressed more than 50% which allows us to validate that the buffer
// used for decompression grows beyond the initial allocation size of 2x the
// compressed file size.
constexpr char kUncompressedData[] =
    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

TEST(LZ4FileImplTest, RainyDayOpenNonExistentFile) {
  LZ4FileImpl file;
  EXPECT_FALSE(file.Open("thisfiledoesnotexist"));
}

TEST(LZ4FileImplTest, RainyDayReadBeforeOpening) {
  LZ4FileImpl file;
  EXPECT_FALSE(file.ReadFromOffset(/* offset = */ 0,
                                   /* buffer = */ nullptr,
                                   /* size   = */ 0));
}

TEST(LZ4FileImplTest, RainyDayReadNegativeOffset) {
  LZ4FileImpl file;
  EXPECT_FALSE(file.ReadFromOffset(/* offset = */ -1,
                                   /* buffer = */ nullptr,
                                   /* size   = */ 0));
}

TEST(LZ4FileImplTest, SunnyDayDecompressesFile) {
  std::vector<char> content_path(kSbFileMaxPath + 1);
  EXPECT_TRUE(SbSystemGetPath(kSbSystemPathContentDirectory,
                              content_path.data(), kSbFileMaxPath + 1));
  std::string file_path = std::string(content_path.data()) + kSbFileSepChar +
                          "test" + kSbFileSepChar + "starboard" +
                          kSbFileSepChar + "elf_loader" + kSbFileSepChar +
                          "testdata" + kSbFileSepChar + "compressed.lz4";

  LZ4FileImpl file;

  EXPECT_TRUE(file.Open(file_path.c_str()));

  char decompressed[SB_ARRAY_SIZE(kUncompressedData)]{0};

  EXPECT_TRUE(file.ReadFromOffset(0, decompressed,
                                  SB_ARRAY_SIZE(kUncompressedData) - 1));
  EXPECT_EQ(strcmp(decompressed, kUncompressedData), 0);
}

}  // namespace
}  // namespace elf_loader
}  // namespace starboard
