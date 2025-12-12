// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#include <fcntl.h>
#include <unistd.h>

#include "starboard/common/string.h"
#include "starboard/nplb/file_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

class FileTempTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {
    // Clean up any files created during the tests.
    for (const std::string& filename : created_files_) {
      unlink(filename.c_str());
    }
    created_files_.clear();
  }

  void AddCreatedFile(const std::string& filename) {
    created_files_.push_back(filename);
  }

 private:
  std::vector<std::string> created_files_;  // Keep track of files to delete
};

TEST_F(FileTempTest, MkdtempBasicSuccess) {
  const std::string tmpl = GetTempDir() + "my_temp_dir.XXXXXX";
  char buffer[tmpl.length() + 1];
  strcpy(buffer, tmpl.c_str());

  char* const result = mkdtemp(buffer);
  ASSERT_THAT(result, ::testing::NotNull())
      << "mkdtemp failed for template '" << tmpl << "': " << strerror(errno);
}

TEST_F(FileTempTest, MkdtempInvalidTemplate) {
  const std::string tmpl = GetTempDir() + "invalid_template";
  char buffer[tmpl.length() + 1];
  strcpy(buffer, tmpl.c_str());

  char* const result = mkdtemp(buffer);
  ASSERT_THAT(result, ::testing::IsNull())
      << "mkdtemp should fail for invalid template, but returned '"
      << (result ? result : "nullptr") << "'.";
  EXPECT_EQ(errno, EINVAL) << "mkdtemp expected errno EINVAL (" << EINVAL
                           << ") for invalid template, but got " << errno
                           << " (" << strerror(errno) << ").";
}

TEST_F(FileTempTest, MkdtempNonExistentPath) {
  const std::string tmpl = "/nonexistent/path/temp.XXXXXX";
  char buffer[tmpl.length() + 1];
  strcpy(buffer, tmpl.c_str());

  char* const result = mkdtemp(buffer);
  ASSERT_THAT(result, ::testing::IsNull())
      << "mkdtemp should fail for non-existent path, but returned '"
      << (result ? result : "nullptr") << "'.";
  EXPECT_EQ(errno, ENOENT) << "mkdtemp expected errno ENOENT (" << ENOENT
                           << ") for non-existent path, but got " << errno
                           << " (" << strerror(errno) << ").";
}

TEST_F(FileTempTest, MkstempBasicSuccess) {
  const std::string tmpl = GetTempDir() + "test_file.XXXXXX";
  char buffer[tmpl.size() + 1];
  strcpy(buffer, tmpl.c_str());

  const int fd = mkstemp(buffer);
  ASSERT_NE(fd, -1) << "mkstemp failed: " << strerror(errno);

  EXPECT_EQ(close(fd), 0) << "close failed: " << strerror(errno);
  AddCreatedFile(buffer);  // Add to cleanup list
}

TEST_F(FileTempTest, MkstempInvalidTemplate) {
  const std::string tmpl = GetTempDir() + "invalid_template";
  char buffer[tmpl.size() + 1];
  strcpy(buffer, tmpl.c_str());

  const int fd = mkstemp(buffer);
  EXPECT_EQ(fd, -1)
      << "mkstemp should fail for invalid template, but returned fd " << fd
      << ".";
  EXPECT_EQ(errno, EINVAL) << "mkstemp expected errno EINVAL (" << EINVAL
                           << ") for invalid template, but got " << errno
                           << " (" << strerror(errno) << ").";
}

TEST_F(FileTempTest, MkstempNonExistentPath) {
  const std::string tmpl = "/nonexistent/path/test_file.XXXXXX";
  char buffer[tmpl.size() + 1];
  strcpy(buffer, tmpl.c_str());

  const int fd = mkstemp(buffer);
  EXPECT_EQ(fd, -1)
      << "mkstemp should fail for non-existent path, but returned fd " << fd
      << ".";
  EXPECT_EQ(errno, ENOENT) << "mkstemp expected errno ENOENT (" << ENOENT
                           << ") for non-existent path, but got " << errno
                           << " (" << strerror(errno) << ").";
}

