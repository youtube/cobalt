// Copyright 2021 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BROWSER_USER_AGENT_PLATFORM_INFO_H_
#define COBALT_BROWSER_USER_AGENT_PLATFORM_INFO_H_

#include <map>
#include <string>

#include "cobalt/dom/user_agent_platform_info.h"

namespace cobalt {
namespace browser {

void GetUserAgentInputMap(
    const std::string& user_agent_input,
    std::map<std::string, std::string>& user_agent_input_map);

class UserAgentPlatformInfo : public dom::UserAgentPlatformInfo {
 public:
  UserAgentPlatformInfo();
  ~UserAgentPlatformInfo() override{};

  // From: dom:UserAgentPlatformInfo
  //
  const std::string& starboard_version() const override {
    return starboard_version_;
  }
  const std::string& os_name_and_version() const override {
    return os_name_and_version_;
  }
  base::Optional<std::string> original_design_manufacturer() const override {
    return original_design_manufacturer_;
  }
  SbSystemDeviceType device_type() const override { return device_type_; }
  const std::string& device_type_string() const override {
    return device_type_string_;
  }
  base::Optional<std::string> chipset_model_number() const override {
    return chipset_model_number_;
  }
  base::Optional<std::string> model_year() const override {
    return model_year_;
  }
  base::Optional<std::string> firmware_version() const override {
    return firmware_version_;
  }
  base::Optional<std::string> brand() const override { return brand_; }
  base::Optional<std::string> model() const override { return model_; }
  const std::string& aux_field() const override { return aux_field_; }
  base::Optional<SbSystemConnectionType> connection_type() const override {
    return connection_type_;
  }
  const std::string& connection_type_string() const override {
    return connection_type_string_;
  }
  const std::string& javascript_engine_version() const override {
    return javascript_engine_version_;
  }
  const std::string& rasterizer_type() const override {
    return rasterizer_type_;
  }
  const std::string& evergreen_type() const override { return evergreen_type_; }
  const std::string& evergreen_version() const override {
    return evergreen_version_;
  }
  const std::string& cobalt_version() const override { return cobalt_version_; }
  const std::string& cobalt_build_version_number() const override {
    return cobalt_build_version_number_;
  }
  const std::string& build_configuration() const override {
    return build_configuration_;
  }

  // Other: Setters that sanitize the strings where needed.
  //
  void set_starboard_version(const std::string& starboard_version);
  void set_os_name_and_version(const std::string& os_name_and_version);
  void set_original_design_manufacturer(
      base::Optional<std::string> original_design_manufacturer);
  void set_device_type(SbSystemDeviceType device_type);
  void set_chipset_model_number(
      base::Optional<std::string> chipset_model_number);
  void set_model_year(base::Optional<std::string> model_year);
  void set_firmware_version(base::Optional<std::string> firmware_version);
  void set_brand(base::Optional<std::string> brand);
  void set_model(base::Optional<std::string> model);
  void set_aux_field(const std::string& aux_field);
  void set_connection_type(
      base::Optional<SbSystemConnectionType> connection_type);
  void set_javascript_engine_version(
      const std::string& javascript_engine_version);
  void set_rasterizer_type(const std::string& rasterizer_type);
  void set_evergreen_type(const std::string& evergreen_type);
  void set_evergreen_version(const std::string& evergreen_version);
  void set_cobalt_version(const std::string& cobalt_version);
  void set_cobalt_build_version_number(
      const std::string& cobalt_build_version_number);
  void set_build_configuration(const std::string& build_configuration);

 private:
  std::string starboard_version_;
  std::string os_name_and_version_;
  base::Optional<std::string> original_design_manufacturer_;
  SbSystemDeviceType device_type_ = kSbSystemDeviceTypeUnknown;
  std::string device_type_string_;
  base::Optional<std::string> chipset_model_number_;
  base::Optional<std::string> model_year_;
  base::Optional<std::string> firmware_version_;
  base::Optional<std::string> brand_;
  base::Optional<std::string> model_;
  std::string aux_field_;
  base::Optional<SbSystemConnectionType> connection_type_;
  std::string connection_type_string_;
  std::string javascript_engine_version_;
  std::string rasterizer_type_;
  std::string evergreen_type_;
  std::string evergreen_version_;

  std::string cobalt_version_;
  std::string cobalt_build_version_number_;
  std::string build_configuration_;
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_USER_AGENT_PLATFORM_INFO_H_
