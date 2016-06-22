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

#include "starboard/nplb/file_helpers.h"

#include <sstream>
#include <string>

#include "starboard/file.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {

namespace {
// Size of appropriate path buffer.
const size_t kPathSize = SB_FILE_MAX_PATH + 1;
}  // namespace

std::string GetTempDir() {
  // It seems there's absolutely no way to get to std::string without a copy.
  char path[kPathSize] = {0};
  if (!SbSystemGetPath(kSbSystemPathTempDirectory, path,
                       SB_ARRAY_SIZE_INT(path))) {
    return "";
  }

  return path;
}

// static
void ScopedRandomFile::ExpectPattern(int pattern_offset,
                                     void* buffer,
                                     int size,
                                     int line) {
  char* char_buffer = static_cast<char*>(buffer);
  for (int i = 0; i < size; ++i) {
    EXPECT_EQ(static_cast<char>((i + pattern_offset) & 0xFF), char_buffer[i])
        << "original line=" << line;
  }
}

// static
std::string ScopedRandomFile::MakeRandomFilename() {
  char path[kPathSize] = {0};
  bool result = SbSystemGetPath(kSbSystemPathTempDirectory, path, kPathSize);
  EXPECT_TRUE(result);
  if (!result) {
    return "";
  }

  std::ostringstream filename_stream;
  filename_stream << path << "/ScopedRandomFile.File_"
                  << SbSystemGetRandomUInt64();
  return filename_stream.str();
}

std::string ScopedRandomFile::MakeRandomFile(int length) {
  std::string filename = MakeRandomFilename();
  if (filename.empty()) {
    return filename;
  }

  SbFile file = SbFileOpen(filename.c_str(), kSbFileCreateOnly | kSbFileWrite,
                           NULL, NULL);
  EXPECT_TRUE(SbFileIsValid(file));
  if (!SbFileIsValid(file)) {
    return "";
  }

  char* data = new char[length];
  for (int i = 0; i < length; ++i) {
    data[i] = static_cast<char>(i & 0xFF);
  }

  int bytes = SbFileWrite(file, data, length);
  EXPECT_EQ(bytes, length) << "Failed to write " << length << " bytes to "
                           << filename;

  bool result = SbFileClose(file);
  EXPECT_TRUE(result) << "Failed to close " << filename;
  delete[] data;
  return filename;
}

}  // namespace nplb
}  // namespace starboard
