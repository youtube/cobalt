// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

TEST(PosixSysMincoreTest, SucceedsForValidAddress) {
  void* mem =
      mmap(NULL, 1, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  ASSERT_NE(mem, MAP_FAILED);

  static_cast<char*>(mem)[0] = 'A';

  unsigned char vec;
  errno = 0;
  int result = mincore(mem, 1, &vec);

  EXPECT_EQ(result, 0) << "mincore failed with error: " << strerror(errno);
  EXPECT_EQ(vec & 1, 1) << "Page not in core.";

  EXPECT_EQ(munmap(mem, 1), 0);
}

TEST(PosixSysMincoreTest, FailsForInvalidAddress) {
  void* invalid_addr = reinterpret_cast<void*>(1);
  unsigned char vec;

  errno = 0;
  int result = mincore(invalid_addr, 1, &vec);

  EXPECT_EQ(result, -1);
  EXPECT_EQ(errno, EINVAL);
}

}  // namespace
}  // namespace nplb
