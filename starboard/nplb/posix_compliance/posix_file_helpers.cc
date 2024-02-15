// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/nplb/posix_compliance/posix_file_helpers.h"

#include <fcntl.h>
#include <unistd.h>

#include <sstream>
#include <string>
#include <vector>

#include "starboard/configuration_constants.h"
#include "starboard/file.h"
#include "starboard/shared/posix/file_internal.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {

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

  int file = open(filename.c_str(), O_CREAT | O_WRONLY);
  EXPECT_TRUE(fcntl(file, F_GETFD));
  if (!fcntl(file, F_GETFD)) {
    return "";
  }

  char* data = new char[length];
  for (int i = 0; i < length; ++i) {
    data[i] = static_cast<char>(i & 0xFF);
  }

  bool result = close(file);
  EXPECT_TRUE(result) << "Failed to close " << filename;
  delete[] data;
  return filename;
}

}  // namespace nplb
}  // namespace starboard
