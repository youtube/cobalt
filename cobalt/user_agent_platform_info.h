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

#ifndef COBALT_USER_AGENT_PLATFORM_INFO_H_
#define COBALT_USER_AGENT_PLATFORM_INFO_H_

#include <map>
#include <string>

namespace cobalt {

void GetUserAgentInputMap(
    const std::string& user_agent_input,
    std::map<std::string, std::string>& user_agent_input_map);

class UserAgentPlatformInfo {
 public:
  explicit UserAgentPlatformInfo(bool enable_skia_rasterizer = false);
  ~UserAgentPlatformInfo() = default;

  std::string ToString() const;

  const std::string& starboard_version() const { return starboard_version_; }
  const std::string& os_name_and_version() const {
    return os_name_and_version_;
  }
  std::optional<std::string> original_design_manufacturer() const {
    return original_design_manufacturer_;
  }

  const std::string& device_type_string() const { return device_type_string_; }
  std::optional<std::string> chipset_model_number() const {
    return chipset_model_number_;
  }
  std::optional<std::string> model_year() const { return model_year_; }
  std::optional<std::string> firmware_version() const {
    return firmware_version_;
  }
  std::optional<std::string> brand() const { return brand_; }
  std::optional<std::string> model() const { return model_; }
  const std::string& aux_field() const { return aux_field_; }
  const std::string& javascript_engine_version() const {
    return javascript_engine_version_;
  }
  const std::string& rasterizer_type() const { return rasterizer_type_; }
  const std::string& evergreen_type() const { return evergreen_type_; }
  const std::string& evergreen_file_type() const {
    return evergreen_file_type_;
  }
  const std::string& evergreen_version() const { return evergreen_version_; }
  const std::string& android_build_fingerprint() const {
    return android_build_fingerprint_;
  }
  const std::string& android_os_experience() const {
    return android_os_experience_;
  }
  const std::string& android_play_services_version() const {
    return android_play_services_version_;
  }
  const std::string& cobalt_version() const { return cobalt_version_; }
  const std::string& cobalt_build_version_number() const {
    return cobalt_build_version_number_;
  }
  const std::string& build_configuration() const {
    return build_configuration_;
  }

  bool enable_skia_rasterizer() { return enable_skia_rasterizer_; }

  // Other: Setters that sanitize the strings where needed.
  //
  void set_starboard_version(const std::string& starboard_version);
  void set_os_name_and_version(const std::string& os_name_and_version);
  void set_original_design_manufacturer(
      std::optional<std::string> original_design_manufacturer);
  void set_device_type(const std::string& device_type);
  void set_chipset_model_number(
      std::optional<std::string> chipset_model_number);
  void set_model_year(std::optional<std::string> model_year);
  void set_firmware_version(std::optional<std::string> firmware_version);
  void set_brand(std::optional<std::string> brand);
  void set_model(std::optional<std::string> model);
  void set_aux_field(const std::string& aux_field);
  void set_javascript_engine_version(
      const std::string& javascript_engine_version);
  void set_rasterizer_type(const std::string& rasterizer_type);
  void set_evergreen_type(const std::string& evergreen_type);
  void set_evergreen_file_type(const std::string& evergreen_file_type);
  void set_evergreen_version(const std::string& evergreen_version);
  void set_android_build_fingerprint(
      const std::string& android_build_fingerprint);
  void set_android_os_experience(const std::string& android_os_experience);
  void set_android_play_services_version(
      const std::string& android_play_services_version);
  void set_cobalt_version(const std::string& cobalt_version);
  void set_cobalt_build_version_number(
      const std::string& cobalt_build_version_number);
  void set_build_configuration(const std::string& build_configuration);

 private:
  std::string starboard_version_;
  std::string os_name_and_version_;
  std::optional<std::string> original_design_manufacturer_;
  std::string device_type_string_;
  std::optional<std::string> chipset_model_number_;
  std::optional<std::string> model_year_;
  std::optional<std::string> firmware_version_;
  std::optional<std::string> brand_;
  std::optional<std::string> model_;
  std::string aux_field_;
  std::string javascript_engine_version_;
  std::string rasterizer_type_;
  std::string evergreen_type_;
  std::string evergreen_file_type_;
  std::string evergreen_version_;
  std::string android_build_fingerprint_;      // Only via Client Hints
  std::string android_os_experience_;          // Only via Client Hints
  std::string android_play_services_version_;  // Only via Client Hints

  std::string cobalt_version_;
  std::string cobalt_build_version_number_;
  std::string build_configuration_;

  const bool enable_skia_rasterizer_;
};

}  // namespace cobalt

#endif  // COBALT_USER_AGENT_PLATFORM_INFO_H_
