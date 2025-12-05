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

#include <dirent.h>
#include <fcntl.h>

#include <algorithm>
#include <string>

#include "starboard/configuration_constants.h"
#include "starboard/nplb/file_helpers.h"
#include "starboard/system.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

/*
Scenarios that aren't tested:
1. Invalid directory pointer (ie, readdir(invalid_pointer)) as this behavior
crashes with a seg fault.
TODO: b/419539055 - Test seg fault scenarios.
2. EOVERFLOW error: This error is hard to reliably test as it depends on
filesystem characteristics and internal structure of dirent. It's usually
handled by the underlying filesystem implementation.
3. Concurrent calls on the same directory from different threads, readdir is
not necessarily threadsafe, so this behavior shouldn't be tested.
*/

class PosixReaddirTests : public ::testing::Test {
 protected:
  void SetUp() override {
    std::vector<char> temp_path(kSbFileMaxPath);
    bool system_path_success = SbSystemGetPath(
        kSbSystemPathTempDirectory, temp_path.data(), kSbFileMaxPath);
    ASSERT_TRUE(system_path_success);
    struct stat info;
    test_dir_ = std::string(temp_path.data()) + "/posix_readdir_tests";
    EXPECT_TRUE(stat(temp_path.data(), &info) == 0 && S_ISDIR(info.st_mode));
    EXPECT_TRUE(mkdir(test_dir_.c_str(), 0700) == 0 ||
                stat(test_dir_.c_str(), &info) == 0 && S_ISDIR(info.st_mode));
  }

  void TearDown() override { RemoveFileOrDirectoryRecursively(test_dir_); }

  std::string test_dir_;
  int flags_ = O_CREAT | O_WRONLY | O_TRUNC;
};

// Test that d_ino is populated correctly (non-zero)
TEST_F(PosixReaddirTests, DInoPopulated) {
  // Create a file
  std::string file_path = test_dir_ + "/inode_test_file.txt";
  int fd = open(file_path.c_str(), flags_, 0666);
  ASSERT_NE(fd, -1) << "Failed to create test file.";
  close(fd);

  DIR* dir = opendir(test_dir_.c_str());
  ASSERT_NE(dir, nullptr) << "Failed to open directory.";

  struct dirent* entry;
  bool found_file = false;
  while ((entry = readdir(dir)) != nullptr) {
    if (std::string(entry->d_name) == "inode_test_file.txt") {
      ASSERT_NE(entry->d_ino, 0u) << "d_ino for file is zero.";
      found_file = true;
    } else if (std::string(entry->d_name) == ".") {
      ASSERT_NE(entry->d_ino, 0u) << "d_ino for '.' is zero.";
    } else if (std::string(entry->d_name) == "..") {
      ASSERT_NE(entry->d_ino, 0u) << "d_ino for '..' is zero.";
    }
  }
  ASSERT_EQ(closedir(dir), 0) << "Failed to close directory.";
  ASSERT_TRUE(found_file) << "Did not find the created file in readdir.";
}

// Test that dType is populated correctly, checking for types DT_REG (file) and
// DT_DIR (directory) which are universally supported.
TEST_F(PosixReaddirTests, DTypePopulatedRegularFileAndDirectory) {
  // Create a regular file
  std::string regular_file_path = test_dir_ + "/regular_file.txt";
  int fd = open(regular_file_path.c_str(), flags_, 0666);
  ASSERT_NE(fd, -1) << "Failed to create regular file.";
  close(fd);

  // Create a subdirectory
  std::string sub_directory_path = test_dir_ + "/sub_directory";
  ASSERT_EQ(mkdir(sub_directory_path.c_str(), 0777), 0)
      << "Failed to create subdirectory.";

  DIR* dir = opendir(test_dir_.c_str());
  ASSERT_NE(dir, nullptr) << "Failed to open directory.";

  bool found_regular_file = false;
  bool found_sub_directory = false;
  bool found_dot = false;
  bool found_dot_dot = false;

  struct dirent* entry;
  while ((entry = readdir(dir)) != nullptr) {
    std::string name = entry->d_name;
    if (name == "regular_file.txt") {
      // d_type for regular files should be DT_REG or DT_UNKNOWN
      ASSERT_TRUE(entry->d_type == DT_REG || entry->d_type == DT_UNKNOWN)
          << "Incorrect d_type for regular file '" << name
          << "': " << static_cast<int>(entry->d_type);
      found_regular_file = true;
    } else if (name == "sub_directory") {
      // d_type for directories should be DT_DIR or DT_UNKNOWN
      ASSERT_TRUE(entry->d_type == DT_DIR || entry->d_type == DT_UNKNOWN)
          << "Incorrect d_type for subdirectory '" << name
          << "': " << static_cast<int>(entry->d_type);
      found_sub_directory = true;
    } else if (name == ".") {
      ASSERT_TRUE(entry->d_type == DT_DIR || entry->d_type == DT_UNKNOWN)
          << "Incorrect d_type for '.': " << static_cast<int>(entry->d_type);
      found_dot = true;
    } else if (name == "..") {
      ASSERT_TRUE(entry->d_type == DT_DIR || entry->d_type == DT_UNKNOWN)
          << "Incorrect d_type for '..': " << static_cast<int>(entry->d_type);
      found_dot_dot = true;
    }
  }

  ASSERT_EQ(closedir(dir), 0) << "Failed to close directory.";

  ASSERT_TRUE(found_regular_file) << "Did not find the expected regular file.";
  ASSERT_TRUE(found_sub_directory) << "Did not find the expected subdirectory.";
  ASSERT_TRUE(found_dot) << "Did not find '.' entry.";
  ASSERT_TRUE(found_dot_dot) << "Did not find '..' entry.";
}

