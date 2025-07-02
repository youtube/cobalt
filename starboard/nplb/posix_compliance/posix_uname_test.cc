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
#include <sys/utsname.h>

#include <string>
#include "starboard/common/log.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

class PosixUnameTest : public ::testing::Test {
 protected:
  PosixUnameTest() = default;

  void SetUp() override { errno = 0; }
};

// Verify that uname() executes successfully and populates the struct.
TEST_F(PosixUnameTest, BasicUnameCall) {
  struct utsname name;
  int result = uname(&name);

  ASSERT_GE(0, result) << "uname() failed with error: " << strerror(errno);
  EXPECT_FALSE(std::string(name.sysname).empty());
  EXPECT_FALSE(std::string(name.nodename).empty());
  EXPECT_FALSE(std::string(name.release).empty());
  EXPECT_FALSE(std::string(name.version).empty());
  EXPECT_FALSE(std::string(name.machine).empty());
  EXPECT_FALSE(std::string(name.domainname).empty());
  EXPECT_EQ(0, errno) << "errno was set to " << errno << " (" << strerror(errno)
                      << ") after successful uname call.";

  SB_LOG(INFO) << "sysname: " << name.sysname;
  SB_LOG(INFO) << "nodename: " << name.nodename;
  SB_LOG(INFO) << "release: " << name.release;
  SB_LOG(INFO) << "version: " << name.version;
  SB_LOG(INFO) << "machine: " << name.machine;
  SB_LOG(INFO) << "domainname: " << name.domainname;
}

// Ensure that passing a nullptr to uname() sets errno to EFAULT.
TEST_F(PosixUnameTest, HandlesNullPointer) {
  int result = uname(nullptr);

  EXPECT_EQ(-1, result) << "uname() with nullptr did not return -1.";
  EXPECT_EQ(EFAULT, errno) << "Expected errno to be EFAULT, but got " << errno
                           << " (" << strerror(errno) << ").";
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
