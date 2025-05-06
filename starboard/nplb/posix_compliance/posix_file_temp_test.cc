#include <fcntl.h>
#include <unistd.h>

#include "starboard/common/string.h"
#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
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

  char* result = mkdtemp(buffer);
  ASSERT_NE(result, nullptr);
}

TEST_F(FileTempTest, MkdtempInvalidTemplate) {
  const std::string tmpl = GetTempDir() + "invalid_template";
  char buffer[tmpl.length() + 1];
  strcpy(buffer, tmpl.c_str());

  char* result = mkdtemp(buffer);
  EXPECT_EQ(result, nullptr);
  EXPECT_EQ(errno, EINVAL);
}

TEST_F(FileTempTest, MkdtempNonExistentPath) {
  const std::string tmpl = "/nonexistent/path/temp.XXXXXX";
  char buffer[tmpl.length() + 1];
  strcpy(buffer, tmpl.c_str());

  char* result = mkdtemp(buffer);
  EXPECT_EQ(result, nullptr);
  EXPECT_EQ(errno, ENOENT);
}

TEST_F(FileTempTest, MkstempBasicSuccess) {
  const std::string tmpl = GetTempDir() + "test_file.XXXXXX";
  char buffer[tmpl.size() + 1];
  strcpy(buffer, tmpl.c_str());

  const int fd = mkstemp(buffer);
  ASSERT_NE(fd, -1);

  EXPECT_EQ(close(fd), 0);
  AddCreatedFile(buffer);  // Add to cleanup list
}

TEST_F(FileTempTest, MkstempInvalidTemplate) {
  const std::string tmpl = GetTempDir() + "invalid_template";
  char buffer[tmpl.size() + 1];
  strcpy(buffer, tmpl.c_str());

  const int fd = mkstemp(buffer);
  EXPECT_EQ(fd, -1);
  EXPECT_EQ(errno, EINVAL);
}

TEST_F(FileTempTest, MkstempNonExistentPath) {
  const std::string tmpl = "/nonexistent/path/test_file.XXXXXX";
  char buffer[tmpl.size() + 1];
  strcpy(buffer, tmpl.c_str());

  const int fd = mkstemp(buffer);
  EXPECT_EQ(fd, -1);
  EXPECT_EQ(errno, ENOENT);
}

TEST_F(FileTempTest, MkstempFileIsOpened) {
  const std::string tmpl = GetTempDir() + "open_file_test.XXXXXX";
  char buffer[tmpl.size() + 1];
  strcpy(buffer, tmpl.c_str());

  const int fd = mkstemp(buffer);
  ASSERT_NE(fd, -1) << "mkstemp failed: " << errno;

  // Try to write something to the file descriptor.
  const char* test_data = "test data";
  const ssize_t bytes_written = write(fd, test_data, strlen(test_data));
  EXPECT_EQ(bytes_written, strlen(test_data));

  EXPECT_EQ(close(fd), 0);
  AddCreatedFile(buffer);
}

TEST_F(FileTempTest, MkostempBasicSuccess) {
  const std::string tmpl = GetTempDir() + "test_ostemp_file.XXXXXX";
  char buffer[tmpl.size() + 1];
  strcpy(buffer, tmpl.c_str());

  const int fd = mkostemp(buffer, O_RDWR);
  ASSERT_NE(fd, -1);

  EXPECT_EQ(close(fd), 0);
  AddCreatedFile(buffer);

  struct stat st;
  ASSERT_EQ(stat(buffer, &st), 0);
  ASSERT_TRUE(S_ISREG(st.st_mode));
}

TEST_F(FileTempTest, MkostempWithFlags) {
  const std::string tmpl = GetTempDir() + "test_ostemp_flags.XXXXXX";
  char buffer[tmpl.size() + 1];
  strcpy(buffer, tmpl.c_str());

  const int fd = mkostemp(buffer, O_RDWR | O_CREAT | O_EXCL);
  ASSERT_NE(fd, -1);

  EXPECT_EQ(close(fd), 0);
  AddCreatedFile(buffer);
}

TEST_F(FileTempTest, MkostempInvalidTemplate) {
  const std::string tmpl = GetTempDir() + "invalid_template";
  char buffer[tmpl.size() + 1];
  strcpy(buffer, tmpl.c_str());

  const int fd = mkostemp(buffer, O_RDWR);
  EXPECT_EQ(fd, -1);
  EXPECT_EQ(errno, EINVAL);
}

TEST_F(FileTempTest, MkostempNonExistentPath) {
  const std::string tmpl = "/nonexistent/path/test_ostemp_file.XXXXXX";
  char buffer[tmpl.size() + 1];
  strcpy(buffer, tmpl.c_str());

  const int fd = mkostemp(buffer, O_RDWR);
  EXPECT_EQ(fd, -1);
  EXPECT_EQ(errno, ENOENT);
}

TEST_F(FileTempTest, MkostempWriteFile) {
  const std::string tmpl = GetTempDir() + "mkostemp_write_test.XXXXXX";
  char buffer[tmpl.size() + 1];
  strcpy(buffer, tmpl.c_str());

  const int fd = mkostemp(buffer, O_RDWR);
  ASSERT_NE(fd, -1);

  const char test_data[] = "Hello, world!";
  ssize_t bytes_written = write(fd, test_data, sizeof(test_data) - 1);
  EXPECT_EQ(bytes_written, sizeof(test_data) - 1);

  EXPECT_EQ(close(fd), 0);
  AddCreatedFile(buffer);
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
