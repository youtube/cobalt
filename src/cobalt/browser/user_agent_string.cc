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
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "cobalt/browser/switches.h"
#include "cobalt/renderer/get_default_rasterizer_for_platform.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/version.h"
#include "cobalt_build_id.h"  // NOLINT(build/include)
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/system.h"
#if SB_IS(EVERGREEN)
#include "cobalt/updater/utils.h"
#endif

namespace cobalt {
namespace browser {

namespace {

struct SanitizeReplacements {
  const char* replace_chars;
  const char* replace_with;
} kSanitizeReplacements[] = {
    {",", u8"\uFF0C"},  // fullwidth comma
    {"_", u8"\u2E0F"},  // paragraphos
    {"/", u8"\u2215"},  // division slash
    {"(", u8"\uFF08"},  // fullwidth left paren
    {")", u8"\uFF09"},  // fullwidth right paren
};

// Replace reserved characters with Unicode homoglyphs
std::string Sanitize(const std::string& str) {
  std::string clean(str);
  for (size_t i = 0; i < arraysize(kSanitizeReplacements); i++) {
    const SanitizeReplacements* replacement = kSanitizeReplacements + i;
    base::ReplaceChars(clean, replacement->replace_chars,
                       replacement->replace_with, &clean);
  }
  return clean;
}

std::string CreateDeviceTypeString(SbSystemDeviceType device_type) {
  switch (device_type) {
    case kSbSystemDeviceTypeBlueRayDiskPlayer:
      return "BDP";
    case kSbSystemDeviceTypeGameConsole:
      return "GAME";
    case kSbSystemDeviceTypeOverTheTopBox:
      return "OTT";
    case kSbSystemDeviceTypeSetTopBox:
      return "STB";
    case kSbSystemDeviceTypeTV:
      return "TV";
    case kSbSystemDeviceTypeAndroidTV:
      return "ATV";
    case kSbSystemDeviceTypeDesktopPC:
      return "DESKTOP";
    case kSbSystemDeviceTypeUnknown:
      return "UNKNOWN";
    default:
      NOTREACHED();
      return "";
  }
}

std::string CreateConnectionTypeString(
    const base::Optional<SbSystemConnectionType>& connection_type) {
  if (connection_type) {
    switch (*connection_type) {
      case kSbSystemConnectionTypeWired:
        return "Wired";
      case kSbSystemConnectionTypeWireless:
        return "Wireless";
      default:
        NOTREACHED();
    }
  }

  return "";
}

}  // namespace

UserAgentPlatformInfo GetUserAgentPlatformInfoFromSystem() {
  UserAgentPlatformInfo platform_info;

  platform_info.starboard_version =
      base::StringPrintf("Starboard/%d", SB_API_VERSION);

  const size_t kSystemPropertyMaxLength = 1024;
  char value[kSystemPropertyMaxLength];
  bool result;

  result = SbSystemGetProperty(kSbSystemPropertyPlatformName, value,
                               kSystemPropertyMaxLength);
  SB_DCHECK(result);
  platform_info.os_name_and_version = value;

  // Fill platform info if it is a hardware TV device.
  SbSystemDeviceType device_type = SbSystemGetDeviceType();

#if SB_API_VERSION >= 12
  // System Integrator
  result = SbSystemGetProperty(kSbSystemPropertySystemIntegratorName, value,
                               kSystemPropertyMaxLength);
#elif SB_API_VERSION == 11
  // Original Design Manufacturer (ODM)
  result = SbSystemGetProperty(kSbSystemPropertyOriginalDesignManufacturerName,
                               value, kSystemPropertyMaxLength);
#else
  result = SbSystemGetProperty(kSbSystemPropertyNetworkOperatorName,
                               value, kSystemPropertyMaxLength);
#endif
  if (result) {
    platform_info.original_design_manufacturer = value;
  }

  platform_info.javascript_engine_version =
      script::GetJavaScriptEngineNameAndVersion();

  platform_info.rasterizer_type =
      renderer::GetDefaultRasterizerForPlatform().rasterizer_name;

  platform_info.cobalt_version = COBALT_VERSION;
  platform_info.cobalt_build_version_number = COBALT_BUILD_VERSION_NUMBER;

#if defined(COBALT_BUILD_TYPE_DEBUG)
  platform_info.build_configuration = "debug";
#elif defined(COBALT_BUILD_TYPE_DEVEL)
  platform_info.build_configuration = "devel";
#elif defined(COBALT_BUILD_TYPE_QA)
  platform_info.build_configuration = "qa";
#elif defined(COBALT_BUILD_TYPE_GOLD)
  platform_info.build_configuration = "gold";
#else
#error Unknown build configuration.
#endif

#if SB_API_VERSION >= 5
  result = SbSystemGetProperty(kSbSystemPropertyUserAgentAuxField, value,
                               kSystemPropertyMaxLength);
  if (result) {
    platform_info.aux_field = value;
  }
#endif  // SB_API_VERSION >= 5

  // Device Type
  platform_info.device_type = device_type;

  // Chipset model number
  result = SbSystemGetProperty(kSbSystemPropertyChipsetModelNumber, value,
                               kSystemPropertyMaxLength);
  if (result) {
    platform_info.chipset_model_number = value;
  }

  // Model year
  result = SbSystemGetProperty(kSbSystemPropertyModelYear, value,
                               kSystemPropertyMaxLength);
  if (result) {
    platform_info.model_year = value;
  }

  // Firmware version
  result = SbSystemGetProperty(kSbSystemPropertyFirmwareVersion, value,
                               kSystemPropertyMaxLength);
  if (result) {
    platform_info.firmware_version = value;
  }

  // Brand
  result = SbSystemGetProperty(kSbSystemPropertyBrandName, value,
                               kSystemPropertyMaxLength);
  if (result) {
    platform_info.brand = value;
  }

  // Model name
  result = SbSystemGetProperty(kSbSystemPropertyModelName, value,
                               kSystemPropertyMaxLength);
  if (result) {
    platform_info.model = value;
  }

  // Connection type
  SbSystemConnectionType connection_type = SbSystemGetConnectionType();
  if (connection_type != kSbSystemConnectionTypeUnknown) {
    platform_info.connection_type = connection_type;
  }

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

  std::string os_name_and_version = platform_info.os_name_and_version;

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kUserAgentOsNameVersion)) {
    os_name_and_version =
        command_line->GetSwitchValueASCII(switches::kUserAgentOsNameVersion);
  }
#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES

