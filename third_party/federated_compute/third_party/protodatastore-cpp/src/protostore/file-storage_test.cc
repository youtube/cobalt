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

#include "protostore/file-storage.h"

#include <cstdint>

#include "absl/strings/str_cat.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "protostore/testing-matchers.h"
#include "protostore/testfile-fixture.h"

namespace protostore {
namespace {

using ::testing::Eq;
using ::testing::StartsWith;
using testing::IsOk;
using testing::StatusIs;

class FileStorageTest : public testing::TestFileFixture {};

TEST_F(FileStorageTest, SmallWriteRead) {
  FileStorage storage;
  std::string testfile = TestFile("SmallWriteRead");
  auto out = storage.OpenForWrite(testfile);
  ASSERT_THAT(out, IsOk());
  ASSERT_OK((*out)->Append("small"));
  ASSERT_OK((*out)->Close());

  auto in = storage.OpenForRead(testfile);
  ASSERT_THAT(in, IsOk());
  char buffer[1024];
  absl::string_view result;
  ASSERT_OK((*in)->Read(5, &result, buffer));
  EXPECT_THAT(result, Eq("small"));
}

TEST_F(FileStorageTest, ReadALittleAtATime) {
  FileStorage storage;
  std::string testfile = TestFile("ReadALittleAtATime");
  auto out = storage.OpenForWrite(testfile);
  ASSERT_THAT(out, IsOk());
  ASSERT_OK((*out)->Append("fred did feed the three red fish"));
  ASSERT_OK((*out)->Close());

  auto in = storage.OpenForRead(testfile);
  ASSERT_THAT(in, IsOk());
  char buffer[1024];
  absl::string_view result;
  ASSERT_OK((*in)->Read(5, &result, buffer));
  EXPECT_THAT(result, Eq("fred "));
  ASSERT_OK((*in)->Read(10, &result, buffer));
  EXPECT_THAT(result, Eq("did feed t"));
  ASSERT_OK((*in)->Read(17, &result, buffer));
  EXPECT_THAT(result, Eq("he three red fish"));
}

TEST_F(FileStorageTest, ReadTooFar) {
  FileStorage storage;
  std::string testfile = TestFile("ReadTooFar");
  auto out = storage.OpenForWrite(testfile);
  ASSERT_THAT(out, IsOk());
  ASSERT_OK((*out)->Append("a"));
  ASSERT_OK((*out)->Close());

  auto in = storage.OpenForRead(testfile);
  ASSERT_THAT(in, IsOk());
  char buffer[1024];
  absl::string_view result;
  ASSERT_THAT((*in)->Read(10, &result, buffer),
    StatusIs(absl::StatusCode::kOutOfRange));
}

TEST_F(FileStorageTest, OutputAutoFlushes) {
  FileStorage storage;
  std::string testfile = TestFile("OutputAutoFlushes");
  {
    auto out = storage.OpenForWrite(testfile);
    ASSERT_THAT(out, IsOk());
    ASSERT_OK((*out)->Append("testing the flushing"));
    // Goes out of scope so flushes and closes.
  }

  auto in = storage.OpenForRead(testfile);
  ASSERT_THAT(in, IsOk());
  char buffer[1024];
  absl::string_view result;
  ASSERT_OK((*in)->Read(20, &result, buffer));
  EXPECT_THAT(result, Eq("testing the flushing"));
}

TEST_F(FileStorageTest, WritesNeedFlushing) {
  FileStorage storage;
  std::string testfile = TestFile("WritesNeedFlushing");
  auto out = storage.OpenForWrite(testfile);
  ASSERT_THAT(out, IsOk());
  ASSERT_OK((*out)->Append("testing the flushing"));
  // Not closed.

  auto in = storage.OpenForRead(testfile);
  ASSERT_THAT(in, IsOk());
  char buffer[1024];
  absl::string_view result;

  ASSERT_THAT((*in)->Read(20, &result, buffer),
    StatusIs(absl::StatusCode::kOutOfRange));

  ASSERT_OK((*out)->Close());

  // Try again.
  in = storage.OpenForRead(testfile);
  ASSERT_OK((*in)->Read(20, &result, buffer));
  EXPECT_THAT(result, Eq("testing the flushing"));
}

TEST_F(FileStorageTest, LargeWriteRead) {
  FileStorage storage;
  std::string testfile = TestFile("LargeWriteRead");
  auto out = storage.OpenForWrite(testfile);
  ASSERT_THAT(out, IsOk());
  for (int i = 0; i < 10000; i++) {
    ASSERT_OK((*out)->Append("LARGE"));
  }
  ASSERT_OK((*out)->Close());

  auto in = storage.OpenForRead(testfile);
  ASSERT_THAT(in, IsOk());
  char buffer[50000];
  absl::string_view result;
  ASSERT_OK((*in)->Read(50000, &result, buffer));
  EXPECT_THAT(std::string(result), StartsWith("LARGELARGELARGE"));
  EXPECT_THAT(result.size(), Eq(50000));
}

TEST_F(FileStorageTest, FileNotFound) {
  FileStorage storage;
  std::string testfile = TestFile("FileNotFound");

  auto in = storage.OpenForRead(testfile);
  ASSERT_THAT(in, StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(FileStorageTest, GetFileSizeNotFound) {
  FileStorage storage;
  std::string testfile = TestFile("GetFileSizeNotFound");

  auto size = storage.GetFileSize(testfile);
  ASSERT_THAT(size, StatusIs(absl::StatusCode::kNotFound));
}

TEST_F(FileStorageTest, GetFileSize) {
  FileStorage storage;
  std::string testfile = TestFile("GetFileSize");
  {
    auto out = storage.OpenForWrite(testfile);
    ASSERT_THAT(out, IsOk());
    ASSERT_OK((*out)->Append("0123456789"));
  }

  auto size = storage.GetFileSize(testfile);
  ASSERT_THAT(*size, Eq(10));
}

}  // namespace
}  // namespace protostore
