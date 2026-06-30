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

#include "starboard/shared/starboard/command_line_preprocessor.h"

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

TEST(CommandLinePreprocessorTest, ConvertBooleanDeprecatedSwitch) {
  CommandLine command_line({"app", "--dump_video_data"});
  ConvertDeprecatedSwitches(&command_line);

  EXPECT_FALSE(command_line.HasSwitch("dump_video_data"));
  EXPECT_TRUE(command_line.HasSwitch("enable-features"));
  EXPECT_EQ(command_line.GetSwitchValue("enable-features"), "DumpVideoData");
}

TEST(CommandLinePreprocessorTest, ConvertParameterizedDeprecatedSwitch) {
  CommandLine command_line({"app", "--maximum_drm_session_updates=5"});
  ConvertDeprecatedSwitches(&command_line);

  EXPECT_FALSE(command_line.HasSwitch("maximum_drm_session_updates"));
  EXPECT_TRUE(command_line.HasSwitch("enable-features"));
  EXPECT_EQ(command_line.GetSwitchValue("enable-features"),
            "LimitDrmSessionUpdates:MaximumDrmSessionUpdates/5");
}

TEST(CommandLinePreprocessorTest, PreservesExistingEnableFeatures) {
  CommandLine command_line(
      {"app", "--enable-features=custom_feature", "--dump_video_data"});
  ConvertDeprecatedSwitches(&command_line);

  EXPECT_FALSE(command_line.HasSwitch("dump_video_data"));
  std::string enabled = command_line.GetSwitchValue("enable-features");
  EXPECT_NE(enabled.find("custom_feature"), std::string::npos);
  EXPECT_NE(enabled.find("DumpVideoData"), std::string::npos);
}

}  // namespace
}  // namespace starboard
