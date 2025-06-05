#include <fcntl.h>
#include <unistd.h>

#include <vector> // Added for std::vector
#include "starboard/common/string.h"
#include "starboard/nplb/file_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class FileTempTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {
    // Clean up any files created during the tests.
    for (const auto& filename : created_files_) {
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
  const std::string tmpl = GetTempDir() + "my_temp_dir.XXXXXX"; // tmpl is already const
  std::vector<char> buffer(tmpl.begin(), tmpl.end());
  buffer.push_back('\0'); // Null-terminate for C-style string functions

  char* const result = mkdtemp(buffer.data()); // result is already char* const
  ASSERT_THAT(result, ::testing::NotNull())
      << "mkdtemp failed for template '" << tmpl << "': " << strerror(errno);
}

TEST_F(FileTempTest, MkdtempInvalidTemplate) {
  const std::string tmpl = GetTempDir() + "invalid_template"; // tmpl is already const
  std::vector<char> buffer(tmpl.begin(), tmpl.end());
  buffer.push_back('\0');

  char* const result = mkdtemp(buffer.data()); // result is already char* const
  ASSERT_THAT(result, ::testing::IsNull())
      << "mkdtemp should fail for invalid template, but returned '"
      << (result ? result : "nullptr") << "'.";
  EXPECT_EQ(errno, EINVAL) << "mkdtemp expected errno EINVAL (" << EINVAL
                           << ") for invalid template, but got " << errno
                           << " (" << strerror(errno) << ").";
}

TEST_F(FileTempTest, MkdtempNonExistentPath) {
  const std::string tmpl = "/nonexistent/path/temp.XXXXXX"; // tmpl is already const
  std::vector<char> buffer(tmpl.begin(), tmpl.end());
  buffer.push_back('\0');

  char* const result = mkdtemp(buffer.data()); // result is already char* const
  ASSERT_THAT(result, ::testing::IsNull())
      << "mkdtemp should fail for non-existent path, but returned '"
      << (result ? result : "nullptr") << "'.";
  EXPECT_EQ(errno, ENOENT) << "mkdtemp expected errno ENOENT (" << ENOENT
                           << ") for non-existent path, but got " << errno
                           << " (" << strerror(errno) << ").";
}

TEST_F(FileTempTest, MkstempBasicSuccess) {
  const std::string tmpl = GetTempDir() + "test_file.XXXXXX"; // tmpl is already const
  std::vector<char> buffer(tmpl.begin(), tmpl.end());
  buffer.push_back('\0');

  const int fd = mkstemp(buffer.data()); // fd is already const
  ASSERT_NE(fd, -1) << "mkstemp failed: " << strerror(errno);

  EXPECT_EQ(close(fd), 0) << "close failed: " << strerror(errno);
  AddCreatedFile(buffer.data());  // Add to cleanup list
}

TEST_F(FileTempTest, MkstempInvalidTemplate) {
  const std::string tmpl = GetTempDir() + "invalid_template"; // tmpl is already const
  std::vector<char> buffer(tmpl.begin(), tmpl.end());
  buffer.push_back('\0');

  const int fd = mkstemp(buffer.data()); // fd is already const
  EXPECT_EQ(fd, -1)
      << "mkstemp should fail for invalid template, but returned fd " << fd
      << ".";
  EXPECT_EQ(errno, EINVAL) << "mkstemp expected errno EINVAL (" << EINVAL
                           << ") for invalid template, but got " << errno
                           << " (" << strerror(errno) << ").";
}

TEST_F(FileTempTest, MkstempNonExistentPath) {
  const std::string tmpl = "/nonexistent/path/test_file.XXXXXX"; // tmpl is already const
  std::vector<char> buffer(tmpl.begin(), tmpl.end());
  buffer.push_back('\0');

  const int fd = mkstemp(buffer.data()); // fd is already const
  EXPECT_EQ(fd, -1)
      << "mkstemp should fail for non-existent path, but returned fd " << fd
      << ".";
  EXPECT_EQ(errno, ENOENT) << "mkstemp expected errno ENOENT (" << ENOENT
                           << ") for non-existent path, but got " << errno
                           << " (" << strerror(errno) << ").";
}

