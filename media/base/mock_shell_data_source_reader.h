/*
 * Copyright 2012 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef MEDIA_BASE_MOCK_SHELL_DATA_SOURCE_READER_H_
#define MEDIA_BASE_MOCK_SHELL_DATA_SOURCE_READER_H_

#include "media/base/shell_data_source_reader.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MockShellDataSourceReader : public ShellDataSourceReader {
 public:
  MockShellDataSourceReader() {}

  // ShellDataSourceReader implementation
  MOCK_METHOD1(SetDataSource, void(scoped_refptr<DataSource>));
  MOCK_METHOD1(SetErrorCallback, void(base::Closure));
  MOCK_METHOD3(BlockingRead, int(int64, int, uint8*));
  MOCK_METHOD0(FileSize, int64());
  MOCK_METHOD0(AbortPendingReadIfAny, void());
};

}  // namespace media

#endif  // MEDIA_BASE_MOCK_SHELL_DATA_SOURCE_READER_H_
