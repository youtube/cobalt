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

/*
  The following error conditions are not tested due to the difficulty of
  reliably triggering them in a portable unit testing environment:
  - EACCES: Read or search permission was denied for a component of the
            pathname.
  - ENOMEM: There is insufficient memory to satisfy the request when dynamically
            allocating a buffer.

  The non-standard `getcwd(NULL, 0)` behavior, where the implementation is
  expected to `malloc` a buffer, is not supported and therefore not tested.
*/

#include <errno.h>
#include <limits.h>
#include <string.h>

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

TEST(PosixGetcwdTest, SucceedsWithSufficientBuffer) {
  char path_buffer[PATH_MAX];
  errno = 0;

  char* result = getcwd(path_buffer, sizeof(path_buffer));
  ASSERT_NE(nullptr, result)
      << "getcwd failed with unexpected error: " << strerror(errno);
  EXPECT_EQ(path_buffer, result)
      << "getcwd should return a pointer to the provided buffer on success.";

  ASSERT_GT(strlen(result), 0u) << "Path should not be empty.";
  EXPECT_EQ('/', result[0]) << "Path should be absolute.";
}

TEST(PosixGetcwdTest, FailsWithErrangeForSmallBuffer) {
  char full_path[PATH_MAX];
  ASSERT_NE(nullptr, getcwd(full_path, sizeof(full_path)));
  size_t path_len = strlen(full_path);

  // Attempt to get the path with a buffer that is one byte too small to hold
  // the path and the null terminator. A size of |path_len| is too small.
  std::vector<char> small_buffer(path_len);
  errno = 0;
  char* result = getcwd(small_buffer.data(), small_buffer.size());

  EXPECT_EQ(nullptr, result)
      << "getcwd should have returned nullptr for an undersized buffer.";
  EXPECT_EQ(ERANGE, errno) << "errno should be ERANGE, but was: " << errno
                           << " (" << strerror(errno) << ")";
}

TEST(PosixGetcwdTest, FailsWithEinvalForZeroSizeNonNullBuffer) {
  char path_buffer[1];
  errno = 0;
  char* result = getcwd(path_buffer, 0);

  EXPECT_EQ(nullptr, result)
      << "getcwd should fail for a non-null buffer with size 0.";
  EXPECT_EQ(EINVAL, errno) << "errno should be EINVAL, but was: " << errno
                           << " (" << strerror(errno) << ")";
}

}  // namespace
}  // namespace nplb
