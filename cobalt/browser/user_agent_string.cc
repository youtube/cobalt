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

#include <vector>

#include "base/string_util.h"
#include "base/stringprintf.h"
#if defined(COBALT_ENABLE_LIB)
#include "cobalt/browser/lib/exported/user_agent.h"
#endif
#include "cobalt/script/javascript_engine.h"
#include "cobalt/version.h"
#include "cobalt_build_id.h"  // NOLINT(build/include)
#include "starboard/log.h"
#include "starboard/string.h"
#include "starboard/system.h"

// Setup CobaltLib overrides for some user agent components.
#if defined(COBALT_ENABLE_LIB)

namespace {

// Max length including null terminator.
const size_t kUserAgentPlatformSuffixMaxLength = 128;
char g_user_agent_platform_suffix[kUserAgentPlatformSuffixMaxLength] = {0};

bool g_device_type_override_set = false;
SbSystemDeviceType g_device_type_override = kSbSystemDeviceTypeUnknown;

// Max length including null terminator.
const size_t kPropertyMaxLength = 512;
bool g_brand_name_override_set = false;
char g_brand_name_override[kPropertyMaxLength] = {0};
bool g_model_name_override_set = false;
char g_model_name_override[kPropertyMaxLength] = {0};

bool ShouldOverrideDevice() {
  return g_device_type_override_set || g_brand_name_override_set ||
         g_model_name_override_set;
}

bool CopyStringAndTestIfSuccess(char* out_value, size_t value_length,
                                const char* from_value) {
  if (strlen(from_value) + 1 > value_length) return false;
  base::strlcpy(out_value, from_value, value_length);
  return true;
}

}  // namespace

// Allow host app to append a suffix to the reported platform name.
bool CbLibUserAgentSetPlatformNameSuffix(const char* suffix) {
  if (!CopyStringAndTestIfSuccess(g_user_agent_platform_suffix,
                                  kPropertyMaxLength, suffix)) {
    // If the suffix is too large then nothing is appended to the platform.
    g_user_agent_platform_suffix[0] = '\0';
    return false;
  }

  return true;
}

bool CbLibUserAgentSetBrandNameOverride(const char* value) {
  if (!value) {
    g_brand_name_override_set = false;
    return true;
  }

  if (CopyStringAndTestIfSuccess(g_brand_name_override, kPropertyMaxLength,
                                 value)) {
    g_brand_name_override_set = true;
    return true;
  }

  // Failed to reset/set value.
  return false;
}

bool CbLibUserAgentSetModelNameOverride(const char* value) {
  if (!value) {
    g_model_name_override_set = false;
    return true;
  }

  if (CopyStringAndTestIfSuccess(g_model_name_override, kPropertyMaxLength,
                                 value)) {
    g_model_name_override_set = true;
    return true;
  }

  // Failed to reset/set value.
  return false;
}

bool CbLibUserAgentSetDeviceTypeOverride(SbSystemDeviceType device_type) {
  switch (device_type) {
    case kSbSystemDeviceTypeBlueRayDiskPlayer:
    case kSbSystemDeviceTypeGameConsole:
    case kSbSystemDeviceTypeOverTheTopBox:
    case kSbSystemDeviceTypeSetTopBox:
    case kSbSystemDeviceTypeTV:
    case kSbSystemDeviceTypeDesktopPC:
    case kSbSystemDeviceTypeAndroidTV:
    case kSbSystemDeviceTypeUnknown:
      g_device_type_override_set = true;
      g_device_type_override = device_type;
      return true;
    default:
      g_device_type_override_set = false;
      return false;
  }
}

#endif  // defined(COBALT_ENABLE_LIB)

namespace cobalt {
namespace browser {

namespace {

#if SB_API_VERSION == SB_EXPERIMENTAL_API_VERSION
const char kStarboardStabilitySuffix[] = "-Experimental";
#elif defined(SB_RELEASE_CANDIDATE_API_VERSION) &&        \
    SB_API_VERSION >= SB_RELEASE_CANDIDATE_API_VERSION && \
    SB_API_VERSION < SB_EXPERIMENTAL_API_VERSION
const char kStarboardStabilitySuffix[] = "-ReleaseCandidate";
#else
const char kStarboardStabilitySuffix[] = "";
#endif

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
    ReplaceChars(clean, replacement->replace_chars, replacement->replace_with,
                 &clean);
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
    const base::optional<SbSystemConnectionType>& connection_type) {
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

  platform_info.starboard_version = base::StringPrintf(
      "Starboard/%d%s", SB_API_VERSION, kStarboardStabilitySuffix);

  const size_t kSystemPropertyMaxLength = 1024;
  char value[kSystemPropertyMaxLength];
  bool result;

  result = SbSystemGetProperty(kSbSystemPropertyPlatformName, value,
                               kSystemPropertyMaxLength);
  SB_DCHECK(result);
  platform_info.os_name_and_version = value;

  // Fill platform info if it is a hardware TV device.
  SbSystemDeviceType device_type = SbSystemGetDeviceType();

  // Network operator
  result = SbSystemGetProperty(kSbSystemPropertyNetworkOperatorName, value,
                               kSystemPropertyMaxLength);
  if (result) {
    platform_info.network_operator = value;
  }

  platform_info.javascript_engine_version =
      script::GetJavaScriptEngineNameAndVersion();

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
  result = SbSystemGetProperty(kSbSystemPropertyManufacturerName, value,
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

#if defined(COBALT_ENABLE_LIB)
  if (g_user_agent_platform_suffix[0] != '\0') {
    platform_info.os_name_and_version += "; ";
    platform_info.os_name_and_version += g_user_agent_platform_suffix;
  }

  // Check for overrided values, to see if a client of CobaltLib has explicitly
  // requested that some values be overridden.
  if (g_device_type_override_set) {
    platform_info.device_type = g_device_type_override;
  }
  if (g_brand_name_override_set) {
    platform_info.brand = g_brand_name_override;
  }
  if (g_model_name_override_set) {
    platform_info.model = g_model_name_override;
  }
#endif  // defined(COBALT_ENABLE_LIB)

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
      "Mozilla/5.0 (%s)", platform_info.os_name_and_version.c_str());

  //   Cobalt/Version.BuildNumber-BuildConfiguration (unlike Gecko)
  base::StringAppendF(&user_agent, " Cobalt/%s.%s-%s (unlike Gecko)",
                      platform_info.cobalt_version.c_str(),
                      platform_info.cobalt_build_version_number.c_str(),
                      platform_info.build_configuration.c_str());

  //   JavaScript Engine Name/Version
  if (!platform_info.javascript_engine_version.empty()) {
    base::StringAppendF(&user_agent, " %s",
                        platform_info.javascript_engine_version.c_str());
  }

  //   Starboard/APIVersion,
  if (!platform_info.starboard_version.empty()) {
    base::StringAppendF(&user_agent, " %s",
                        platform_info.starboard_version.c_str());
  }

  //   Device/FirmwareVersion (Brand, Model, ConnectionType)
  base::StringAppendF(
      &user_agent, ", %s_%s_%s_%s/%s (%s, %s, %s)",
      Sanitize(platform_info.network_operator.value_or("")).c_str(),
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
