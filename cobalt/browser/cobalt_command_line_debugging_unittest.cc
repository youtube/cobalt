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

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "cobalt/browser/command_line_debugging_test_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace {

TEST(CommandLineDebuggingTest, NoSwitches) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  EXPECT_EQ("", FormatCommandLineSwitchesForDebugging(command_line));
}

TEST(CommandLineDebuggingTest, SingleSwitchNoValue) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch("test-switch");
  EXPECT_EQ("'test-switch': ''",
            FormatCommandLineSwitchesForDebugging(command_line));
}

TEST(CommandLineDebuggingTest, SingleSwitchWithValue) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII("test-switch", "value");
  EXPECT_EQ("'test-switch': 'value'",
            FormatCommandLineSwitchesForDebugging(command_line));
}

TEST(CommandLineDebuggingTest, MultipleSwitches) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch("switch1");
  command_line.AppendSwitchASCII("switch2", "value2");
  command_line.AppendSwitch("switch3");

  std::string formatted_switches =
      FormatCommandLineSwitchesForDebugging(command_line);
  EXPECT_TRUE(formatted_switches.find("'switch1': ''") != std::string::npos);
  EXPECT_TRUE(formatted_switches.find("'switch2': 'value2'") !=
              std::string::npos);
  EXPECT_TRUE(formatted_switches.find("'switch3': ''") != std::string::npos);
}

TEST(CommandLineDebuggingTest, SwitchesWithSpacesAndQuotes) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII("path", "/usr/local/bin/my app");
  command_line.AppendSwitchASCII("message", "Hello World");

  // Note: CommandLine::AppendSwitchASCII handles internal quoting if needed.
  std::string formatted_switches =
      FormatCommandLineSwitchesForDebugging(command_line);
  EXPECT_TRUE(formatted_switches.find("'path': '/usr/local/bin/my app'") !=
              std::string::npos);
  EXPECT_TRUE(formatted_switches.find("'message': 'Hello World'") !=
              std::string::npos);
}

}  // namespace
}  // namespace cobalt
