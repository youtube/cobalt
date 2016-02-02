/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

class UserAgentStringFactoryWithYouTubeTVInfo : public UserAgentStringFactory {
 public:
  UserAgentStringFactoryWithYouTubeTVInfo() {
    os_name_and_version_ = "GLaDOS 3.11";
    youtube_tv_info_ = YouTubeTVInfo();
    youtube_tv_info_->network_operator = "ApertureLaboratories";
    youtube_tv_info_->device_type = YouTubeTVInfo::kOverTheTopBox;
    youtube_tv_info_->chipset_model_number = "Wheatley";
    youtube_tv_info_->firmware_version = "0.01";
    youtube_tv_info_->brand = "Aperture Science";
    youtube_tv_info_->model = "GLaDOS";
  }
};

TEST(UserAgentStringFactoryTest, WithYouTubeTVInfo) {
  std::string user_agent_string =
      UserAgentStringFactoryWithYouTubeTVInfo().CreateUserAgentString();
  EXPECT_NE(std::string::npos,
            user_agent_string.find("ApertureLaboratories_OTT_Wheatley/0.01 "
                                   "(Aperture Science, GLaDOS, )"));
}

class UserAgentStringFactoryWithWiredConnection
    : public UserAgentStringFactory {
 public:
  UserAgentStringFactoryWithWiredConnection() {
    os_name_and_version_ = "GLaDOS 3.11";
    youtube_tv_info_ = YouTubeTVInfo();
    youtube_tv_info_->connection_type = YouTubeTVInfo::kWiredConnection;
    youtube_tv_info_->device_type = YouTubeTVInfo::kOverTheTopBox;
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
    youtube_tv_info_ = YouTubeTVInfo();
    youtube_tv_info_->connection_type = YouTubeTVInfo::kWirelessConnection;
    youtube_tv_info_->device_type = YouTubeTVInfo::kOverTheTopBox;
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
