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

#ifndef COBALT_DOM_USER_AGENT_PLATFORM_INFO_H_
#define COBALT_DOM_USER_AGENT_PLATFORM_INFO_H_

#include <string>

#include "base/optional.h"
#include "starboard/system.h"

namespace cobalt {
namespace dom {

// This class gives the DOM an interface to the user agent platform info. This
// allows NavigatorUAData to access user agent string info.
class UserAgentPlatformInfo {
 public:
  virtual ~UserAgentPlatformInfo() {}

  virtual const std::string& starboard_version() const = 0;
  virtual const std::string& os_name_and_version() const = 0;
  virtual base::Optional<std::string> original_design_manufacturer() const = 0;
  virtual SbSystemDeviceType device_type() const = 0;
  virtual const std::string& device_type_string() const = 0;
  virtual base::Optional<std::string> chipset_model_number() const = 0;
  virtual base::Optional<std::string> model_year() const = 0;
  virtual base::Optional<std::string> firmware_version() const = 0;
  virtual base::Optional<std::string> brand() const = 0;
  virtual base::Optional<std::string> model() const = 0;
  virtual const std::string& aux_field() const = 0;
  virtual base::Optional<SbSystemConnectionType> connection_type() const = 0;
  virtual const std::string& connection_type_string() const = 0;
  virtual const std::string& javascript_engine_version() const = 0;
  virtual const std::string& rasterizer_type() const = 0;
  virtual const std::string& evergreen_type() const = 0;
  virtual const std::string& evergreen_version() const = 0;

  virtual const std::string& cobalt_version() const = 0;
  virtual const std::string& cobalt_build_version_number() const = 0;
  virtual const std::string& build_configuration() const = 0;

 protected:
  UserAgentPlatformInfo() {}
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_USER_AGENT_PLATFORM_INFO_H_
