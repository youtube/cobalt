// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_timeouts.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/test/test_switches.h"

namespace {

// Sets value to the greatest of:
// 1) value's current value.
// 2) min_value.
// 3) the numerical value given by switch_name on the command line.
void InitializeTimeout(const char* switch_name, int min_value, int* value) {
  DCHECK(value);
  if (CommandLine::ForCurrentProcess()->HasSwitch(switch_name)) {
    std::string string_value(
        CommandLine::ForCurrentProcess()->GetSwitchValueASCII(switch_name));
    int timeout;
    base::StringToInt(string_value, &timeout);
    *value = std::max(*value, timeout);
  }
  *value = std::max(*value, min_value);
}

// Sets value to the greatest of:
// 1) value's current value.
// 2) 0
// 3) the numerical value given by switch_name on the command line.
void InitializeTimeout(const char* switch_name, int* value) {
  InitializeTimeout(switch_name, 0, value);
}

}  // namespace

// static
bool TestTimeouts::initialized_ = false;

// The timeout values should increase in the order they appear in this block.
// static
int TestTimeouts::tiny_timeout_ms_ = 100;
int TestTimeouts::action_timeout_ms_ = 2000;
int TestTimeouts::action_max_timeout_ms_ = 20000;
int TestTimeouts::large_test_timeout_ms_ = 3 * 60 * 1000;
int TestTimeouts::huge_test_timeout_ms_ = 10 * 60 * 1000;

// static
int TestTimeouts::command_execution_timeout_ms_ = 25000;

// static
int TestTimeouts::wait_for_terminate_timeout_ms_ = 15000;

// static
int TestTimeouts::live_operation_timeout_ms_ = 45000;

// static
void TestTimeouts::Initialize() {
  if (initialized_) {
    NOTREACHED();
    return;
  }
  initialized_ = true;

  // Note that these timeouts MUST be initialized in the correct order as
  // per the CHECKS below.
  InitializeTimeout(switches::kTestTinyTimeout, &tiny_timeout_ms_);
  InitializeTimeout(switches::kUiTestActionTimeout, tiny_timeout_ms_,
                    &action_timeout_ms_);
  InitializeTimeout(switches::kUiTestActionMaxTimeout, action_timeout_ms_,
                    &action_max_timeout_ms_);
  InitializeTimeout(switches::kTestLargeTimeout, action_max_timeout_ms_,
                    &large_test_timeout_ms_);
  InitializeTimeout(switches::kUiTestTimeout, large_test_timeout_ms_,
                    &huge_test_timeout_ms_);

  // The timeout values should be increasing in the right order.
  CHECK(tiny_timeout_ms_ <= action_timeout_ms_);
  CHECK(action_timeout_ms_ <= action_max_timeout_ms_);
  CHECK(action_max_timeout_ms_ <= large_test_timeout_ms_);
  CHECK(large_test_timeout_ms_ <= huge_test_timeout_ms_);

  InitializeTimeout(switches::kUiTestCommandExecutionTimeout,
                    &command_execution_timeout_ms_);
  InitializeTimeout(switches::kUiTestTerminateTimeout,
                    &wait_for_terminate_timeout_ms_);

  InitializeTimeout(switches::kLiveOperationTimeout,
                    &live_operation_timeout_ms_);
}
