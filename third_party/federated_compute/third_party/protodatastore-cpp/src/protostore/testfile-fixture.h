// Copyright (C) 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PROTOSTORE_TESTFILE_FIXTURE_H_
#define PROTOSTORE_TESTFILE_FIXTURE_H_

#include "protostore/file-storage.h"

#include <cstdint>
#include <dirent.h>

#include "absl/strings/str_cat.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace protostore {
namespace testing {

using ::testing::Ne;
using ::testing::NotNull;

class TestFileFixture : public ::testing::Test {
 public:
  void SetUp() override {
#ifdef __ANDROID__
    char tmpl[] = "/data/local/tmp/file-storage.XXXXXX";
#else
    char tmpl[] = "/tmp/file-storage.XXXXXX";
#endif
    char* tmpdir = mkdtemp(tmpl);
    ASSERT_THAT(tmpdir, NotNull()) << tmpl << ": " << strerror(errno);
    testdir_ = tmpdir;
  }

  void TearDown() override {
    DIR* dir = opendir(testdir_.c_str());
    ASSERT_THAT(dir, NotNull()) << testdir_ << ": " << strerror(errno);
    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr) {
      std::string filename = absl::StrCat(testdir_, "/", entry->d_name);
      unlink(filename.c_str());
    }
    ASSERT_THAT(rmdir(testdir_.c_str()), Ne(-1)) << strerror(errno);
    ASSERT_THAT(closedir(dir), Ne(-1)) << strerror(errno);
  }

  std::string TestFile(absl::string_view name) {
    return absl::StrCat(testdir_, "/", name);
  }

 private:
  std::string testdir_;
};

}  // namespace testing
}  // namespace protostore

#endif  // PROTOSTORE_TESTFILE_FIXTURE_H_