  //   Mozilla/5.0 (ChromiumStylePlatform)
  std::string user_agent =
      base::StringPrintf("Mozilla/5.0 (%s)", os_name_and_version.c_str());

  //   Cobalt/Version.BuildNumber-BuildConfiguration (unlike Gecko)
  base::StringAppendF(&user_agent, " Cobalt/%s.%s-%s (unlike Gecko)",
                      platform_info.cobalt_version.c_str(),
                      platform_info.cobalt_build_version_number.c_str(),
                      platform_info.build_configuration.c_str());

  // JavaScript Engine Name/Version
  if (!platform_info.javascript_engine_version.empty()) {
    base::StringAppendF(&user_agent, " %s",
                        platform_info.javascript_engine_version.c_str());
  }

  // Rasterizer Type
  if (!platform_info.rasterizer_type.empty()) {
    base::StringAppendF(&user_agent, " %s",
                        platform_info.rasterizer_type.c_str());
  }

// Evergreen version
#if SB_IS(EVERGREEN)
  const std::string evergreen_version = updater::GetCurrentEvergreenVersion();
  if (!evergreen_version.empty()) {
    base::StringAppendF(&user_agent, " Evergreen/%s",
                        evergreen_version.c_str());
  }
#endif

  // Starboard/APIVersion,
  if (!platform_info.starboard_version.empty()) {
    base::StringAppendF(&user_agent, " %s",
                        platform_info.starboard_version.c_str());
  }

  // Device/FirmwareVersion (Brand, Model, ConnectionType)
  base::StringAppendF(
      &user_agent, ", %s_%s_%s_%s/%s (%s, %s, %s)",
      Sanitize(platform_info.original_design_manufacturer.value_or("")).c_str(),
      CreateDeviceTypeString(platform_info.device_type).c_str(),
      Sanitize(platform_info.chipset_model_number.value_or("")).c_str(),
      Sanitize(platform_info.model_year.value_or("")).c_str(),
      Sanitize(platform_info.firmware_version.value_or("")).c_str(),
      Sanitize(platform_info.brand.value_or("")).c_str(),
      Sanitize(platform_info.model.value_or("")).c_str(),
      CreateConnectionTypeString(platform_info.connection_type).c_str());

  if (!platform_info.aux_field.empty()) {
    user_agent.append(" ");
    user_agent.append(platform_info.aux_field);
  }
  return user_agent;
}

}  // namespace browser
}  // namespace cobalt
