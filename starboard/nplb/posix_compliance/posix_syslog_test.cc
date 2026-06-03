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

#include <syslog.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

TEST(PosixSyslogTest, CanCallFunctions) {
  // This test mainly ensures that the syslog functions can be called without
  // causing a crash. It doesn't verify the output, as that would require
  // more complex test setup (e.g., redirecting stderr or checking system logs).

  // The functions should be callable without any setup.
  syslog(LOG_INFO, "This is a test message from %s.", "PosixSyslogTest");

  // Call openlog, which should set the identifier for subsequent syslog calls.
  openlog("MyTestProgram", LOG_PID | LOG_CONS, LOG_USER);

  // Log another message.
  syslog(LOG_WARNING, "Another test message.");

  // And a final one.
  syslog(LOG_ERR, "And a final test message with value %d.", 123);

  // Close the log.
  closelog();

  // All calls should complete without crashing.
  SUCCEED();
}

}  // namespace
}  // namespace nplb
