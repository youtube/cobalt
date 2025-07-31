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
#include <sys/auxv.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class PosixAuxvGetauxvalRequiredTest
    : public ::testing::TestWithParam<unsigned long> {
 protected:
  void SetUp() override {
    // Clear errno before each test to ensure a clean state.
    errno = 0;
  }
};

class PosixAuxvGetauxvalOptionalTest
    : public ::testing::TestWithParam<unsigned long> {
 protected:
  void SetUp() override {
    // Clear errno before each test to ensure a clean state.
    errno = 0;
  }
};

TEST_P(PosixAuxvGetauxvalRequiredTest, RequiredValuesAreValid) {
  unsigned long at_name = GetParam();
  getauxval(at_name);
  EXPECT_EQ(errno, 0) << "getauxval failed with error: " << strerror(errno);
}

TEST_P(PosixAuxvGetauxvalOptionalTest, OptionalValuesAreValid) {
  unsigned long at_name = GetParam();
  getauxval(at_name);
  EXPECT_EQ(errno, 0) << "getauxval failed with error: " << strerror(errno);
}

TEST(PosixAuxvGetauxvalTest, InvalidTypeFails) {
  // Use a value that is guaranteed to be an invalid type.
  errno = 0;
  unsigned long result = getauxval(0);
  EXPECT_EQ(result, 0u);
  EXPECT_EQ(errno, ENOENT)
      << "getauxval with an invalid name failed with an unexpected error: "
      << strerror(errno);
}

INSTANTIATE_TEST_SUITE_P(PosixAuxvGetauxvalRequiredTests,
                         PosixAuxvGetauxvalRequiredTest,
                         ::testing::Values(AT_HWCAP, AT_HWCAP2, AT_EXECFN));

INSTANTIATE_TEST_SUITE_P(PosixAuxvGetauxvalOptionalTests,
                         PosixAuxvGetauxvalOptionalTest,
                         ::testing::Values(
#if defined(AT_PHDR)
                             AT_PHDR,
#endif
#if defined(AT_PHENT)
                             AT_PHENT,
#endif
#if defined(AT_PHNUM)
                             AT_PHNUM,
#endif
#if defined(AT_PAGESZ)
                             AT_PAGESZ,
#endif
#if defined(AT_BASE)
                             AT_BASE,
#endif
#if defined(AT_FLAGS)
                             AT_FLAGS,
#endif
#if defined(AT_ENTRY)
                             AT_ENTRY,
#endif
#if defined(AT_UID)
                             AT_UID,
#endif
#if defined(AT_EUID)
                             AT_EUID,
#endif
#if defined(AT_GID)
                             AT_GID,
#endif
#if defined(AT_EGID)
                             AT_EGID,
#endif
#if defined(AT_CLKTCK)
                             AT_CLKTCK,
#endif
#if defined(AT_PLATFORM)
                             AT_PLATFORM,
#endif
#if defined(AT_SECURE)
                             AT_SECURE,
#endif
#if defined(AT_RANDOM)
                             AT_RANDOM,
#endif
#if defined(AT_SYSINFO_EHDR)
                             AT_SYSINFO_EHDR,
#endif
#if defined(AT_MINSIGSTKSZ)
                             AT_MINSIGSTKSZ,
#endif
                             // Ensure brackets are closed.
                             AT_HWCAP));

}  // namespace
}  // namespace nplb
}  // namespace starboard
