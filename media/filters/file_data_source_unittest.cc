// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/base_paths.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "media/base/mock_callback.h"
#include "media/base/mock_filter_host.h"
#include "media/base/mock_filters.h"
#include "media/filters/file_data_source.h"

using ::testing::NiceMock;
using ::testing::StrictMock;

namespace media {

class ReadCallbackHandler {
 public:
  ReadCallbackHandler() {}

  MOCK_METHOD1(ReadCallback, void(size_t size));

 private:
  DISALLOW_COPY_AND_ASSIGN(ReadCallbackHandler);
};

// Returns a path to the test file which contains the string "0123456789"
// without the quotes or any trailing space or null termination.  The file lives
// under the media/test/data directory.  Under Windows, strings for the
// FilePath class are unicode, and the pipeline wants char strings.  Convert
// the string to UTF8 under Windows.  For Mac and Linux, file paths are already
// chars so just return the string from the FilePath.
std::string TestFileURL() {
  FilePath data_dir;
  EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &data_dir));
  data_dir = data_dir.Append(FILE_PATH_LITERAL("media"))
                     .Append(FILE_PATH_LITERAL("test"))
                     .Append(FILE_PATH_LITERAL("data"))
                     .Append(FILE_PATH_LITERAL("ten_byte_file"));
#if defined (OS_WIN)
  return WideToUTF8(data_dir.value());
#else
  return data_dir.value();
#endif
}

// Test that FileDataSource call the appropriate methods on its filter host.
TEST(FileDataSourceTest, OpenFile) {
  StrictMock<MockFilterHost> host;
  EXPECT_CALL(host, SetTotalBytes(10));
  EXPECT_CALL(host, SetBufferedBytes(10));

  scoped_refptr<FileDataSource> filter(new FileDataSource());
  filter->set_host(&host);
  filter->Initialize(TestFileURL(), NewExpectedCallback());

  filter->Stop(NewExpectedCallback());
}

// Use the mock filter host to directly call the Read and GetPosition methods.
TEST(FileDataSourceTest, ReadData) {
  int64 size;
  uint8 ten_bytes[10];

  // Create our mock filter host and initialize the data source.
  NiceMock<MockFilterHost> host;
  scoped_refptr<FileDataSource> filter(new FileDataSource());

  filter->set_host(&host);
  filter->Initialize(TestFileURL(), NewExpectedCallback());

  EXPECT_TRUE(filter->GetSize(&size));
  EXPECT_EQ(10, size);

  ReadCallbackHandler handler;
  EXPECT_CALL(handler, ReadCallback(10));
  filter->Read(0, 10, ten_bytes,
               NewCallback(&handler, &ReadCallbackHandler::ReadCallback));
  EXPECT_EQ('0', ten_bytes[0]);
  EXPECT_EQ('5', ten_bytes[5]);
  EXPECT_EQ('9', ten_bytes[9]);

  EXPECT_CALL(handler, ReadCallback(0));
  filter->Read(10, 10, ten_bytes,
               NewCallback(&handler, &ReadCallbackHandler::ReadCallback));

  EXPECT_CALL(handler, ReadCallback(5));
  filter->Read(5, 10, ten_bytes,
               NewCallback(&handler, &ReadCallbackHandler::ReadCallback));
  EXPECT_EQ('5', ten_bytes[0]);

  filter->Stop(NewExpectedCallback());
}

// Test that FileDataSource does nothing on Seek().
TEST(FileDataSourceTest, Seek) {
  const base::TimeDelta kZero;

  scoped_refptr<FileDataSource> filter(new FileDataSource());
  filter->Seek(kZero, NewExpectedCallback());

  filter->Stop(NewExpectedCallback());
}

}  // namespace media
