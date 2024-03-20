// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/media/file_data_source.h"

#include <string.h>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/test/task_environment.h"
#include "starboard/common/log.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace media {

void OnReadFinished(int* bytes_read_out, int bytes_read_in) {
  SB_CHECK(bytes_read_out);
  *bytes_read_out = bytes_read_in;
}

TEST(FileDataSourceTest, SunnyDay) {
  // TODO(b/292134341): Test fails on all platforms.
  GTEST_SKIP();

  base::test::TaskEnvironment scoped_task_environment_;
  FileDataSource data_source(
      GURL("file:///cobalt/media/testing/data/"
           "progressive_aac_44100_stereo_h264_1280_720.mp4"));

  int64 file_size = -1;
  ASSERT_TRUE(data_source.GetSize(&file_size));
  ASSERT_GT(file_size, 0);

  uint8 buffer[1024];
  int bytes_read = 0;

  // Read from the beginning
  data_source.Read(0, 1024, buffer, base::Bind(OnReadFinished, &bytes_read));
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(bytes_read, 1024);
  EXPECT_EQ(memcmp(buffer + 4, "ftyp", 4), 0);

  // Read from the end
  data_source.Read(file_size - 1024, 1024, buffer,
                   base::Bind(OnReadFinished, &bytes_read));
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(bytes_read, 1024);

  // Read beyond the end
  data_source.Read(file_size - 512, 1024, buffer,
                   base::Bind(OnReadFinished, &bytes_read));
  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(bytes_read, FileDataSource::kReadError);
}

TEST(FileDataSourceTest, RainyDay) {
  base::test::TaskEnvironment scoped_task_environment_;
  FileDataSource data_source(
      GURL("file:///cobalt/media/testing/data/do_not_exist.invalid"));

  int64 size = -1;
  ASSERT_FALSE(data_source.GetSize(&size));

  uint8 buffer[1024];
  int bytes_read = 0;
  data_source.Read(0, 1024, buffer, base::Bind(OnReadFinished, &bytes_read));

  scoped_task_environment_.RunUntilIdle();
  EXPECT_EQ(bytes_read, FileDataSource::kReadError);
}

}  // namespace media
}  // namespace cobalt
