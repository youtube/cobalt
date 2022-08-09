// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_WEB_TESTING_MOCK_USER_AGENT_PLATFORM_INFO_H_
#define COBALT_WEB_TESTING_MOCK_USER_AGENT_PLATFORM_INFO_H_

#include <string>

#include "cobalt/web/user_agent_platform_info.h"

namespace cobalt {
namespace web {
namespace testing {

// Mock class implementing UserAgentPlatformInfo returning empty strings.
class MockUserAgentPlatformInfo : public web::UserAgentPlatformInfo {
 public:
  MockUserAgentPlatformInfo() {}
  ~MockUserAgentPlatformInfo() override{};

  // From: dom:UserAgentPlatformInfo
  //
  const std::string& starboard_version() const override {
    return empty_string_;
  }
  const std::string& os_name_and_version() const override {
    return empty_string_;
  }
  base::Optional<std::string> original_design_manufacturer() const override {
    return optional_empty_string_;
  }
  SbSystemDeviceType device_type() const override {
    return kSbSystemDeviceTypeUnknown;
  }
  const std::string& device_type_string() const override {
    return empty_string_;
  }
  base::Optional<std::string> chipset_model_number() const override {
    return optional_empty_string_;
  }
  base::Optional<std::string> model_year() const override {
    return optional_empty_string_;
  }
  base::Optional<std::string> firmware_version() const override {
    return optional_empty_string_;
  }
  base::Optional<std::string> brand() const override {
    return optional_empty_string_;
  }
  base::Optional<std::string> model() const override {
    return optional_empty_string_;
  }
  const std::string& aux_field() const override { return empty_string_; }
  const std::string& javascript_engine_version() const override {
    return empty_string_;
  }
  const std::string& rasterizer_type() const override { return empty_string_; }
  const std::string& evergreen_type() const override { return empty_string_; }
  const std::string& evergreen_file_type() const override {
    return empty_string_;
  }
  const std::string& evergreen_version() const override {
    return empty_string_;
  }
  const std::string& cobalt_version() const override { return empty_string_; }
  const std::string& cobalt_build_version_number() const override {
    return empty_string_;
  }
  const std::string& build_configuration() const override {
    return empty_string_;
  }

 private:
  const std::string empty_string_;
  base::Optional<std::string> optional_empty_string_;
};  // namespace cobalt

}  // namespace testing
}  // namespace web
}  // namespace cobalt

#endif  // COBALT_WEB_TESTING_MOCK_USER_AGENT_PLATFORM_INFO_H_
