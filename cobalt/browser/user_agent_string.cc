// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#include <vector>

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "cobalt/browser/switches.h"
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/system.h"

namespace cobalt {
namespace browser {
namespace {

const char kUnknownFieldName[] = "Unknown";

}  // namespace

UserAgentPlatformInfo GetUserAgentPlatformInfoFromSystem() {
  UserAgentPlatformInfo platform_info;
  return platform_info;
}

// This function is expected to be deterministic and non-dependent on global
// variables and state.  If global state should be referenced, it should be done
// so during the creation of |platform_info| instead.
std::string CreateUserAgentString(const UserAgentPlatformInfo& platform_info) {
  // Cobalt's user agent contains the following sections:
  //   Mozilla/5.0 (ChromiumStylePlatform)
  //   Cobalt/Version.BuildNumber-BuildConfiguration (unlike Gecko)
  //   JavaScript Engine Name/Version
  //   Starboard/APIVersion,
  //   Device/FirmwareVersion (Brand, Model, ConnectionType)

  //   Mozilla/5.0 (ChromiumStylePlatform)
  std::string user_agent = base::StringPrintf(
      "Mozilla/5.0 (%s)", platform_info.os_name_and_version().c_str());

  //   Cobalt/Version.BuildNumber-BuildConfiguration (unlike Gecko)
  base::StringAppendF(&user_agent, " Cobalt/%s.%s-%s (unlike Gecko)",
                      platform_info.cobalt_version().c_str(),
                      platform_info.cobalt_build_version_number().c_str(),
                      platform_info.build_configuration().c_str());

  // JavaScript Engine Name/Version
  if (!platform_info.javascript_engine_version().empty()) {
    base::StringAppendF(&user_agent, " %s",
                        platform_info.javascript_engine_version().c_str());
  }

  // Rasterizer Type
  if (!platform_info.rasterizer_type().empty()) {
    base::StringAppendF(&user_agent, " %s",
                        platform_info.rasterizer_type().c_str());
  }

  // Evergreen version
  if (!platform_info.evergreen_version().empty()) {
    base::StringAppendF(&user_agent, " Evergreen/%s",
                        platform_info.evergreen_version().c_str());
  }
  // Evergreen type
  if (!platform_info.evergreen_type().empty()) {
    base::StringAppendF(&user_agent, " Evergreen-%s",
                        platform_info.evergreen_type().c_str());
  }

  // Starboard/APIVersion,
  if (!platform_info.starboard_version().empty()) {
    base::StringAppendF(&user_agent, " %s",
                        platform_info.starboard_version().c_str());
  }

  // Device/FirmwareVersion (Brand, Model, ConnectionType)
  base::StringAppendF(
      &user_agent, ", %s_%s_%s_%s/%s (%s, %s, %s)",
      platform_info.original_design_manufacturer()
          .value_or(kUnknownFieldName)
          .c_str(),
      platform_info.device_type_string().c_str(),
      platform_info.chipset_model_number().value_or(kUnknownFieldName).c_str(),
      platform_info.model_year().value_or("0").c_str(),
      platform_info.firmware_version().value_or(kUnknownFieldName).c_str(),
      platform_info.brand().value_or(kUnknownFieldName).c_str(),
      platform_info.model().value_or(kUnknownFieldName).c_str(),
      platform_info.connection_type_string().c_str());

  if (!platform_info.aux_field().empty()) {
    base::StringAppendF(&user_agent, " %s", platform_info.aux_field().c_str());
  }
  return user_agent;
}

}  // namespace browser
}  // namespace cobalt