TEST_F(FileTempTest, MkstempFileIsOpened) {
  const std::string tmpl = GetTempDir() + "open_file_test.XXXXXX";
  char buffer[tmpl.size() + 1];
  strcpy(buffer, tmpl.c_str());

  const int fd = mkstemp(buffer);
  ASSERT_NE(fd, -1) << "mkstemp failed: " << strerror(errno);

  // Try to write something to the file descriptor.
  const char* test_data = "test data";
  const ssize_t bytes_written = write(fd, test_data, strlen(test_data));
  EXPECT_EQ(static_cast<size_t>(bytes_written), strlen(test_data))
      << "write failed or wrote incorrect bytes for fd " << fd
      << " with error: " << strerror(errno);

  EXPECT_EQ(close(fd), 0) << "close failed: " << strerror(errno);
  AddCreatedFile(buffer);
}

TEST_F(FileTempTest, MkostempBasicSuccess) {
  const std::string tmpl = GetTempDir() + "test_ostemp_file.XXXXXX";
  char buffer[tmpl.size() + 1];
  strcpy(buffer, tmpl.c_str());

  const int fd = mkostemp(buffer, O_RDWR);
  ASSERT_NE(fd, -1) << "mkostemp failed: " << strerror(errno);

  EXPECT_EQ(close(fd), 0) << "close failed: " << strerror(errno);
  AddCreatedFile(buffer);

  struct stat st{};
  const int stat_result = stat(buffer, &st);
  ASSERT_EQ(stat_result, 0) << "stat failed: " << strerror(errno);
  ASSERT_TRUE(S_ISREG(st.st_mode));
}

TEST_F(FileTempTest, MkostempWithFlags) {
  const std::string tmpl = GetTempDir() + "test_ostemp_flags.XXXXXX";
  char buffer[tmpl.size() + 1];
  strcpy(buffer, tmpl.c_str());

  const int fd = mkostemp(buffer, O_RDWR | O_CREAT | O_EXCL);
  ASSERT_NE(fd, -1) << "mkostemp failed: " << strerror(errno);

  EXPECT_EQ(close(fd), 0) << "close failed: " << strerror(errno);
  AddCreatedFile(buffer);
}

TEST_F(FileTempTest, MkostempInvalidTemplate) {
  const std::string tmpl = GetTempDir() + "invalid_template";
  char buffer[tmpl.size() + 1];
  strcpy(buffer, tmpl.c_str());

  const int fd = mkostemp(buffer, O_RDWR);
  EXPECT_EQ(fd, -1)
      << "mkostemp should fail for invalid template, but returned fd " << fd
      << ".";
  EXPECT_EQ(errno, EINVAL) << "mkostemp expected errno EINVAL (" << EINVAL
                           << ") for invalid template, but got " << errno
                           << " (" << strerror(errno) << ").";
}

TEST_F(FileTempTest, MkostempNonExistentPath) {
  const std::string tmpl = "/nonexistent/path/test_ostemp_file.XXXXXX";
  char buffer[tmpl.size() + 1];
  strcpy(buffer, tmpl.c_str());

  const int fd = mkostemp(buffer, O_RDWR);
  EXPECT_EQ(fd, -1)
      << "mkostemp should fail for non-existent path, but returned fd " << fd
      << ".";
  EXPECT_EQ(errno, ENOENT) << "mkostemp expected errno ENOENT (" << ENOENT
                           << ") for non-existent path, but got " << errno
                           << " (" << strerror(errno) << ").";
}

TEST_F(FileTempTest, MkostempWriteFile) {
  const std::string tmpl = GetTempDir() + "mkostemp_write_test.XXXXXX";
  char buffer[tmpl.size() + 1];
  strcpy(buffer, tmpl.c_str());

  const int fd = mkostemp(buffer, O_RDWR);
  ASSERT_NE(fd, -1) << "mkostemp failed: " << strerror(errno);

  const char test_data[] = "Hello, world!";
  const ssize_t bytes_written = write(fd, test_data, sizeof(test_data) - 1);
  EXPECT_EQ(static_cast<size_t>(bytes_written), sizeof(test_data) - 1)
      << "write failed or wrote incorrect bytes for fd " << fd
      << " with error: " << strerror(errno);

  EXPECT_EQ(close(fd), 0) << "close failed: " << strerror(errno);
  AddCreatedFile(buffer);
}

}  // namespace
}  // namespace nplb
