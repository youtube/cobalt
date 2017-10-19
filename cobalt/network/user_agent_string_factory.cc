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

#if defined(COBALT_ENABLE_LIB)
namespace {
// Max length including null terminator.
const size_t kUserAgentPlatformMaxSuffixLength = 128;
char g_user_agent_platform_suffix[kUserAgentPlatformMaxSuffixLength] = {0};
}  // namespace
#endif  // defined(COBALT_ENABLE_LIB)

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
  if (youtube_tv_info_) {
    base::StringAppendF(
        &user_agent, ", %s_%s_%s/%s (%s, %s, %s)",
        Sanitize(youtube_tv_info_->network_operator.value_or("")).c_str(),
        CreateDeviceTypeString().c_str(),
        Sanitize(youtube_tv_info_->chipset_model_number.value_or("")).c_str(),
        Sanitize(youtube_tv_info_->firmware_version.value_or("")).c_str(),
        Sanitize(youtube_tv_info_->brand).c_str(),
        Sanitize(youtube_tv_info_->model).c_str(),
        CreateConnectionTypeString().c_str());
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

#if defined(COBALT_ENABLE_LIB)
  if (g_user_agent_platform_suffix[0] != '\0') {
    platform += "; ";
    platform += g_user_agent_platform_suffix;
  }
#endif  // defined(COBALT_ENABLE_LIB)

  return platform;
}

std::string UserAgentStringFactory::CreateDeviceTypeString() {
  switch (youtube_tv_info_->device_type) {
    case YouTubeTVInfo::kBlueRayDiskPlayer:
      return "BDP";
    case YouTubeTVInfo::kGameConsole:
      return "GAME";
    case YouTubeTVInfo::kOverTheTopBox:
      return "OTT";
    case YouTubeTVInfo::kSetTopBox:
      return "STB";
    case YouTubeTVInfo::kTV:
      return "TV";
    case YouTubeTVInfo::kAndroidTV:
      return "ATV";
    case YouTubeTVInfo::kInvalidDeviceType:
    default:
      NOTREACHED();
      return "";
  }
}

std::string UserAgentStringFactory::CreateConnectionTypeString() {
  if (youtube_tv_info_->connection_type) {
    switch (*youtube_tv_info_->connection_type) {
      case YouTubeTVInfo::kWiredConnection:
        return "Wired";
      case YouTubeTVInfo::kWirelessConnection:
        return "Wireless";
      default:
        NOTREACHED();
    }
  }

  return "";
}

}  // namespace network
}  // namespace cobalt

#if defined(COBALT_ENABLE_LIB)
// Allow host app to append a suffix to the reported platform name.
bool CbLibUserAgentSetPlatformNameSuffix(const char* suffix) {
  size_t suffix_length = base::strlcpy(g_user_agent_platform_suffix, suffix,
                                       kUserAgentPlatformMaxSuffixLength);
  if (suffix_length >= kUserAgentPlatformMaxSuffixLength) {
    // If the suffix is too large then nothing is appended to the platform.
    g_user_agent_platform_suffix[0] = '\0';
    return false;
  }

  return true;
}
#endif  // defined(COBALT_ENABLE_LIB)
