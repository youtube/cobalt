// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "media/base/mock_data_source_host.h"
#include "media/base/test_helpers.h"
#include "media/filters/file_data_source.h"

using ::testing::NiceMock;
using ::testing::StrictMock;

namespace media {

class ReadCBHandler {
 public:
  ReadCBHandler() {}

  MOCK_METHOD1(ReadCB, void(int size));

 private:
  DISALLOW_COPY_AND_ASSIGN(ReadCBHandler);
};

// Returns a path to the test file which contains the string "0123456789"
// without the quotes or any trailing space or null termination.  The file lives
// under the media/test/data directory.  Under Windows, strings for the
// FilePath class are unicode, and the pipeline wants char strings.  Convert
// the string to UTF8 under Windows.  For Mac and Linux, file paths are already
// chars so just return the string from the FilePath.
FilePath TestFileURL() {
  FilePath data_dir;
  EXPECT_TRUE(PathService::Get(base::DIR_TEST_DATA, &data_dir));
  data_dir = data_dir.Append(FILE_PATH_LITERAL("media"))
                     .Append(FILE_PATH_LITERAL("test"))
                     .Append(FILE_PATH_LITERAL("data"))
                     .Append(FILE_PATH_LITERAL("ten_byte_file"));
  return data_dir;
}

// Test that FileDataSource call the appropriate methods on its filter host.
TEST(FileDataSourceTest, OpenFile) {
  StrictMock<MockDataSourceHost> host;
  EXPECT_CALL(host, SetTotalBytes(10));
  EXPECT_CALL(host, AddBufferedByteRange(0, 10));

  scoped_refptr<FileDataSource> filter(new FileDataSource());
  filter->set_host(&host);
  EXPECT_TRUE(filter->Initialize(TestFileURL()));

  filter->Stop(NewExpectedClosure());
}

// Use the mock filter host to directly call the Read and GetPosition methods.
TEST(FileDataSourceTest, ReadData) {
  int64 size;
  uint8 ten_bytes[10];

  // Create our mock filter host and initialize the data source.
  NiceMock<MockDataSourceHost> host;
  scoped_refptr<FileDataSource> filter(new FileDataSource());

  filter->set_host(&host);
  EXPECT_TRUE(filter->Initialize(TestFileURL()));

  EXPECT_TRUE(filter->GetSize(&size));
  EXPECT_EQ(10, size);

  ReadCBHandler handler;
  EXPECT_CALL(handler, ReadCB(10));
  filter->Read(0, 10, ten_bytes, base::Bind(
      &ReadCBHandler::ReadCB, base::Unretained(&handler)));
  EXPECT_EQ('0', ten_bytes[0]);
  EXPECT_EQ('5', ten_bytes[5]);
  EXPECT_EQ('9', ten_bytes[9]);

  EXPECT_CALL(handler, ReadCB(1));
  filter->Read(9, 1, ten_bytes, base::Bind(
      &ReadCBHandler::ReadCB, base::Unretained(&handler)));
  EXPECT_EQ('9', ten_bytes[0]);

  EXPECT_CALL(handler, ReadCB(0));
  filter->Read(10, 10, ten_bytes, base::Bind(
      &ReadCBHandler::ReadCB, base::Unretained(&handler)));

  EXPECT_CALL(handler, ReadCB(5));
  filter->Read(5, 10, ten_bytes, base::Bind(
      &ReadCBHandler::ReadCB, base::Unretained(&handler)));
  EXPECT_EQ('5', ten_bytes[0]);

  filter->Stop(NewExpectedClosure());
}

}  // namespace media
