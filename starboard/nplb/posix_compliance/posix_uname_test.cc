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

class PosixUnameTest : public ::testing::Test {
 protected:
  PosixUnameTest() {}

  void SetUp() override { errno = 0; }

  void TearDown() override {}
};

// Verify that uname() executes successfully and populates the struct.
TEST_F(PosixUnameTest, BasicUnameCall) {
  struct utsname name;
  int result = uname(&name);

  ASSERT_EQ(0, result) << "uname() failed with error: " << strerror(errno);
  EXPECT_EQ(0, errno) << "errno was set to " << errno << " (" << strerror(errno)
                      << ") after successful uname call.";

  EXPECT_FALSE(std::string(name.sysname).empty()) << "sysname is empty.";
  EXPECT_FALSE(std::string(name.nodename).empty()) << "nodename is empty.";
  EXPECT_FALSE(std::string(name.release).empty()) << "release is empty.";
  EXPECT_FALSE(std::string(name.version).empty()) << "version is empty.";
  EXPECT_FALSE(std::string(name.machine).empty()) << "machine is empty.";

  // Optional: Print the uname information for debugging/informational purposes.
  // This is similar to how SB_LOG(INFO) was used in the isatty test.
  SB_LOG(INFO) << "uname() output:";
  SB_LOG(INFO) << "  Sysname:  " << name.sysname;
  SB_LOG(INFO) << "  Nodename: " << name.nodename;
  SB_LOG(INFO) << "  Release:  " << name.release;
  SB_LOG(INFO) << "  Version:  " << name.version;
  SB_LOG(INFO) << "  Machine:  " << name.machine;
}

// Ensure that passing a nullptr to uname() sets errno to EFAULT.
TEST_F(PosixUnameTest, HandlesNullPointer) {
  int result = uname(nullptr);

  EXPECT_EQ(-1, result) << "uname() with nullptr did not return -1.";
  EXPECT_EQ(EFAULT, errno) << "Expected errno to be EFAULT, but got " << errno
                           << " (" << strerror(errno) << ").";
}

// Ensure that utsname fields are null-termnimated.
TEST_F(PosixUnameTest, CheckForNullTerminatedFields) {
  struct utsname name;
  int result = uname(&name);

  ASSERT_EQ(0, result) << "uname() failed with error: " << strerror(errno);
  ASSERT_EQ(0, errno) << "errno was set to " << errno << " (" << strerror(errno)
                      << ") after successful uname call.";

  EXPECT_EQ('\0', name.sysname[sizeof(name.sysname) - 1])
      << "expected utsname sysname to be null-terminated, got "
      << name.sysname[sizeof(name.sysname) - 1];
}
