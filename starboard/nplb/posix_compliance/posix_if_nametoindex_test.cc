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

TEST(PosixIfNametoindexTest, InvalidName) {
  unsigned int index = if_nametoindex("nonexistent_interface");
  EXPECT_EQ(index, 0u);
}

TEST(PosixIfNametoindexTest, EmptyName) {
  unsigned int index = if_nametoindex("");
  EXPECT_EQ(index, 0u);
}

TEST(PosixIfNametoindexTest, ValidNameLoopback) {
  // Loopback is named "lo" on almost all Linux systems and "lo0" on Mac.
  const char* loopback_name = "lo";
  unsigned int index = if_nametoindex(loopback_name);
  if (index == 0) {
    loopback_name = "lo0";
    index = if_nametoindex(loopback_name);
  }
  EXPECT_NE(index, 0u);
}

TEST(PosixIfNametoindexTest, MatchesIfIndextoname) {
  // Loopback is named "lo" on almost all Linux systems and "lo0" on Mac.
  const char* loopback_name = "lo";
  unsigned int index = if_nametoindex(loopback_name);
  if (index == 0) {
    loopback_name = "lo0";
    index = if_nametoindex(loopback_name);
  }
  ASSERT_NE(index, 0u);
  char buf[IF_NAMESIZE];
  EXPECT_STREQ(if_indextoname(index, buf), loopback_name);
}

}  // namespace
}  // namespace nplb
