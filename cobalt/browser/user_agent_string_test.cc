// Copyright 2018 Google Inc. All Rights Reserved.
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

#include "cobalt/browser/user_agent_string.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace browser {

namespace {

UserAgentPlatformInfo CreateOnlyOSNameAndVersionPlatformInfo() {
  UserAgentPlatformInfo platform_info;
  platform_info.os_name_and_version = "GLaDOS 3.11";
  return platform_info;
}

TEST(UserAgentStringFactoryTest, StartsWithMozilla) {
  std::string user_agent_string =
      CreateUserAgentString(CreateOnlyOSNameAndVersionPlatformInfo());
  EXPECT_EQ(0, user_agent_string.find("Mozilla/5.0"));
}

TEST(UserAgentStringFactoryTest, ContainsCobalt) {
  std::string user_agent_string =
      CreateUserAgentString(CreateOnlyOSNameAndVersionPlatformInfo());
  EXPECT_NE(std::string::npos, user_agent_string.find("Cobalt"));
}

TEST(UserAgentStringFactoryTest, WithOSNameAndVersion) {
  std::string user_agent_string =
      CreateUserAgentString(CreateOnlyOSNameAndVersionPlatformInfo());
  EXPECT_NE(std::string::npos, user_agent_string.find("(GLaDOS 3.11)"));
}

TEST(UserAgentStringFactoryTest, WithCobaltVersionAndConfiguration) {
  UserAgentPlatformInfo platform_info;
  platform_info.os_name_and_version = "GLaDOS 3.11";
  platform_info.cobalt_version = "16";
  platform_info.cobalt_build_version_number = "123456";
  platform_info.build_configuration = "gold";
  std::string user_agent_string = CreateUserAgentString(platform_info);

  const char* tv_info_str = "Cobalt/16.123456-gold (unlike Gecko)";
  EXPECT_NE(std::string::npos, user_agent_string.find(tv_info_str));
}

// Look-alike replacements expected from sanitizing fields
#define COMMA u8"\uFF0C"   // fullwidth comma
#define UNDER u8"\u2E0F"   // paragraphos
#define SLASH u8"\u2215"   // division slash
#define LPAREN u8"\uFF08"  // fullwidth left paren
#define RPAREN u8"\uFF09"  // fullwidth right paren

TEST(UserAgentStringFactoryTest, WithPlatformInfo) {
  // There are deliberately a variety of underscores, commas, slashes, and
  // parentheses in the strings below to ensure they get sanitized.
  UserAgentPlatformInfo platform_info;
  platform_info.os_name_and_version = "GLaDOS 3.11";
  platform_info.network_operator = "Aperture_Science_Innovators";
  platform_info.device_type = kSbSystemDeviceTypeOverTheTopBox;
  platform_info.chipset_model_number = "P-body/Orange_Atlas/Blue";
  platform_info.model_year = "2013";
  platform_info.firmware_version = "0,01";
  platform_info.brand = "Aperture Science (Labs)";
  platform_info.model = "GLaDOS";
  std::string user_agent_string = CreateUserAgentString(platform_info);

  const char* tv_info_str =
      "Aperture" UNDER "Science" UNDER
      "Innovators"
      "_OTT_"
      "P-body" SLASH "Orange" UNDER "Atlas" SLASH
      "Blue"
      "_2013"
      "/0" COMMA "01 (Aperture Science " LPAREN "Labs" RPAREN ", GLaDOS, )";
  EXPECT_NE(std::string::npos, user_agent_string.find(tv_info_str));
}

TEST(UserAgentStringFactoryTest, WithWiredConnection) {
  UserAgentPlatformInfo platform_info;
  platform_info.os_name_and_version = "GLaDOS 3.11";
  platform_info.connection_type = kSbSystemConnectionTypeWired;
  platform_info.device_type = kSbSystemDeviceTypeOverTheTopBox;
  std::string user_agent_string = CreateUserAgentString(platform_info);

  EXPECT_NE(std::string::npos, user_agent_string.find("Wired"));
}

TEST(UserAgentStringFactoryTest, WithWirelessConnection) {
  UserAgentPlatformInfo platform_info;
  platform_info.os_name_and_version = "GLaDOS 3.11";
  platform_info.connection_type = kSbSystemConnectionTypeWireless;
  platform_info.device_type = kSbSystemDeviceTypeOverTheTopBox;
  std::string user_agent_string = CreateUserAgentString(platform_info);

  EXPECT_NE(std::string::npos, user_agent_string.find("Wireless"));
}

TEST(UserAgentStringFactoryTest, WithOnlyBrandModelAndDeviceType) {
  UserAgentPlatformInfo platform_info;
  platform_info.os_name_and_version = "GLaDOS 3.11";
  platform_info.device_type = kSbSystemDeviceTypeOverTheTopBox;
  platform_info.brand = "Aperture Science";
  platform_info.model = "GLaDOS";
  std::string user_agent_string = CreateUserAgentString(platform_info);

  const char* tv_info_str = ", _OTT__/ (Aperture Science, GLaDOS, )";
  EXPECT_NE(std::string::npos, user_agent_string.find(tv_info_str));
}

TEST(UserAgentStringFactoryTest, WithStarboardVersion) {
  UserAgentPlatformInfo platform_info;
  platform_info.starboard_version = "Starboard/6";
  platform_info.device_type = kSbSystemDeviceTypeOverTheTopBox;
  std::string user_agent_string = CreateUserAgentString(platform_info);

  const char* tv_info_str = "Starboard/6, _OTT__/ (, , )";
  EXPECT_NE(std::string::npos, user_agent_string.find(tv_info_str));
}

TEST(UserAgentStringFactoryTest, WithJavaScriptVersion) {
  UserAgentPlatformInfo platform_info;
  platform_info.starboard_version = "Starboard/6";
  platform_info.javascript_engine_version = "V8/6.5.254.28";
  platform_info.device_type = kSbSystemDeviceTypeOverTheTopBox;
  std::string user_agent_string = CreateUserAgentString(platform_info);

  const char* tv_info_str = "V8/6.5.254.28 Starboard/6, _OTT__/ (, , )";
  EXPECT_NE(std::string::npos, user_agent_string.find(tv_info_str));
}

}  // namespace

}  // namespace browser
}  // namespace cobalt
