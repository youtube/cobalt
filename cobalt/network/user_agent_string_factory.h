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

#ifndef COBALT_NETWORK_USER_AGENT_STRING_FACTORY_H_
#define COBALT_NETWORK_USER_AGENT_STRING_FACTORY_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/optional.h"

namespace cobalt {
namespace network {

class UserAgentStringFactory {
 public:
  static scoped_ptr<UserAgentStringFactory> ForCurrentPlatform();
  virtual ~UserAgentStringFactory() {}

  // Creates user agent string for the current platform that follows the format
  // of Chromium's user agent, without pretending to be Chromium.
  // Optionally includes platform information specified in YouTube TV HTML5
  // Technical Requirements (2016), see
  // https://docs.google.com/document/d/1STWxTx4PMz0BQpB29EcJFPMMUwPrk8_BD6ky_N6moaI/edit#heading=h.f921oqp9td36.
  std::string CreateUserAgentString();

 protected:
  UserAgentStringFactory() {}

  std::string historic_platform_name_;
  std::string os_name_and_version_;
  std::string architecture_tokens_;
  std::string starboard_version_;
  std::string aux_field_;

  struct PlatformInfo {
    enum DeviceType {
      kInvalidDeviceType,
      kAndroidTV,
      kBlueRayDiskPlayer,
      kGameConsole,
      kOverTheTopBox,
      kSetTopBox,
      kTV,
    };

    enum ConnectionType {
      kWiredConnection,
      kWirelessConnection,
    };

    PlatformInfo() : device_type(kInvalidDeviceType) {}

    base::optional<std::string> network_operator;
    DeviceType device_type;
    base::optional<std::string> chipset_model_number;
    base::optional<std::string> firmware_version;
    std::string brand;
    std::string model;
    base::optional<ConnectionType> connection_type;
  };
  base::optional<PlatformInfo> platform_info_;

 private:
  std::string CreatePlatformString();
  std::string CreateDeviceTypeString();
  std::string CreateConnectionTypeString();

  DISALLOW_COPY_AND_ASSIGN(UserAgentStringFactory);
};

}  // namespace network
}  // namespace cobalt

#endif  // COBALT_NETWORK_USER_AGENT_STRING_FACTORY_H_