// Test readdir on a directory opened with opendir
TEST_F(PosixReaddirTests, ReaddirOnValidDIRPointer) {
  DIR* dir = opendir(test_dir_.c_str());
  ASSERT_NE(dir, nullptr) << "Failed to open directory.";

  struct dirent* entry = readdir(dir);
  ASSERT_NE(entry, nullptr)
      << "readdir returned nullptr for a non-empty directory.";
  // We expect to get at least "." or ".."
  ASSERT_TRUE(std::string(entry->d_name) == "." ||
              std::string(entry->d_name) == "..")
      << "First entry was not '.' or '..'";

  ASSERT_EQ(closedir(dir), 0) << "Failed to close directory.";
}

// Test readdir returning nullptr at end of directory stream
TEST_F(PosixReaddirTests, ReaddirReturnsNullAtEndOfStream) {
  DIR* dir = opendir(test_dir_.c_str());
  ASSERT_NE(dir, nullptr) << "Failed to open directory.";

  // Read all entries until readdir returns nullptr
  int count = 0;
  while (readdir(dir) != nullptr) {
    count++;
  }
  // An empty directory should have at least "." and ".."
  ASSERT_GE(count, 2) << "Did not read at least '.' and '..'";

  // The next call should return nullptr
  ASSERT_EQ(readdir(dir), nullptr)
      << "readdir did not return nullptr at the end of the stream.";
  ASSERT_EQ(closedir(dir), 0) << "Failed to close directory.";
}

// Test readdir with a large number of entries to check for buffer handling
TEST_F(PosixReaddirTests, LargeNumberOfEntries) {
  const int kNumFiles = 1000;  // A reasonably large number of files
  for (int i = 0; i < kNumFiles; ++i) {
    std::string file_path = test_dir_ + "/file_" + std::to_string(i);
    int fd = open(file_path.c_str(), flags_, 0666);
    ASSERT_NE(fd, -1) << "Failed to create file " << i;
    close(fd);
  }

  DIR* dir = opendir(test_dir_.c_str());
  ASSERT_NE(dir, nullptr) << "Failed to open directory.";

  int count = 0;
  struct dirent* entry;
  while ((entry = readdir(dir)) != nullptr) {
    std::string name = entry->d_name;
    if (name != "." && name != "..") {
      count++;
    }
  }
  ASSERT_EQ(closedir(dir), 0) << "Failed to close directory.";

  ASSERT_EQ(count, kNumFiles) << "Did not read the expected number of files.";
}

// Test readdir on two separate directories and verify distinct entries.
TEST_F(PosixReaddirTests, SeparateDirectoriesSeparateReaddir) {
  // Create first directory
  std::string dir1_path = test_dir_ + "_dir1";
  ASSERT_EQ(mkdir(dir1_path.c_str(), 0777), 0) << "Failed to create dir1.";
  std::string file1_path = dir1_path + "/file_in_dir1.txt";
  int fd1 = open(file1_path.c_str(), flags_, 0666);
  ASSERT_NE(fd1, -1) << "Failed to create file in dir1.";
  close(fd1);

  // Create second directory
  std::string dir2_path = test_dir_ + "_dir2";
  ASSERT_EQ(mkdir(dir2_path.c_str(), 0777), 0) << "Failed to create dir2.";
  std::string file2_path = dir2_path + "/file_in_dir2.txt";
  int fd2 = open(file2_path.c_str(), flags_, 0666);
  ASSERT_NE(fd2, -1) << "Failed to create file in dir2.";
  close(fd2);

  DIR* dir1 = opendir(dir1_path.c_str());
  ASSERT_NE(dir1, nullptr) << "Failed to open dir1.";

  DIR* dir2 = opendir(dir2_path.c_str());
  ASSERT_NE(dir2, nullptr) << "Failed to open dir2.";

  struct dirent* entry1;
  struct dirent* entry2;

  std::vector<std::string> entries_dir1;
  while ((entry1 = readdir(dir1)) != nullptr) {
    entries_dir1.push_back(entry1->d_name);
  }

  std::vector<std::string> entries_dir2;
  while ((entry2 = readdir(dir2)) != nullptr) {
    entries_dir2.push_back(entry2->d_name);
  }

  ASSERT_EQ(closedir(dir1), 0) << "Failed to close dir1.";
  ASSERT_EQ(closedir(dir2), 0) << "Failed to close dir2.";

  // Sort and compare contents. We expect the common "." and ".." and their
  // distinct files.
  std::sort(entries_dir1.begin(), entries_dir1.end());
  std::sort(entries_dir2.begin(), entries_dir2.end());

  std::vector<std::string> expected_dir1_entries = {".", "..",
                                                    "file_in_dir1.txt"};
  std::vector<std::string> expected_dir2_entries = {".", "..",
                                                    "file_in_dir2.txt"};

  std::sort(expected_dir1_entries.begin(), expected_dir1_entries.end());
  std::sort(expected_dir2_entries.begin(), expected_dir2_entries.end());

  ASSERT_EQ(entries_dir1, expected_dir1_entries)
      << "Contents of dir1 do not match expected.";
  ASSERT_EQ(entries_dir2, expected_dir2_entries)
      << "Contents of dir2 do not match expected.";

  // Clean up the additional directories
  RemoveFileOrDirectoryRecursively(dir1_path);
  RemoveFileOrDirectoryRecursively(dir2_path);
}

}  // namespace
}  // namespace nplb
