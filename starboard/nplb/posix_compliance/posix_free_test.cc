// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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
Untested scenarios:
Calling free() on a pointer that has already been freed is undefined
behavior.
Calling free() on a pointer not returned by an allocation function
(e.g., a pointer to a stack variable) is undefined behavior.
*/

#include <stdlib.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard::nplb {
namespace {

// The POSIX spec states that if ptr is a null pointer, no action shall occur.
TEST(PosixFreeTest, FreeNullDoesNothing) {
  free(NULL);
  // The test passes if the above line does not crash.
  SUCCEED();
}

// The free() function shall not modify errno if ptr is a null pointer.
TEST(PosixFreeTest, FreeNullDoesNotModifyErrno) {
  errno = EIO;
  free(NULL);
  EXPECT_EQ(errno, EIO);
}

// Tests that a valid pointer from malloc() can be freed.
TEST(PosixFreeTest, FreesMemoryFromMalloc) {
  void* mem = malloc(1024);
  ASSERT_NE(mem, nullptr);
  free(mem);
  SUCCEED();
}

// Tests that a valid pointer from posix_memalign() can be freed.
TEST(PosixFreeTest, FreesMemoryFromPosixMemalign) {
  void* mem = NULL;
  int result = posix_memalign(&mem, sizeof(void*) * 2, 1024);
  ASSERT_EQ(result, 0);
  ASSERT_NE(mem, nullptr);
  free(mem);
  SUCCEED();
}

// The free() function shall not modify errno for a valid pointer.
TEST(PosixFreeTest, FreeValidPointerDoesNotModifyErrno) {
  void* mem = malloc(16);
  ASSERT_NE(mem, nullptr);
  errno = EIO;
  free(mem);
  EXPECT_EQ(errno, EIO);
}

}  // namespace
}  // namespace starboard::nplb
