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
  EXPECT_TRUE(SbFileExists(directory_name.c_str()));

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
    SbDirectoryEntry entry = {0};
    if (!SbDirectoryGetNext(directory, &entry)) {
      break;
    }

    // SbDirectoryEntry just contains the last component of the absolute path to
    // the file, but ScopedRandomFile::filename() returns the full path.
    std::string filename;
    filename += directory_name;
    filename += SB_FILE_SEP_CHAR;
    filename += entry.name;

    StringSet::iterator iterator = names_to_find.find(filename);
    if (iterator != names_to_find.end()) {
      names_to_find.erase(iterator);
    } else {
      // If it isn't in |names_to_find|, make sure it's some external entry and
      // not one of ours. Otherwise, an entry must have shown up twice.
      EXPECT_TRUE(names.find(entry.name) == names.end());
    }
  }

  // Make sure we found all of our names.
  EXPECT_EQ(0, names_to_find.size());

  EXPECT_TRUE(SbDirectoryClose(directory));
}

TEST(SbDirectoryGetNextTest, FailureInvalidSbDirectory) {
  SbDirectoryEntry entry = {0};
  EXPECT_FALSE(SbDirectoryGetNext(kSbDirectoryInvalid, &entry));
}

TEST(SbDirectoryGetNextTest, FailureNullEntry) {
  // Ensure there's at least one file in the directory.
  ScopedRandomFile file;

  std::string path = GetTempDir();
  EXPECT_FALSE(path.empty());
  EXPECT_TRUE(SbFileExists(path.c_str()));

  SbFileError error = kSbFileErrorMax;
  SbDirectory directory = SbDirectoryOpen(path.c_str(), &error);
  EXPECT_TRUE(SbDirectoryIsValid(directory));
  EXPECT_EQ(kSbFileOk, error);

  EXPECT_FALSE(SbDirectoryGetNext(directory, NULL));

  EXPECT_TRUE(SbDirectoryClose(directory));
}

TEST(SbDirectoryGetNextTest, FailureInvalidAndNull) {
  EXPECT_FALSE(SbDirectoryGetNext(kSbDirectoryInvalid, NULL));
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
