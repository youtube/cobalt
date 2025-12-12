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
#include <limits.h>
#include <string.h>
#include <unistd.h>

#include <tuple>

#include "starboard/nplb/file_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

// General information of the pathconf function can be found here:
// https://pubs.opengroup.org/onlinepubs/9699919799/functions/fpathconf.html
//
// Specifications on acceptable minimum values can be found here:
// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/limits.h.html
//
// |FILESIZEBITS| has the same minimum acceptable value,
// regardless of platform.
constexpr int FILE_SIZE_BITS = 32;
// If there is no specification on a minimum value for a configuration,
// we set their expected minimum value to -1. Values whose implementation
// specify that they are to be used in "#if preprocessing directives"
// will also hold this value.
constexpr int MINIMUM_NOT_SPECIFIED = -1;

class PosixFilePathconfTest : public ::testing::Test {
 protected:
  PosixFilePathconfTest() : random_file_(kFileSize) {}
  const int kFileSize = 64;
  ScopedRandomFile random_file_;
};

class PosixFilePathconfValidNameTest
    : public PosixFilePathconfTest,
      public ::testing::WithParamInterface<std::tuple<int, int>> {};

// A pathconf call with a valid name should not fail with EINVAL. If a pathconf
// call does not return with errno EINVAL, we then can assume that the value
// is defined with a specified value or has no limit (denoted by a return value
// of -1).
TEST_P(PosixFilePathconfValidNameTest, PathconfDoesNotFailWithEinval) {
  int pc_name = std::get<0>(GetParam());
  long minimum_value = std::get<1>(GetParam());
  const std::string& filename = random_file_.filename();

  errno = 0;
  long result = pathconf(filename.c_str(), pc_name);

  EXPECT_NE(errno, EINVAL)
      << "pathconf(" << pc_name
      << ") failed with EINVAL, meaning the name is considered invalid.";

  // If the result of pathconf is -1, but errno is 0, we can assume that there
  // is no specified minimum value. If there is a specified minimum value, this
  // check ensures that that minimum value is acceptable.
  //
  // Note: As per the documentation on limits, configurations who have _MIN in
  // their name actually use their "acceptable minimum value" as their highest
  // value, meaning that the |result| should be compared using EXPECT_LE.
  // However, we are able to get away with just only using EXPECT_GE as all the
  // _MIN values that MUSL supports have no specified "acceptable minimum
  // value". If, in the future, more _MIN configurations are supported that have
  // specified "acceptable minimum values", we must then edit these tests to
  // support EXPECT_LE.
  if (result != -1) {
    EXPECT_GE(result, minimum_value)
        << "pathconf(" << pc_name
        << ") did not have the minimum expected value of " << minimum_value
        << ", but had actual value of " << result;
  }
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

INSTANTIATE_TEST_SUITE_P(
    PosixPathconfTests,
    PosixFilePathconfValidNameTest,
    ::testing::Values(
#if defined(_PC_LINK_MAX)
        std::make_tuple(_PC_LINK_MAX, _POSIX_LINK_MAX),
#endif  // defined(_PC_LINK_MAX)
#if defined(_PC_MAX_CANON)
        std::make_tuple(_PC_MAX_CANON, _POSIX_MAX_CANON),
#endif  // defined(_PC_MAX_CANON)
#if defined(_PC_MAX_INPUT)
        std::make_tuple(_PC_MAX_INPUT, _POSIX_MAX_INPUT),
#endif  // defined(_PC_MAX_INPUT)
#if defined(_PC_NAME_MAX)
        std::make_tuple(_PC_NAME_MAX, _POSIX_NAME_MAX),
#endif  // defined(_PC_NAME_MAX)
#if defined(_PC_PATH_MAX)
        std::make_tuple(_PC_PATH_MAX, _POSIX_PATH_MAX),
#endif  // defined(_PC_PATH_MAX)
#if defined(_PC_PIPE_BUF)
        std::make_tuple(_PC_PIPE_BUF, _POSIX_PIPE_BUF),
#endif  // defined(_PC_PIPE_BUF)
#if defined(_PC_CHOWN_RESTRICTED)
        std::make_tuple(_PC_CHOWN_RESTRICTED, _POSIX_CHOWN_RESTRICTED),
#endif  // defined(_PC_CHOWN_RESTRICTED)
#if defined(_PC_NO_TRUNC)
        std::make_tuple(_PC_NO_TRUNC, _POSIX_NO_TRUNC),
#endif  // defined(_PC_NO_TRUNC)
#if defined(_PC_VDISABLE)
        std::make_tuple(_PC_VDISABLE, _POSIX_VDISABLE),
#endif  // defined(_PC_VDISABLE)
#if defined(_PC_SYNC_IO)
        std::make_tuple(_PC_SYNC_IO, MINIMUM_NOT_SPECIFIED),
#endif  // defined(_PC_SYNC_IO)
#if defined(_PC_ASYNC_IO)
        std::make_tuple(_PC_ASYNC_IO, MINIMUM_NOT_SPECIFIED),
#endif  // defined(_PC_ASYNC_IO)
#if defined(_PC_PRIO_IO)
        std::make_tuple(_PC_PRIO_IO, MINIMUM_NOT_SPECIFIED),
#endif  // defined(_PC_PRIO_IO)
#if defined(_PC_SOCK_MAXBUF)
        std::make_tuple(_PC_SOCK_MAXBUF, MINIMUM_NOT_SPECIFIED),
#endif  // defined(_PC_SOCK_MAXBUF)
#if defined(_PC_FILESIZEBITS)
        std::make_tuple(_PC_FILESIZEBITS, FILE_SIZE_BITS),
#endif  // defined(_PC_FILESIZEBITS)
#if defined(_PC_REC_INCR_XFER_SIZE)
        std::make_tuple(_PC_REC_INCR_XFER_SIZE, MINIMUM_NOT_SPECIFIED),
#endif  // defined(_PC_REC_INCR_XFER_SIZE)
#if defined(_PC_REC_MAX_XFER_SIZE)
        std::make_tuple(_PC_REC_MAX_XFER_SIZE, MINIMUM_NOT_SPECIFIED),
#endif  // defined(_PC_REC_MAX_XFER_SIZE)
#if defined(_PC_REC_MIN_XFER_SIZE)
        std::make_tuple(_PC_REC_MIN_XFER_SIZE, MINIMUM_NOT_SPECIFIED),
#endif  // defined(_PC_REC_MIN_XFER_SIZE)
#if defined(_PC_REC_XFER_ALIGN)
        std::make_tuple(_PC_REC_XFER_ALIGN, MINIMUM_NOT_SPECIFIED),
#endif  // defined(_PC_REC_XFER_ALIGN)
#if defined(_PC_ALLOC_SIZE_MIN)
        std::make_tuple(_PC_ALLOC_SIZE_MIN, MINIMUM_NOT_SPECIFIED),
#endif  // defined(_PC_ALLOC_SIZE_MIN)
#if defined(_PC_SYMLINK_MAX)
        std::make_tuple(_PC_SYMLINK_MAX, _POSIX_SYMLINK_MAX),
#endif  // defined(_PC_SYMLINK_MAX)
#if defined(_PC_2_SYMLINKS)
        std::make_tuple(_PC_2_SYMLINKS, MINIMUM_NOT_SPECIFIED)
#endif  // defined(_PC_2_SYMLINKS)
            ));

}  // namespace
}  // namespace nplb
