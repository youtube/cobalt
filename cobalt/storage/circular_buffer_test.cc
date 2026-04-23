// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/storage/circular_buffer.h"

#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace storage {

class CircularBufferTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    test_file_path_ = temp_dir_.GetPath().Append("test_buffer.dat").value();
  }

  base::ScopedTempDir temp_dir_;
  std::string test_file_path_;
};

TEST_F(CircularBufferTest, InitTest) {
  CircularBuffer buffer(test_file_path_, 1024 * 1024);  // 1MB
  EXPECT_TRUE(buffer.Init());
}

TEST_F(CircularBufferTest, AppendTest) {
  CircularBuffer buffer(test_file_path_, 1024 * 1024);  // 1MB
  ASSERT_TRUE(buffer.Init());
  EXPECT_TRUE(buffer.Append("test_data"));
  std::string read_data;
  EXPECT_TRUE(buffer.Peek(&read_data));
  EXPECT_EQ(read_data, "test_data");
  EXPECT_TRUE(buffer.Pop());
}

TEST_F(CircularBufferTest, PersistenceTest) {
  {
    CircularBuffer buffer(test_file_path_, 1024 * 1024);
    ASSERT_TRUE(buffer.Init());
    EXPECT_TRUE(buffer.Append("persisted_data"));
  }  // buffer destroyed here

  // Re-open and check if it loads fine.
  CircularBuffer buffer(test_file_path_, 1024 * 1024);
  EXPECT_TRUE(buffer.Init());
  std::string read_data;
  EXPECT_TRUE(buffer.Peek(&read_data));
  EXPECT_EQ(read_data, "persisted_data");
  EXPECT_TRUE(buffer.Pop());
}

}  // namespace storage
}  // namespace cobalt