TEST_F(FileTempTest, MkstempFileIsOpened) {
  const std::string tmpl = GetTempDir() + "open_file_test.XXXXXX"; // tmpl is already const
  std::vector<char> buffer(tmpl.begin(), tmpl.end());
  buffer.push_back('\0');

  const int fd = mkstemp(buffer.data()); // fd is already const
  ASSERT_NE(fd, -1) << "mkstemp failed: " << strerror(errno);

  // Try to write something to the file descriptor.
  const char* test_data = "test data"; // test_data is already const char*
  const ssize_t bytes_written = write(fd, test_data, strlen(test_data)); // bytes_written is already const
  EXPECT_EQ(bytes_written, strlen(test_data))
      << "write failed or wrote incorrect bytes for fd " << fd
      << " with error: " << strerror(errno);

  EXPECT_EQ(close(fd), 0) << "close failed: " << strerror(errno);
  AddCreatedFile(buffer.data());
}

TEST_F(FileTempTest, MkostempBasicSuccess) {
  const std::string tmpl = GetTempDir() + "test_ostemp_file.XXXXXX"; // tmpl is already const
  std::vector<char> buffer(tmpl.begin(), tmpl.end());
  buffer.push_back('\0');

  const int fd = mkostemp(buffer.data(), O_RDWR); // fd is already const
  ASSERT_NE(fd, -1) << "mkostemp failed: " << strerror(errno);

  EXPECT_EQ(close(fd), 0) << "close failed: " << strerror(errno);
  AddCreatedFile(buffer.data());

  struct stat st {};
  const int stat_result = stat(buffer.data(), &st); // stat_result is already const
  ASSERT_EQ(stat_result, 0) << "stat failed: " << strerror(errno);
  ASSERT_TRUE(S_ISREG(st.st_mode));
}

TEST_F(FileTempTest, MkostempWithFlags) {
  const std::string tmpl = GetTempDir() + "test_ostemp_flags.XXXXXX"; // tmpl is already const
  std::vector<char> buffer(tmpl.begin(), tmpl.end());
  buffer.push_back('\0');

  const int fd = mkostemp(buffer.data(), O_RDWR | O_CREAT | O_EXCL); // fd is already const
  ASSERT_NE(fd, -1) << "mkostemp failed: " << strerror(errno);

  EXPECT_EQ(close(fd), 0) << "close failed: " << strerror(errno);
  AddCreatedFile(buffer.data());
}

TEST_F(FileTempTest, MkostempInvalidTemplate) {
  const std::string tmpl = GetTempDir() + "invalid_template"; // tmpl is already const
  std::vector<char> buffer(tmpl.begin(), tmpl.end());
  buffer.push_back('\0');

  const int fd = mkostemp(buffer.data(), O_RDWR); // fd is already const
  EXPECT_EQ(fd, -1)
      << "mkostemp should fail for invalid template, but returned fd " << fd
      << ".";
  EXPECT_EQ(errno, EINVAL) << "mkostemp expected errno EINVAL (" << EINVAL
                           << ") for invalid template, but got " << errno
                           << " (" << strerror(errno) << ").";
}

TEST_F(FileTempTest, MkostempNonExistentPath) {
  const std::string tmpl = "/nonexistent/path/test_ostemp_file.XXXXXX"; // tmpl is already const
  std::vector<char> buffer(tmpl.begin(), tmpl.end());
  buffer.push_back('\0');

  const int fd = mkostemp(buffer.data(), O_RDWR); // fd is already const
  EXPECT_EQ(fd, -1)
      << "mkostemp should fail for non-existent path, but returned fd " << fd
      << ".";
  EXPECT_EQ(errno, ENOENT) << "mkostemp expected errno ENOENT (" << ENOENT
                           << ") for non-existent path, but got " << errno
                           << " (" << strerror(errno) << ").";
}

TEST_F(FileTempTest, MkostempWriteFile) {
  const std::string tmpl = GetTempDir() + "mkostemp_write_test.XXXXXX"; // tmpl is already const
  std::vector<char> buffer(tmpl.begin(), tmpl.end());
  buffer.push_back('\0');

  const int fd = mkostemp(buffer.data(), O_RDWR); // fd is already const
  ASSERT_NE(fd, -1) << "mkostemp failed: " << strerror(errno);

  const char test_data[] = "Hello, world!"; // test_data is already const char[]
  const ssize_t bytes_written = write(fd, test_data, sizeof(test_data) - 1); // bytes_written is already const
  EXPECT_EQ(bytes_written, sizeof(test_data) - 1)
      << "write failed or wrote incorrect bytes for fd " << fd
      << " with error: " << strerror(errno);

  EXPECT_EQ(close(fd), 0) << "close failed: " << strerror(errno);
  AddCreatedFile(buffer.data());
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
