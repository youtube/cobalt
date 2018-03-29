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

#include <vector>

#include "base/string_util.h"
#include "base/stringprintf.h"
#if defined(COBALT_ENABLE_LIB)
#include "cobalt/network/lib/exported/user_agent.h"
#endif
#include "cobalt/version.h"

#include "cobalt_build_id.h"  // NOLINT(build/include)

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

#if defined(COBALT_ENABLE_LIB)
bool CopyStringAndTestIfSuccess(char* out_value,
                                size_t value_length,
                                const char* from_value) {
  if (strlen(from_value) + 1 > value_length)
    return false;
  base::strlcpy(out_value, from_value, value_length);
  return true;
}
#endif

}  // namespace

namespace cobalt {
namespace network {

namespace {

#if defined(COBALT_BUILD_TYPE_DEBUG)
const char kBuildConfiguration[] = "debug";
#elif defined(COBALT_BUILD_TYPE_DEVEL)
const char kBuildConfiguration[] = "devel";
#elif defined(COBALT_BUILD_TYPE_QA)
const char kBuildConfiguration[] = "qa";
#elif defined(COBALT_BUILD_TYPE_GOLD)
const char kBuildConfiguration[] = "gold";
#else
#error Unknown build configuration.
#endif

struct SanitizeReplacements {
  const char* replace_chars;
  const char* replace_with;
} kSanitizeReplacements[] = {
  { ",", u8"\uFF0C" },  // fullwidth comma
  { "_", u8"\u2E0F" },  // paragraphos
  { "/", u8"\u2215" },  // division slash
  { "(", u8"\uFF08" },  // fullwidth left paren
  { ")", u8"\uFF09" },  // fullwidth right paren
};

// Replace reserved characters with Unicode homoglyphs
std::string Sanitize(const std::string& str) {
  std::string clean(str);
  for (size_t i=0; i < arraysize(kSanitizeReplacements); i++) {
    const SanitizeReplacements* replacement = kSanitizeReplacements + i;
    ReplaceChars(
        clean, replacement->replace_chars, replacement->replace_with, &clean);
  }
  return clean;
}

}  // namespace

std::string UserAgentStringFactory::CreateUserAgentString() {
  // Cobalt's user agent contains the following sections:
  //   Mozilla/5.0 (ChromiumStylePlatform)
  //   Cobalt/Version.BuildNumber-BuildConfiguration (unlike Gecko)
  //   Starboard/APIVersion,
  //   Device/FirmwareVersion (Brand, Model, ConnectionType)

  //   Mozilla/5.0 (ChromiumStylePlatform)
  std::string user_agent =
      base::StringPrintf("Mozilla/5.0 (%s)", CreatePlatformString().c_str());

  //   Cobalt/Version.BuildNumber-BuildConfiguration (unlike Gecko)
  base::StringAppendF(&user_agent, " Cobalt/%s.%s-%s (unlike Gecko)",
                      COBALT_VERSION, COBALT_BUILD_VERSION_NUMBER,
                      kBuildConfiguration);

  //   Starboard/APIVersion,
  if (!starboard_version_.empty()) {
    base::StringAppendF(&user_agent, " %s", starboard_version_.c_str());
  }

  //   Device/FirmwareVersion (Brand, Model, ConnectionType)
  if (platform_info_) {
    base::StringAppendF(
        &user_agent, ", %s_%s_%s/%s (%s, %s, %s)",
        Sanitize(platform_info_->network_operator.value_or("")).c_str(),
        CreateDeviceTypeString().c_str(),
        Sanitize(platform_info_->chipset_model_number.value_or("")).c_str(),
        Sanitize(platform_info_->firmware_version.value_or("")).c_str(),
        Sanitize(g_brand_name_override_set ?
            std::string(g_brand_name_override) : platform_info_->brand).c_str(),
        Sanitize(g_model_name_override_set ?
            std::string(g_model_name_override) : platform_info_->model).c_str(),
        CreateConnectionTypeString().c_str());
  } else if (ShouldOverrideDevice()) {
    // When Starboard does not report platform info but we are overriding the
    // device.
    base::StringAppendF(
        &user_agent, ", _%s_/ (%s, %s,)", CreateDeviceTypeString().c_str(),
        Sanitize(g_brand_name_override_set ? g_brand_name_override : "")
            .c_str(),
        Sanitize(g_model_name_override_set ? g_model_name_override : "")
            .c_str());
  }

  if (!aux_field_.empty()) {
    user_agent.append(" ");
    user_agent.append(aux_field_);
  }
  return user_agent;
}

// Inserts semicolons between non-empty components of the platform.
std::string UserAgentStringFactory::CreatePlatformString() {
  std::string platform;
  if (!historic_platform_name_.empty()) {
    platform += historic_platform_name_;
    platform += "; ";
  }

  DCHECK(!os_name_and_version_.empty());
  platform += os_name_and_version_;

  if (!architecture_tokens_.empty()) {
    platform += "; ";
    platform += architecture_tokens_;
  }
  if (g_user_agent_platform_suffix[0] != '\0') {
    platform += "; ";
    platform += g_user_agent_platform_suffix;
  }

  return platform;
}

std::string UserAgentStringFactory::CreateDeviceTypeString() {
  auto reported_device_type =
      g_device_type_override_set ?
          StarboardToPlatformInfoDeviceType(g_device_type_override) :
          platform_info_ ? platform_info_->device_type :
                           PlatformInfo::kInvalidDeviceType;

  switch (reported_device_type) {
    case PlatformInfo::kBlueRayDiskPlayer:
      return "BDP";
    case PlatformInfo::kGameConsole:
      return "GAME";
    case PlatformInfo::kOverTheTopBox:
      return "OTT";
    case PlatformInfo::kSetTopBox:
      return "STB";
    case PlatformInfo::kTV:
      return "TV";
    case PlatformInfo::kAndroidTV:
      return "ATV";
    case PlatformInfo::kUnknown:
      return "UNKNOWN";
    case PlatformInfo::kInvalidDeviceType:
    default:
      NOTREACHED();
      return "";
  }
}

std::string UserAgentStringFactory::CreateConnectionTypeString() {
  if (platform_info_->connection_type) {
    switch (*platform_info_->connection_type) {
      case PlatformInfo::kWiredConnection:
        return "Wired";
      case PlatformInfo::kWirelessConnection:
        return "Wireless";
      default:
        NOTREACHED();
    }
  }

  return "";
}

UserAgentStringFactory::PlatformInfo::DeviceType
UserAgentStringFactory::StarboardToPlatformInfoDeviceType(
    SbSystemDeviceType device_type) {
  switch (device_type) {
    case kSbSystemDeviceTypeBlueRayDiskPlayer:
      return PlatformInfo::kBlueRayDiskPlayer;
    case kSbSystemDeviceTypeGameConsole:
      return PlatformInfo::kGameConsole;
    case kSbSystemDeviceTypeOverTheTopBox:
      return PlatformInfo::kOverTheTopBox;
    case kSbSystemDeviceTypeSetTopBox:
      return PlatformInfo::kSetTopBox;
    case kSbSystemDeviceTypeTV:
      return PlatformInfo::kTV;
    case kSbSystemDeviceTypeAndroidTV:
      return PlatformInfo::kAndroidTV;
    case kSbSystemDeviceTypeUnknown:
      return PlatformInfo::kUnknown;
    case kSbSystemDeviceTypeDesktopPC:
      break;
  }
  return PlatformInfo::kInvalidDeviceType;
}

}  // namespace network
}  // namespace cobalt

#if defined(COBALT_ENABLE_LIB)
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

  if (CopyStringAndTestIfSuccess(g_brand_name_override,
                                 kPropertyMaxLength, value)) {
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

  if (CopyStringAndTestIfSuccess(g_model_name_override,
                                 kPropertyMaxLength, value)) {
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
