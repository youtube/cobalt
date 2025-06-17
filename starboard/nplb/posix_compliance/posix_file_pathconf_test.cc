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

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard::nplb {
namespace {

// Test to see if all supported configurations can be called without error.
// A pathconf call returning -1 does not necessarily mean that an error
// occurred (this can happen when a platform disables a configuration we
// support; whether an option is supported or not differs from platform to
// platform), so we only check if each call changes errno from 0 (meaning an
// actual error occurred).
TEST(PosixFilePathconfTest, AllConfigsCalledOnFiles) {
  const int kFileSize = 64;
  ScopedRandomFile random_file(kFileSize);
  const std::string& filename = random_file.filename();
#if defined(_PC_LINK_MAX)
  errno = 0;
  long result = pathconf(filename.c_str(), _PC_LINK_MAX);
  EXPECT_EQ(errno, 0) << "pathconf with _PC_LINK_MAX failed:"
                      << strerror(errno);
#endif  // defined(_PC_LINK_MAX)
#if defined(_PC_MAX_CANON)
  errno = 0;
  result = pathconf(filename.c_str(), _PC_MAX_CANON);
  EXPECT_EQ(errno, 0) << "pathconf with _PC_MAX_CANON failed:"
                      << strerror(errno);
#endif  // defined(_PC_MAX_CANON)
#if defined(_PC_MAX_INPUT)
  errno = 0;
  result = pathconf(filename.c_str(), _PC_MAX_INPUT);
  EXPECT_EQ(errno, 0) << "pathconf with _PC_MAX_INPUT failed:"
                      << strerror(errno);
#endif  // defined(_PC_MAX_INPUT)
#if defined(_PC_NAME_MAX)
  errno = 0;
  result = pathconf(filename.c_str(), _PC_NAME_MAX);
  EXPECT_EQ(errno, 0) << "pathconf with _PC_NAME_MAX failed:"
                      << strerror(errno);
#endif  // defined(_PC_NAME_MAX)
#if defined(_PC_PATH_MAX)
  errno = 0;
  result = pathconf(filename.c_str(), _PC_PATH_MAX);
  EXPECT_EQ(errno, 0) << "pathconf with _PC_PATH_MAX failed:"
                      << strerror(errno);
#endif  // defined(_PC_PATH_MAX)
#if defined(_PC_PIPE_BUF)
  errno = 0;
  result = pathconf(filename.c_str(), _PC_PIPE_BUF);
  EXPECT_EQ(errno, 0) << "pathconf with _PC_PIPE_BUF failed:"
                      << strerror(errno);
#endif  // defined(_PC_PIPE_BUF)
#if defined(_PC_CHOWN_RESTRICTED)
  errno = 0;
  result = pathconf(filename.c_str(), _PC_CHOWN_RESTRICTED);
  EXPECT_EQ(errno, 0) << "pathconf with _PC_CHOWN_RESTRICTED failed:"
                      << strerror(errno);
#endif  // defined(_PC_CHOWN_RESTRICTED)
#if defined(_PC_NO_TRUNC)
  errno = 0;
  result = pathconf(filename.c_str(), _PC_NO_TRUNC);
  EXPECT_EQ(errno, 0) << "pathconf with _PC_NO_TRUNC failed:"
                      << strerror(errno);
#endif  // defined(_PC_NO_TRUNC)
#if defined(_PC_VDISABLE)
  errno = 0;
  result = pathconf(filename.c_str(), _PC_VDISABLE);
  EXPECT_EQ(errno, 0) << "pathconf with _PC_VDISABLE failed:"
                      << strerror(errno);
#endif  // defined(_PC_VDISABLE)
#if defined(_PC_SYNC_IO)
  errno = 0;
  result = pathconf(filename.c_str(), _PC_SYNC_IO);
  EXPECT_EQ(errno, 0) << "pathconf with _PC_SYNC_IO failed:" << strerror(errno);
#endif  // defined(_PC_SYNC_IO)
#if defined(_PC_ASYNC_IO)
  errno = 0;
  result = pathconf(filename.c_str(), _PC_ASYNC_IO);
  EXPECT_EQ(errno, 0) << "pathconf with _PC_ASYNC_IO failed:"
                      << strerror(errno);
#endif  // defined(_PC_ASYNC_IO)
#if defined(_PC_PRIO_IO)
  errno = 0;
  result = pathconf(filename.c_str(), _PC_PRIO_IO);
  EXPECT_EQ(errno, 0) << "pathconf with _PC_PRIO_IO failed:" << strerror(errno);
#endif  // defined(_PC_PRIO_IO)
#if defined(_PC_SOCK_MAXBUF)
  errno = 0;
  result = pathconf(filename.c_str(), _PC_SOCK_MAXBUF);
  EXPECT_EQ(errno, 0) << "pathconf with _PC_SOCK_MAXBUF failed:"
                      << strerror(errno);
#endif  // defined(_PC_SOCK_MAXBUF)
#if defined(_PC_FILESIZEBITS)
  errno = 0;
  result = pathconf(filename.c_str(), _PC_FILESIZEBITS);
  EXPECT_EQ(errno, 0) << "pathconf with _PC_FILESIZEBITS failed:"
                      << strerror(errno);
#endif  // defined(_PC_FILESIZEBITS)
#if defined(_PC_REC_INCR_XFER_SIZE)
  errno = 0;
  result = pathconf(filename.c_str(), _PC_REC_INCR_XFER_SIZE);
  EXPECT_EQ(errno, 0) << "pathconf with _PC_REC_INCR_XFER_SIZE failed:"
                      << strerror(errno);
#endif  // defined(_PC_REC_INCR_XFER_SIZE)
#if defined(_PC_REC_MAX_XFER_SIZE)
  errno = 0;
  result = pathconf(filename.c_str(), _PC_REC_MAX_XFER_SIZE);
  EXPECT_EQ(errno, 0) << "pathconf with _PC_REC_MAX_XFER_SIZE failed:"
                      << strerror(errno);
#endif  // defined(_PC_REC_MAX_XFER_SIZE)
#if defined(_PC_REC_MIN_XFER_SIZE)
  errno = 0;
  result = pathconf(filename.c_str(), _PC_REC_MIN_XFER_SIZE);
  EXPECT_EQ(errno, 0) << "pathconf with _PC_REC_MIN_XFER_SIZE failed:"
                      << strerror(errno);
#endif  // defined(_PC_REC_MIN_XFER_SIZE)
#if defined(_PC_REC_XFER_ALIGN)
  errno = 0;
  result = pathconf(filename.c_str(), _PC_REC_XFER_ALIGN);
  EXPECT_EQ(errno, 0) << "pathconf with _PC_REC_XFER_ALIGN failed:"
                      << strerror(errno);
#endif  // defined(_PC_REC_XFER_ALIGN)
#if defined(_PC_ALLOC_SIZE_MIN)
  errno = 0;
  result = pathconf(filename.c_str(), _PC_ALLOC_SIZE_MIN);
  EXPECT_EQ(errno, 0) << "pathconf with _PC_ALLOC_SIZE_MIN failed:"
                      << strerror(errno);
#endif  // defined(_PC_ALLOC_SIZE_MIN)
#if defined(_PC_SYMLINK_MAX)
  errno = 0;
  result = pathconf(filename.c_str(), _PC_SYMLINK_MAX);
  EXPECT_EQ(errno, 0) << "pathconf with _PC_SYMLINK_MAX failed:"
                      << strerror(errno);
#endif  // defined(_PC_SYMLINK_MAX)
#if defined(_PC_2_SYMLINKS)
  errno = 0;
  result = pathconf(filename.c_str(), _PC_2_SYMLINKS);
  EXPECT_EQ(errno, 0) << "pathconf with _PC_2_SYMLINKS failed:"
                      << strerror(errno);
#endif  // defined(_PC_2_SYMLINKS)
}

TEST(PosixFilePathconfTest, NonExistentPath) {
  EXPECT_EQ(-1, pathconf("/test/non_existent_path123.txt", _PC_NAME_MAX));
  EXPECT_EQ(ENOENT, errno);
}

TEST(PosixFilePathconfTest, InvalidName) {
  const int kFileSize = 64;
  ScopedRandomFile random_file(kFileSize);
  const std::string& filename = random_file.filename();
  const int LARGE_UNDEFINED_NAME_VALUE = 100;
  EXPECT_EQ(-1, pathconf(filename.c_str(), -1));
  EXPECT_EQ(EINVAL, errno);

  errno = 0;
  EXPECT_EQ(-1, pathconf(filename.c_str(), LARGE_UNDEFINED_NAME_VALUE));
  EXPECT_EQ(EINVAL, errno);
}

}  // namespace
}  // namespace starboard::nplb
