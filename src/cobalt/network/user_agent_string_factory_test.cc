// Copyright 2016 Google Inc. All Rights Reserved.
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

#include "cobalt/network/user_agent_string_factory.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace network {

namespace {

class UserAgentStringFactoryWithOSNameAndVerison
    : public UserAgentStringFactory {
 public:
  UserAgentStringFactoryWithOSNameAndVerison() {
    os_name_and_version_ = "GLaDOS 3.11";
  }
};

TEST(UserAgentStringFactoryTest, StartsWithMozilla) {
  std::string user_agent_string =
      UserAgentStringFactoryWithOSNameAndVerison().CreateUserAgentString();
  EXPECT_EQ(0, user_agent_string.find("Mozilla/5.0"));
}

TEST(UserAgentStringFactoryTest, ContainsCobalt) {
  std::string user_agent_string =
      UserAgentStringFactoryWithOSNameAndVerison().CreateUserAgentString();
  EXPECT_NE(std::string::npos, user_agent_string.find("Cobalt"));
}

TEST(UserAgentStringFactoryTest, WithOSNameAndVersion) {
  std::string user_agent_string =
      UserAgentStringFactoryWithOSNameAndVerison().CreateUserAgentString();
  EXPECT_NE(std::string::npos, user_agent_string.find("(GLaDOS 3.11)"));
}

class UserAgentStringFactoryWithHistoricPlatformName
    : public UserAgentStringFactory {
 public:
  UserAgentStringFactoryWithHistoricPlatformName() {
    historic_platform_name_ = "Caroline";
    os_name_and_version_ = "GLaDOS 3.11";
  }
};

TEST(UserAgentStringFactoryTest, WithHistoricPlatformName) {
  std::string user_agent_string =
      UserAgentStringFactoryWithHistoricPlatformName().CreateUserAgentString();
  EXPECT_NE(std::string::npos,
            user_agent_string.find("(Caroline; GLaDOS 3.11)"));
}

class UserAgentStringFactoryWithArchitectureTokens
    : public UserAgentStringFactory {
 public:
  UserAgentStringFactoryWithArchitectureTokens() {
    os_name_and_version_ = "GLaDOS 3.11";
    architecture_tokens_ = "Wheatley";
  }
};

TEST(UserAgentStringFactoryTest, WithArchitectureTokens) {
  std::string user_agent_string =
      UserAgentStringFactoryWithArchitectureTokens().CreateUserAgentString();
  EXPECT_NE(std::string::npos,
            user_agent_string.find("(GLaDOS 3.11; Wheatley)"));
}

class UserAgentStringFactoryWithPlatformInfo : public UserAgentStringFactory {
 public:
  UserAgentStringFactoryWithPlatformInfo() {
    // There are deliberately a variety of underscores, commas, slashes, and
    // parentheses in the strings below to ensure they get sanitized.
    os_name_and_version_ = "GLaDOS 3.11";
    platform_info_ = PlatformInfo();
    platform_info_->network_operator = "Aperture_Science_Innovators";
    platform_info_->device_type = PlatformInfo::kOverTheTopBox;
    platform_info_->chipset_model_number = "P-body/Orange_Atlas/Blue";
    platform_info_->firmware_version = "0,01";
    platform_info_->brand = "Aperture Science (Labs)";
    platform_info_->model = "GLaDOS";
  }
};

// Look-alike replacements expected from sanitizing fields
#define COMMA  u8"\uFF0C"  // fullwidth comma
#define UNDER  u8"\u2E0F"  // paragraphos
#define SLASH  u8"\u2215"  // division slash
#define LPAREN u8"\uFF08"  // fullwidth left paren
#define RPAREN u8"\uFF09"  // fullwidth right paren

TEST(UserAgentStringFactoryTest, WithPlatformInfo) {
  std::string user_agent_string =
      UserAgentStringFactoryWithPlatformInfo().CreateUserAgentString();
  const char* tv_info_str =
      "Aperture" UNDER "Science" UNDER "Innovators"
      "_OTT_"
      "P-body" SLASH "Orange" UNDER "Atlas" SLASH "Blue"
      "/0" COMMA "01 (Aperture Science " LPAREN "Labs" RPAREN ", GLaDOS, )";
  EXPECT_NE(std::string::npos, user_agent_string.find(tv_info_str));
}

class UserAgentStringFactoryWithWiredConnection
    : public UserAgentStringFactory {
 public:
  UserAgentStringFactoryWithWiredConnection() {
    os_name_and_version_ = "GLaDOS 3.11";
    platform_info_ = PlatformInfo();
    platform_info_->connection_type = PlatformInfo::kWiredConnection;
    platform_info_->device_type = PlatformInfo::kOverTheTopBox;
  }
};

TEST(UserAgentStringFactoryTest, WithWiredConnection) {
  std::string user_agent_string =
      UserAgentStringFactoryWithWiredConnection().CreateUserAgentString();
  EXPECT_NE(std::string::npos, user_agent_string.find("Wired"));
}

class UserAgentStringFactoryWithWirelessConnection
    : public UserAgentStringFactory {
 public:
  UserAgentStringFactoryWithWirelessConnection() {
    os_name_and_version_ = "GLaDOS 3.11";
    platform_info_ = PlatformInfo();
    platform_info_->connection_type = PlatformInfo::kWirelessConnection;
    platform_info_->device_type = PlatformInfo::kOverTheTopBox;
  }
};

TEST(UserAgentStringFactoryTest, WithWirelessConnection) {
  std::string user_agent_string =
      UserAgentStringFactoryWithWirelessConnection().CreateUserAgentString();
  EXPECT_NE(std::string::npos, user_agent_string.find("Wireless"));
}

}  // namespace

}  // namespace network
}  // namespace cobalt
