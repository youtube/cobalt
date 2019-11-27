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

#include <set>
#include <string>

#include "starboard/configuration.h"
#include "starboard/directory.h"
#include "starboard/file.h"
#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

typedef std::set<std::string> StringSet;

TEST(SbDirectoryGetNextTest, SunnyDay) {
  const int kNumFiles = 65;
  ScopedRandomFile files[kNumFiles];

  std::string directory_name = files[0].filename();
  directory_name.resize(directory_name.find_last_of(SB_FILE_SEP_CHAR));
  EXPECT_TRUE(SbFileExists(directory_name.c_str()))
      << "Directory_name is " << directory_name;

  SbFileError error = kSbFileErrorMax;
  SbDirectory directory = SbDirectoryOpen(directory_name.c_str(), &error);
  EXPECT_TRUE(SbDirectoryIsValid(directory));
  EXPECT_EQ(kSbFileOk, error);

  // Make sure all the files we created are found exactly once.
  StringSet names;
  for (int i = 0; i < SB_ARRAY_SIZE_INT(files); ++i) {
    names.insert(files[i].filename());
  }

  StringSet names_to_find(names);
  int count = 0;
  while (true) {
#if SB_API_VERSION >= SB_FEATURE_RUNTIME_CONFIGS_VERSION
    std::vector<char> entry(SB_FILE_MAX_NAME, 0);
    if (!SbDirectoryGetNext(directory, entry.data(), entry.size())) {
      break;
    }
    const char* entry_name = entry.data();
#else   // SB_API_VERSION >= SB_FEATURE_RUNTIME_CONFIGS_VERSION
    SbDirectoryEntry entry = {0};
    if (!SbDirectoryGetNext(directory, &entry)) {
      break;
    }
    const char* entry_name = entry.name;
#endif  // SB_API_VERSION >= SB_FEATURE_RUNTIME_CONFIGS_VERSION

    // SbDirectoryEntry just contains the last component of the absolute path to
    // the file, but ScopedRandomFile::filename() returns the full path.
    std::string filename;
    filename += directory_name;
    filename += SB_FILE_SEP_CHAR;
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

  EXPECT_TRUE(SbDirectoryClose(directory));
}

TEST(SbDirectoryGetNextTest, FailureInvalidSbDirectory) {
#if SB_API_VERSION >= SB_FEATURE_RUNTIME_CONFIGS_VERSION
  std::vector<char> entry(SB_FILE_MAX_NAME, 0);
  EXPECT_FALSE(SbDirectoryGetNext(kSbDirectoryInvalid, entry.data(),
                                  entry.size()));
#else  // SB_API_VERSION >= SB_FEATURE_RUNTIME_CONFIGS_VERSION
  SbDirectoryEntry entry = {0};
  EXPECT_FALSE(SbDirectoryGetNext(kSbDirectoryInvalid, &entry));
#endif  // SB_API_VERSION >= SB_FEATURE_RUNTIME_CONFIGS_VERSION
}

TEST(SbDirectoryGetNextTest, FailureNullEntry) {
  // Ensure there's at least one file in the directory.
  ScopedRandomFile file;

  std::string path = GetTempDir();
  EXPECT_FALSE(path.empty());
  EXPECT_TRUE(SbFileExists(path.c_str())) << "Directory is " << path;

  SbFileError error = kSbFileErrorMax;
  SbDirectory directory = SbDirectoryOpen(path.c_str(), &error);
  EXPECT_TRUE(SbDirectoryIsValid(directory));
  EXPECT_EQ(kSbFileOk, error);
#if SB_API_VERSION >= SB_FEATURE_RUNTIME_CONFIGS_VERSION
  EXPECT_FALSE(SbDirectoryGetNext(directory, NULL, SB_FILE_MAX_NAME));
#else   // SB_API_VERSION >= SB_FEATURE_RUNTIME_CONFIGS_VERSION
  EXPECT_FALSE(SbDirectoryGetNext(directory, NULL));
#endif  // SB_API_VERSION >= SB_FEATURE_RUNTIME_CONFIGS_VERSION
  EXPECT_TRUE(SbDirectoryClose(directory));
}

TEST(SbDirectoryGetNextTest, FailureInvalidAndNull) {
#if SB_API_VERSION >= SB_FEATURE_RUNTIME_CONFIGS_VERSION
  EXPECT_FALSE(SbDirectoryGetNext(kSbDirectoryInvalid, NULL, SB_FILE_MAX_NAME));
#else   // SB_API_VERSION >= SB_FEATURE_RUNTIME_CONFIGS_VERSION
  EXPECT_FALSE(SbDirectoryGetNext(kSbDirectoryInvalid, NULL));
#endif  // SB_API_VERSION >= SB_FEATURE_RUNTIME_CONFIGS_VERSION
}

TEST(SbDirectoryGetNextTest, FailureOnEmptyDirectory) {
  ScopedRandomFile dir(ScopedRandomFile::kDontCreate);
  const std::string& path = dir.filename();
  ASSERT_TRUE(SbDirectoryCreate(path.c_str()));
  SbFileError error = kSbFileErrorMax;
  SbDirectory directory = SbDirectoryOpen(path.c_str(), &error);
  ASSERT_TRUE(SbDirectoryIsValid(directory));
  ASSERT_EQ(kSbFileOk, error);

#if SB_API_VERSION >= SB_FEATURE_RUNTIME_CONFIGS_VERSION
  std::vector<char> entry(SB_FILE_MAX_NAME, 0);
  EXPECT_FALSE(SbDirectoryGetNext(directory, entry.data(), entry.size()));
#else   // SB_API_VERSION >= SB_FEATURE_RUNTIME_CONFIGS_VERSION
  SbDirectoryEntry entry = {0};
  EXPECT_FALSE(SbDirectoryGetNext(directory, &entry));
#endif  // SB_API_VERSION >= SB_FEATURE_RUNTIME_CONFIGS_VERSION
  ASSERT_TRUE(SbDirectoryClose(directory));
}

#if SB_API_VERSION >= SB_FEATURE_RUNTIME_CONFIGS_VERSION
TEST(SbDirectoryGetNextTest, FailureOnInsufficientSize) {
  ScopedRandomFile file;
  std::string directory_name = file.filename();
  directory_name.resize(directory_name.find_last_of(SB_FILE_SEP_CHAR));
  EXPECT_TRUE(SbFileExists(directory_name.c_str()))
      << "Directory_name is " << directory_name;

  SbFileError error = kSbFileErrorMax;
  SbDirectory directory = SbDirectoryOpen(directory_name.c_str(), &error);
  EXPECT_TRUE(SbDirectoryIsValid(directory));
  EXPECT_EQ(kSbFileOk, error);

  std::vector<char> entry(SB_FILE_MAX_NAME);
  for (int i = 0; i < SB_FILE_MAX_NAME; i++)
    entry[i] = i;
  std::vector<char> entry_copy = entry;
  EXPECT_EQ(SbDirectoryGetNext(directory, entry.data(), 0), false);
  EXPECT_EQ(entry.size(), SB_FILE_MAX_NAME);
  for (int i = 0; i < SB_FILE_MAX_NAME; i++)
    EXPECT_EQ(entry[i], entry_copy[i]);

  EXPECT_TRUE(SbDirectoryClose(directory));
}
#endif  // SB_API_VERSION >= SB_FEATURE_RUNTIME_CONFIGS_VERSION

}  // namespace
}  // namespace nplb
}  // namespace starboard
