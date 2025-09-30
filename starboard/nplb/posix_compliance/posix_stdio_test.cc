// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string>

#include "starboard/file.h"
#include "starboard/nplb/file_helpers.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {

TEST(PosixStdioTest, FopenFclose) {
  ScopedRandomFile random_file;
  std::string filename = random_file.filename();

  FILE* file = fopen(filename.c_str(), "w");
  ASSERT_NE(file, nullptr) << "Failed to open file for writing: " << filename;

  int result = fclose(file);
  ASSERT_EQ(result, 0) << "Failed to close file: " << filename;

  // Try opening for reading
  file = fopen(filename.c_str(), "r");
  ASSERT_NE(file, nullptr) << "Failed to open file for reading: " << filename;

  result = fclose(file);
  ASSERT_EQ(result, 0) << "Failed to close file: " << filename;
}

TEST(PosixStdioTest, FopenInvalidPath) {
  std::string invalid_path = "invalid/path/to/file.txt";
  FILE* file = fopen(invalid_path.c_str(), "r");
  ASSERT_EQ(file, nullptr);
  // Expect errno to be set to ENOENT (No such file or directory)
  ASSERT_EQ(errno, ENOENT);
}

TEST(PosixStdioTest, FopenPermissions) {
  ScopedRandomFile random_file;
  std::string filename = random_file.filename();

  // Create a file with read-only permissions
  FILE* file = fopen(filename.c_str(), "w");
  ASSERT_NE(file, nullptr);
  fclose(file);

  // Change permissions to read-only
  ASSERT_EQ(chmod(filename.c_str(), S_IRUSR), 0);

  // Try to open for writing (should fail)
  file = fopen(filename.c_str(), "w");
  ASSERT_EQ(file, nullptr);
  ASSERT_EQ(errno, EACCES);  // Permission denied

  // Try to open for reading (should succeed)
  file = fopen(filename.c_str(), "r");
  ASSERT_NE(file, nullptr);
  fclose(file);
}

}  // namespace nplb
