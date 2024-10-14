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

#include <dirent.h>

#include <queue>
#include <set>
#include <string>

#include "starboard/common/log.h"
#include "starboard/configuration.h"
#include "starboard/configuration_constants.h"
#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

typedef std::set<std::string> StringSet;

bool FileExists(const char* path) {
  struct stat info;
  return stat(path, &info) == 0;
}

TEST(PosixDirectoryGetNextTest, SunnyDay) {
  const int kNumFiles = 65;
  ScopedRandomFile files[kNumFiles];

  std::string directory_name = files[0].filename();
  directory_name.resize(directory_name.find_last_of(kSbFileSepChar));
  EXPECT_TRUE(FileExists(directory_name.c_str()))
      << "Missing directory: " << directory_name;

  DIR* directory = opendir(directory_name.c_str());
  EXPECT_TRUE(directory);

  // Make sure all the files we created are found exactly once.
  StringSet names;
  for (int i = 0; i < SB_ARRAY_SIZE_INT(files); ++i) {
    names.insert(files[i].filename());
  }
  StringSet names_to_find(names);
  while (true) {
    std::vector<char> entry(kSbFileMaxName, 0);

    if (entry.size() < kSbFileMaxName || !directory || !entry.data()) {
      break;
    }

    struct dirent dirent_buffer;
    struct dirent* dirent;
    int result = readdir_r(directory, &dirent_buffer, &dirent);
    if (result || !dirent) {
      break;
    }

    starboard::strlcpy(entry.data(), dirent->d_name, entry.size());

    const char* entry_name = entry.data();

    // SbDirectoryEntry just contains the last component of the absolute path to
    // the file, but ScopedRandomFile::filename() returns the full path.
    std::string filename;
    filename += directory_name;
    filename += kSbFileSepChar;
    filename += entry_name;

    StringSet::iterator iterator = names_to_find.find(filename);
    if (iterator != names_to_find.end()) {
      names_to_find.erase(iterator);
    } else {
      // If it isn't in |names_to_find|, make sure it's some external entry and
      // not one of ours. Otherwise, an entry must have shown up twice.
      EXPECT_TRUE(names.find(entry_name) == names.end());
    }
  }

  // Make sure we found all of our names.
  EXPECT_EQ(0, names_to_find.size());

  EXPECT_TRUE(closedir(directory) == 0);
}

TEST(PosixDirectoryGetNextTest, SunnyDayStaticContent) {
  std::string testdata_dir = GetFileTestsDataDir();
  EXPECT_FALSE(testdata_dir.empty());
  EXPECT_TRUE(FileExists(testdata_dir.c_str()))
      << "Missing directory: " << testdata_dir;

  // Make sure all the test directories and files are found exactly once.
  StringSet paths_to_find;
  for (auto path : GetFileTestsDirectoryPaths()) {
    paths_to_find.insert(path);
  }
  for (auto path : GetFileTestsFilePaths()) {
    paths_to_find.insert(path);
  }

  // Breadth-first traversal of our test data.
  std::queue<std::string> directory_queue;
  directory_queue.push(testdata_dir);
  while (!directory_queue.empty()) {
    std::string path = directory_queue.front();
    directory_queue.pop();

    DIR* directory = opendir(path.c_str());
    EXPECT_TRUE(directory) << "Can't open: " << path;

    // Iterate all entries in this directory.
    while (true) {
      std::vector<char> entry(kSbFileMaxName, 0);
      if (entry.size() < kSbFileMaxName || !directory || !entry.data()) {
        break;
      }

      struct dirent dirent_buffer;
      struct dirent* dirent;
      int result = readdir_r(directory, &dirent_buffer, &dirent);
      if (result || !dirent) {
        break;
      }

      starboard::strlcpy(entry.data(), dirent->d_name, entry.size());
      std::string entry_name = entry.data();

      // Accept and ignore '.' and '..' directories.
      if (entry_name == "." || entry_name == "..") {
        continue;
      }

      // Absolute path of the entry.
      std::string entry_path = path + kSbFileSepChar + entry_name;

      StringSet::iterator iterator = paths_to_find.find(entry_path);
      if (iterator != paths_to_find.end()) {
        paths_to_find.erase(iterator);
      } else {
        ADD_FAILURE() << "Unexpected entry: " << entry_path;
      }

      // Traverse into the subdirectory.
      struct stat file_info;
      EXPECT_TRUE(stat(entry_path.c_str(), &file_info) == 0);
      if (S_ISDIR(file_info.st_mode)) {
        directory_queue.push(entry_path);
      }
    }

    EXPECT_TRUE(closedir(directory) == 0);
  }

  // Make sure we found all of test data directories and files.
  EXPECT_EQ(0, paths_to_find.size());
  for (auto it = paths_to_find.begin(); it != paths_to_find.end(); ++it) {
    ADD_FAILURE() << "Missing entry: " << *it;
  }
}

TEST(PosixDirectoryGetNextTest, FailureNullEntry) {
  // Ensure there's at least one file in the directory.
  ScopedRandomFile file;

  std::string path = GetTempDir();
  EXPECT_FALSE(path.empty());
  EXPECT_TRUE(FileExists(path.c_str())) << "Directory is " << path;

  DIR* directory = opendir(path.c_str());
  EXPECT_TRUE(directory);
  struct dirent dirent_buffer;
  struct dirent* dirent;
  int result = readdir_r(directory, &dirent_buffer, &dirent);
  EXPECT_FALSE(result || !dirent);
  EXPECT_TRUE(closedir(directory) == 0);
}

TEST(PosixDirectoryGetNextTest, FailureOnInsufficientSize) {
  ScopedRandomFile file;
  std::string directory_name = file.filename();
  directory_name.resize(directory_name.find_last_of(kSbFileSepChar));
  EXPECT_TRUE(FileExists(directory_name.c_str()))
      << "Directory_name is " << directory_name;

  DIR* directory = opendir(directory_name.c_str());
  EXPECT_TRUE(directory);
  std::vector<char> entry(kSbFileMaxName);
  for (int i = 0; i < kSbFileMaxName; i++) {
    entry[i] = i;
  }
  std::vector<char> entry_copy = entry;

  struct dirent dirent_buffer;
  struct dirent* dirent;

  int result = readdir_r(directory, &dirent_buffer, &dirent);

  EXPECT_TRUE(!result && dirent);
  starboard::strlcpy(entry.data(), dirent->d_name, 0);
  EXPECT_EQ(entry.size(), kSbFileMaxName);
  for (int i = 0; i < kSbFileMaxName; i++) {
    EXPECT_EQ(entry[i], entry_copy[i]);
  }

  EXPECT_TRUE(closedir(directory) == 0);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
