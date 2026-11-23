// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/browser_launch/browser_launch_data_collector_desktop.h"

#include <string>

#include "base/command_line.h"
#include "base/process/process.h"
#include "chrome/browser/enterprise/reporting/browser_launch/scoped_initial_command_line.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_reporting {

class BrowserLaunchDataCollectorDesktopTest : public testing::Test {
 public:
  BrowserLaunchDataCollectorDesktopTest() = default;
  ~BrowserLaunchDataCollectorDesktopTest() override = default;

 protected:
  base::CommandLine stubbed_cli_{base::CommandLine::NO_PROGRAM};
};

TEST_F(BrowserLaunchDataCollectorDesktopTest, GetEvent) {
  // Configure the command line for this specific test case.
  stubbed_cli_.AppendSwitch("switch-1");
  stubbed_cli_.AppendSwitchASCII("switch-2", "value-2");
  stubbed_cli_.AppendSwitchASCII("switch-3", "value-3");

  ScopedInitialCommandLine scoped_cli(&stubbed_cli_);

  BrowserLaunchDataCollectorDesktop collector;
  auto&& event = collector.GetEvent();

  // Verify that all 3 switch keys are captured exactly using GTest matchers.
  EXPECT_THAT(
      event.command_line_switch_keys(),
      testing::UnorderedElementsAre("switch-1", "switch-2", "switch-3"));

  // Verify launch time matches the system process creation time exactly.
  int64_t expected_time_ms =
      base::Process::Current().CreationTime().InMillisecondsSinceUnixEpoch();
  EXPECT_EQ(event.launch_time_millis(), expected_time_ms);
}

}  // namespace enterprise_reporting
