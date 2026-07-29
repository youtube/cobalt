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

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>

#include "starboard/configuration_constants.h"
#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

TEST(PosixFileUnlinkatTest, SucceedsAbsolutePath) {
  ScopedRandomFile file;
  const std::string& filename = file.filename();
  ASSERT_TRUE(FileExists(filename.c_str()));

  // Invalid dirfd should be ignored since the path is absolute.
  EXPECT_EQ(unlinkat(-1, filename.c_str(), 0), 0);
  EXPECT_FALSE(FileExists(filename.c_str()));
}

TEST(PosixFileUnlinkatTest, SucceedsRelativePathWithDirfd) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.IsValid());

  std::string relative_filename = ScopedRandomFile::MakeRandomFilename();
  std::string full_path =
      temp_dir.path() + kSbFileSepString + relative_filename;

  int fd = open(full_path.c_str(), O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
  ASSERT_GE(fd, 0);
  close(fd);

  int dir_fd = open(temp_dir.path().c_str(), O_RDONLY);
  ASSERT_GE(dir_fd, 0);

  EXPECT_EQ(unlinkat(dir_fd, relative_filename.c_str(), 0), 0);
  EXPECT_FALSE(FileExists(full_path.c_str()));

  close(dir_fd);
}

TEST(PosixFileUnlinkatTest, SucceedsRemoveDir) {
  ScopedRandomFile temp_file(ScopedRandomFile::kDontCreate);
  const std::string& path = temp_file.filename();

  ASSERT_EQ(mkdir(path.c_str(), S_IRUSR | S_IWUSR | S_IXUSR), 0);
  ASSERT_TRUE(DirectoryExists(path.c_str()));

  EXPECT_EQ(unlinkat(AT_FDCWD, path.c_str(), AT_REMOVEDIR), 0);
  EXPECT_FALSE(DirectoryExists(path.c_str()));
}

TEST(PosixFileUnlinkatTest, FailsInvalidDirfdRelativePath) {
  EXPECT_EQ(unlinkat(-1, "relative_path", 0), -1);
  EXPECT_EQ(errno, EBADF);
}

TEST(PosixFileUnlinkatTest, FailsInvalidFlags) {
  ScopedRandomFile file;
  int invalid_flags = ~AT_REMOVEDIR;
  EXPECT_EQ(unlinkat(AT_FDCWD, file.filename().c_str(), invalid_flags), -1);
  EXPECT_EQ(errno, EINVAL);
}

TEST(PosixFileUnlinkatTest, FailsDirfdNotADirectory) {
  ScopedRandomFile file;

  int fd = open(file.filename().c_str(), O_RDONLY);
  ASSERT_GE(fd, 0);

  EXPECT_EQ(unlinkat(fd, "relative_path", 0), -1);
  EXPECT_EQ(errno, ENOTDIR);

  close(fd);
}

TEST(PosixFileUnlinkatTest, FailsNotADirectoryButAtRemovedirSet) {
  ScopedRandomFile file;
  EXPECT_EQ(unlinkat(AT_FDCWD, file.filename().c_str(), AT_REMOVEDIR), -1);
  EXPECT_EQ(errno, ENOTDIR);
}

TEST(PosixFileUnlinkatTest, FailsFileDoesNotExist) {
  ScopedRandomFile file(ScopedRandomFile::kDontCreate);
  EXPECT_EQ(unlinkat(AT_FDCWD, file.filename().c_str(), 0), -1);
  EXPECT_EQ(errno, ENOENT);
}

TEST(PosixFileUnlinkatTest, FailsUnlinkDirectoryWithoutFlag) {
  ScopedRandomFile temp_dir_file(ScopedRandomFile::kDontCreate);
  const std::string& path = temp_dir_file.filename();
  ASSERT_EQ(mkdir(path.c_str(), 0700), 0);

  EXPECT_EQ(unlinkat(AT_FDCWD, path.c_str(), 0), -1);
  // POSIX only specifies EPERM, but some implementations use EISDIR instead.
  EXPECT_TRUE(errno == EPERM || errno == EISDIR)
      << "Expected EPERM or EISDIR, got " << errno << "(" << strerror(errno)
      << ")";

  rmdir(path.c_str());
}

TEST(PosixFileUnlinkatTest, FailsNameTooLong) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.IsValid());

  int dir_fd = open(temp_dir.path().c_str(), O_RDONLY);
  ASSERT_GE(dir_fd, 0);

  std::string long_name(kSbFileMaxPath + 1, 'a');

  EXPECT_EQ(unlinkat(dir_fd, long_name.c_str(), 0), -1);
  EXPECT_EQ(errno, ENAMETOOLONG);

  close(dir_fd);
}

}  // namespace
}  // namespace nplb
