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
#include <ifaddrs.h>
#include <net/if.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

class PosixNetIfTest : public ::testing::Test {
 public:
  PosixNetIfTest() { errno = 0; }
  ~PosixNetIfTest() override {
    if (ifs != nullptr) {
      freeifaddrs(ifs);
    }
  }

 protected:
  char buf[IF_NAMESIZE];
  struct ifaddrs* ifs = nullptr;
};

TEST_F(PosixNetIfTest, InvalidName) {
  unsigned int index = if_nametoindex("nonexistent_interface");
  EXPECT_EQ(index, 0u);
}

TEST_F(PosixNetIfTest, EmptyName) {
  unsigned int index = if_nametoindex("");
  EXPECT_EQ(index, 0u);
}

TEST_F(PosixNetIfTest, InvalidIndex) {
  char* name = if_indextoname(0, buf);
  EXPECT_EQ(name, nullptr);
  EXPECT_EQ(errno, ENXIO);
}

TEST_F(PosixNetIfTest, LargeInvalidIndex) {
  // Use a very large index that is unlikely to exist.
  char* name = if_indextoname(0xFFFFFFFF, buf);
  EXPECT_EQ(name, nullptr);
  EXPECT_EQ(errno, ENXIO);
}

TEST_F(PosixNetIfTest, AllValidIndices) {
  ASSERT_GE(getifaddrs(&ifs), 0);
  ASSERT_NE(ifs, nullptr);
  ASSERT_EQ(errno, 0);

  for (struct ifaddrs* ifa = ifs; ifa != nullptr; ifa = ifa->ifa_next) {
    errno = 0;
    unsigned int index = if_nametoindex(ifa->ifa_name);
    EXPECT_NE(index, 0u);
    EXPECT_EQ(errno, 0);

    errno = 0;
    char* name = if_indextoname(index, buf);
    EXPECT_EQ(name, buf);
    EXPECT_EQ(errno, 0);
    EXPECT_STREQ(name, ifa->ifa_name);
  }
}

}  // namespace
}  // namespace nplb
