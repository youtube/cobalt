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

#include "cobalt/browser/command_line_logger.h"

#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace {

TEST(CommandLineLoggerTest, NoSwitches) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  EXPECT_EQ("", CommandLineSwitchesToString(command_line));
}

TEST(CommandLineLoggerTest, SingleSwitchNoValue) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch("test-switch");
  EXPECT_EQ("'test-switch': ''", CommandLineSwitchesToString(command_line));
}

TEST(CommandLineLoggerTest, SingleSwitchWithValue) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII("test-switch", "value");
  EXPECT_EQ("'test-switch': 'value'",
            CommandLineSwitchesToString(command_line));
}

TEST(CommandLineLoggerTest, MultipleSwitches) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitch("switch1");
  command_line.AppendSwitchASCII("switch2", "value2");
  command_line.AppendSwitch("switch3");

  std::string formatted_switches = CommandLineSwitchesToString(command_line);
  std::vector<std::string> parts =
      base::SplitString(formatted_switches, "\n", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  EXPECT_THAT(parts,
              testing::UnorderedElementsAre(
                  "'switch1': ''", "'switch2': 'value2'", "'switch3': ''"));
}

TEST(CommandLineLoggerTest, SwitchesWithSpacesAndQuotes) {
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  command_line.AppendSwitchASCII("path", "/usr/local/bin/my app");
  command_line.AppendSwitchASCII("message", "Hello World");

  // Note: CommandLine::AppendSwitchASCII handles internal quoting if needed.
  std::string formatted_switches = CommandLineSwitchesToString(command_line);
  std::vector<std::string> parts =
      base::SplitString(formatted_switches, "\n", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_NONEMPTY);
  EXPECT_THAT(parts,
              testing::UnorderedElementsAre("'path': '/usr/local/bin/my app'",
                                            "'message': 'Hello World'"));
}

}  // namespace
}  // namespace cobalt
