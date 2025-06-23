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

class PosixFilePathconfTest : public ::testing::Test {
 protected:
  PosixFilePathconfTest() : random_file_(kFileSize) {}
  const int kFileSize = 64;
  ScopedRandomFile random_file_;
};

class PosixFilePathconfValidNameTest
    : public PosixFilePathconfTest,
      public ::testing::WithParamInterface<int> {};

// A pathconf call with a valid name should not fail with EINVAL. It may
// return -1 if there is no limit for a given resource, but errno should not
// be set to EINVAL. This test verifies that the configuration names are
// recognized by the system.
TEST_P(PosixFilePathconfValidNameTest, PathconfDoesNotFailWithEinval) {
  int pc_name = GetParam();
  const std::string& filename = random_file_.filename();

  errno = 0;
  pathconf(filename.c_str(), pc_name);

  EXPECT_NE(errno, EINVAL)
      << "pathconf(" << pc_name
      << ") failed with EINVAL, meaning the name is considered invalid.";
}

TEST_F(PosixFilePathconfTest, InvalidNameFails) {
  const std::string& filename = random_file_.filename();
  errno = 0;
  long result = pathconf(filename.c_str(), -1);

  EXPECT_EQ(result, -1);
  EXPECT_EQ(errno, EINVAL)
      << "pathconf with an invalid name failed with an unexpected error: "
      << strerror(errno);
}

INSTANTIATE_TEST_SUITE_P(PosixPathconfTests,
                         PosixFilePathconfValidNameTest,
                         ::testing::Values(
#if defined(_PC_LINK_MAX)
                             _PC_LINK_MAX,
#endif  // defined(_PC_LINK_MAX)
#if defined(_PC_MAX_CANON)
                             _PC_MAX_CANON,
#endif  // defined(_PC_MAX_CANON)
#if defined(_PC_MAX_INPUT)
                             _PC_MAX_INPUT,
#endif  // defined(_PC_MAX_INPUT)
#if defined(_PC_NAME_MAX)
                             _PC_NAME_MAX,
#endif  // defined(_PC_NAME_MAX)
#if defined(_PC_PATH_MAX)
                             _PC_PATH_MAX,
#endif  // defined(_PC_PATH_MAX)
#if defined(_PC_PIPE_BUF)
                             _PC_PIPE_BUF,
#endif  // defined(_PC_PIPE_BUF)
#if defined(_PC_CHOWN_RESTRICTED)
                             _PC_CHOWN_RESTRICTED,
#endif  // defined(_PC_CHOWN_RESTRICTED)
#if defined(_PC_NO_TRUNC)
                             _PC_NO_TRUNC,
#endif  // defined(_PC_NO_TRUNC)
#if defined(_PC_VDISABLE)
                             _PC_VDISABLE,
#endif  // defined(_PC_VDISABLE)
#if defined(_PC_SYNC_IO)
                             _PC_SYNC_IO,
#endif  // defined(_PC_SYNC_IO)
#if defined(_PC_ASYNC_IO)
                             _PC_ASYNC_IO,
#endif  // defined(_PC_ASYNC_IO)
#if defined(_PC_PRIO_IO)
                             _PC_PRIO_IO,
#endif  // defined(_PC_PRIO_IO)
#if defined(_PC_SOCK_MAXBUF)
                             _PC_SOCK_MAXBUF,
#endif  // defined(_PC_SOCK_MAXBUF)
#if defined(_PC_FILESIZEBITS)
                             _PC_FILESIZEBITS,
#endif  // defined(_PC_FILESIZEBITS)
#if defined(_PC_REC_INCR_XFER_SIZE)
                             _PC_REC_INCR_XFER_SIZE,
#endif  // defined(_PC_REC_INCR_XFER_SIZE)
#if defined(_PC_REC_MAX_XFER_SIZE)
                             _PC_REC_MAX_XFER_SIZE,
#endif  // defined(_PC_REC_MAX_XFER_SIZE)
#if defined(_PC_REC_MIN_XFER_SIZE)
                             _PC_REC_MIN_XFER_SIZE,
#endif  // defined(_PC_REC_MIN_XFER_SIZE)
#if defined(_PC_REC_XFER_ALIGN)
                             _PC_REC_XFER_ALIGN,
#endif  // defined(_PC_REC_XFER_ALIGN)
#if defined(_PC_ALLOC_SIZE_MIN)
                             _PC_ALLOC_SIZE_MIN,
#endif  // defined(_PC_ALLOC_SIZE_MIN)
#if defined(_PC_SYMLINK_MAX)
                             _PC_SYMLINK_MAX,
#endif  // defined(_PC_SYMLINK_MAX)
#if defined(_PC_2_SYMLINKS)
                             _PC_2_SYMLINKS
#endif  // defined(_PC_2_SYMLINKS)
                             ));

}  // namespace
}  // namespace starboard::nplb
