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
#include <net/if.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

TEST(PosixIfIndextonameTest, InvalidIndex) {
  char buf[IF_NAMESIZE];
  errno = 0;
  char* name = if_indextoname(0, buf);
  EXPECT_EQ(name, nullptr);
  EXPECT_EQ(errno, ENXIO);
}

TEST(PosixIfIndextonameTest, LargeInvalidIndex) {
  char buf[IF_NAMESIZE];
  errno = 0;
  // Use a very large index that is unlikely to exist.
  char* name = if_indextoname(0xFFFFFFFF, buf);
  EXPECT_EQ(name, nullptr);
  EXPECT_EQ(errno, ENXIO);
}

TEST(PosixIfIndextonameTest, ValidIndexLoopback) {
  char buf[IF_NAMESIZE];
  // Loopback is named "lo" on almost all Linux systems and "lo0" on Mac.
  const char* loopback_name = "lo";
  unsigned int index = if_nametoindex(loopback_name);
  if (index == 0) {
    loopback_name = "lo0";
    index = if_nametoindex(loopback_name);
  }
  ASSERT_NE(index, 0u);

  errno = 0;
  char* name = if_indextoname(index, buf);
  EXPECT_NE(name, nullptr);
  EXPECT_STREQ(name, loopback_name);
  EXPECT_EQ(name, buf);
  EXPECT_EQ(errno, 0);
}

}  // namespace
}  // namespace nplb
