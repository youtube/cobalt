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

#include "starboard/nplb/file_helpers.h"

#include <sstream>
#include <string>
#include <vector>

#include "starboard/configuration_constants.h"
#include "starboard/file.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {

namespace {
// Size of appropriate path buffer.
const size_t kPathSize = kSbFileMaxPath + 1;
}  // namespace

std::string GetTempDir() {
  // It seems there's absolutely no way to get to std::string without a copy.
  std::vector<char> path(kPathSize, 0);
  if (!SbSystemGetPath(kSbSystemPathTempDirectory, path.data(), path.size())) {
    return "";
  }

  return path.data();
}

std::string GetFileTestsDataDir() {
  std::vector<char> content_path(kPathSize);
  EXPECT_TRUE(SbSystemGetPath(kSbSystemPathContentDirectory,
                              content_path.data(), kPathSize));
  std::string directory_path =
      std::string(content_path.data()) + kSbFileSepChar + "test" +
      kSbFileSepChar + "starboard" + kSbFileSepChar + "nplb" +
      kSbFileSepChar + "file_tests";
  SB_CHECK(SbDirectoryCanOpen(directory_path.c_str()));
  return directory_path;
}

// Make a vector of absolute paths in our test data from a null-terminated array
// of C-strings with the relative paths. Slashes are converted to the platform's
// delimiter.
std::vector<std::string> MakePathsVector(const char* files[]) {
  std::string directory_path = GetFileTestsDataDir();
  std::vector<std::string> paths;
  for (int i = 0; files[i] != nullptr; i++) {
    std::string file_path = directory_path + kSbFileSepChar + files[i];
    std::replace(file_path.begin(), file_path.end(), '/', kSbFileSepChar);
    paths.push_back(file_path);
  }
  return paths;
}

std::vector<std::string> GetFileTestsFilePaths() {
  const char* kFiles[] = {
    // This long file MUST be first -- SbFileSeekTest depends on it!
    "file_with_long_name_and_contents_for_seek_testing_1234567890",
    "file01",
    "dir_with_files/file11",
    "dir_with_files/file12",
    "dir_with_only_subdir/dir_with_files/file21",
    "dir_with_only_subdir/dir_with_files/file22",
    nullptr
  };
  return MakePathsVector(kFiles);
}

std::vector<std::string> GetFileTestsDirectoryPaths() {
  const char* kDirs[] = {
    "dir_with_files",
    "dir_with_only_subdir",
    "dir_with_only_subdir/dir_with_files",
    nullptr
  };
  return MakePathsVector(kDirs);
}

std::string GetTestFileExpectedContent(const std::string& path) {
  // The test file content matches the basename of the file + a newline.
  std::string content(path, path.find_last_of(kSbFileSepChar) + 1);
  content += '\n';
  return content;
}

// static
std::string ScopedRandomFile::MakeRandomFilename() {
  std::ostringstream filename_stream;
  filename_stream << "ScopedRandomFile.File_" << SbSystemGetRandomUInt64();
  return filename_stream.str();
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
std::string ScopedRandomFile::MakeRandomFilePath() {
  std::ostringstream filename_stream;
  filename_stream << GetTempDir();
  if (!filename_stream.tellp()) {
    return "";
  }

  filename_stream << kSbFileSepChar << MakeRandomFilename();
  return filename_stream.str();
}

std::string ScopedRandomFile::MakeRandomFile(int length) {
  std::string filename = MakeRandomFilePath();
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

  int bytes = SbFileWriteAll(file, data, length);
  EXPECT_EQ(bytes, length) << "Failed to write " << length << " bytes to "
                           << filename;

  bool result = SbFileClose(file);
  EXPECT_TRUE(result) << "Failed to close " << filename;
  delete[] data;
  return filename;
}

}  // namespace nplb
}  // namespace starboard
