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

#include "cobalt/browser/user_agent_platform_info.h"

#include <memory>

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "cobalt/browser/switches.h"
#if SB_IS(EVERGREEN)
#include "cobalt/extension/installation_manager.h"
#endif  // SB_IS(EVERGREEN)
#include "cobalt/renderer/get_default_rasterizer_for_platform.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/version.h"
#include "cobalt_build_id.h"  // NOLINT(build/include_subdir)
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/system.h"
#if SB_IS(EVERGREEN)
#include "cobalt/updater/utils.h"
#endif

namespace cobalt {
namespace browser {

namespace {

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
      return "UNKNOWN";
  }
}

const char kUnspecifiedConnectionTypeName[] = "UnspecifiedConnectionType";

std::string CreateConnectionTypeString(
    const base::Optional<SbSystemConnectionType>& connection_type) {
  if (connection_type) {
    switch (*connection_type) {
      case kSbSystemConnectionTypeWired:
        return "Wired";
      case kSbSystemConnectionTypeWireless:
        return "Wireless";
      case kSbSystemConnectionTypeUnknown:
        return kUnspecifiedConnectionTypeName;
    }
  }
  return kUnspecifiedConnectionTypeName;
}

static bool isAsciiAlphaDigit(int c) {
  return base::IsAsciiAlpha(c) || base::IsAsciiDigit(c);
}

// https://datatracker.ietf.org/doc/html/rfc5234#appendix-B.1
static bool isVCHARorSpace(int c) { return c >= 0x20 && c <= 0x7E; }

// https://datatracker.ietf.org/doc/html/rfc7230#section-3.2.6
static bool isTCHAR(int c) {
  if (isAsciiAlphaDigit(c)) return true;
  switch (c) {
    case '!':
    case '#':
    case '$':
    case '%':
    case '&':
    case '\'':
    case '*':
    case '+':
    case '-':
    case '.':
    case '^':
    case '_':
    case '`':
    case '|':
    case '~':
      return true;
    default:
      return false;
  }
}

static bool isTCHARorForwardSlash(int c) { return isTCHAR(c) || c == '/'; }

const char kStripParentheses[] = "()";
const char kStripParenthesesAndComma[] = "(),";

// Replace reserved characters with Unicode homoglyphs
std::string Sanitize(const std::string& str, bool (*allowed)(int),
                     const char* strip = nullptr) {
  std::string clean;
  for (auto c : str) {
    if (allowed(c) && (!strip || !strchr(strip, c))) {
      clean.push_back(c);
    }
  }
  return clean;
}

base::Optional<std::string> Sanitize(base::Optional<std::string> str,
                                     bool (*allowed)(int),
                                     const char* strip = nullptr) {
  std::string clean;
  if (str) {
    clean = Sanitize(str.value(), allowed, strip);
  }
  if (clean.empty()) {
    return base::Optional<std::string>();
  }
  return base::Optional<std::string>(clean);
}


// Function that will query Starboard and populate a UserAgentPlatformInfo
// object based on those results.  This is de-coupled from
// CreateUserAgentString() so that the common logic in CreateUserAgentString()
// can be easily unit tested.
void InitializeUserAgentPlatformInfoFields(UserAgentPlatformInfo& info) {
  info.set_starboard_version(
      base::StringPrintf("Starboard/%d", SB_API_VERSION));

  const size_t kSystemPropertyMaxLength = 1024;
  char value[kSystemPropertyMaxLength];
  bool result;

  result = SbSystemGetProperty(kSbSystemPropertyPlatformName, value,
                               kSystemPropertyMaxLength);
  SB_DCHECK(result);
  info.set_os_name_and_version(value);

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  // Because we add Cobalt's user agent string to Crashpad before we actually
  // start Cobalt, the command line won't be initialized when we first try to
  // get the user agent string.
  if (base::CommandLine::InitializedForCurrentProcess()) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kUserAgentOsNameVersion)) {
      info.set_os_name_and_version(
          command_line->GetSwitchValueASCII(switches::kUserAgentOsNameVersion));
    }
  }
#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES

#if SB_API_VERSION >= 12
  // System Integrator
  result = SbSystemGetProperty(kSbSystemPropertySystemIntegratorName, value,
                               kSystemPropertyMaxLength);
#else
  // Original Design Manufacturer (ODM)
  result = SbSystemGetProperty(kSbSystemPropertyOriginalDesignManufacturerName,
                               value, kSystemPropertyMaxLength);
#endif
  if (result) {
    info.set_original_design_manufacturer(value);
  }

  info.set_javascript_engine_version(
      script::GetJavaScriptEngineNameAndVersion());
  info.set_rasterizer_type(
      renderer::GetDefaultRasterizerForPlatform().rasterizer_name);

// Evergreen version
#if SB_IS(EVERGREEN)
  info.set_evergreen_version(updater::GetCurrentEvergreenVersion());
  if (!SbSystemGetExtension(kCobaltExtensionInstallationManagerName)) {
    // If the installation manager is not initialized, the "evergreen_lite"
    // command line parameter is specified and the system image is loaded.
    info.set_evergreen_type("Lite");
  } else {
    info.set_evergreen_type("Full");
  }
#endif

  info.set_cobalt_version(COBALT_VERSION);
  info.set_cobalt_build_version_number(COBALT_BUILD_VERSION_NUMBER);

#if defined(COBALT_BUILD_TYPE_DEBUG)
  info.set_build_configuration("debug");
#elif defined(COBALT_BUILD_TYPE_DEVEL)
  info.set_build_configuration("devel");
#elif defined(COBALT_BUILD_TYPE_QA)
  info.set_build_configuration("qa");
#elif defined(COBALT_BUILD_TYPE_GOLD)
  info.set_build_configuration("gold");
#else
#error Unknown build configuration.
#endif

  result = SbSystemGetProperty(kSbSystemPropertyUserAgentAuxField, value,
                               kSystemPropertyMaxLength);
  if (result) {
    info.set_aux_field(value);
  }

  // Fill platform info if it is a hardware TV device.
  info.set_device_type(SbSystemGetDeviceType());

  // Chipset model number
  result = SbSystemGetProperty(kSbSystemPropertyChipsetModelNumber, value,
                               kSystemPropertyMaxLength);
  if (result) {
    info.set_chipset_model_number(value);
  }

  // Model year
  result = SbSystemGetProperty(kSbSystemPropertyModelYear, value,
                               kSystemPropertyMaxLength);
  if (result) {
    info.set_model_year(value);
  }

  // Firmware version
  result = SbSystemGetProperty(kSbSystemPropertyFirmwareVersion, value,
                               kSystemPropertyMaxLength);
  if (result) {
    info.set_firmware_version(value);
  }

  // Brand
  result = SbSystemGetProperty(kSbSystemPropertyBrandName, value,
                               kSystemPropertyMaxLength);
  if (result) {
    info.set_brand(value);
    // If we didn't get a value for the original design manufacturer, use the
    // value for the brand if one is available.
    if (!info.original_design_manufacturer() && info.brand()) {
      info.set_original_design_manufacturer(info.brand());
    }
  }

  // Model name
  result = SbSystemGetProperty(kSbSystemPropertyModelName, value,
                               kSystemPropertyMaxLength);
  if (result) {
    info.set_model(value);
  }

  // Connection type
  info.set_connection_type(SbSystemGetConnectionType());
}

}  // namespace

UserAgentPlatformInfo::UserAgentPlatformInfo() {
  InitializeUserAgentPlatformInfoFields(*this);
}

void UserAgentPlatformInfo::set_starboard_version(
    const std::string& starboard_version) {
  starboard_version_ = Sanitize(starboard_version, isTCHARorForwardSlash);
}
void UserAgentPlatformInfo::set_os_name_and_version(
    const std::string& os_name_and_version) {
  os_name_and_version_ =
      Sanitize(os_name_and_version, isVCHARorSpace, kStripParentheses);
}
void UserAgentPlatformInfo::set_original_design_manufacturer(
    base::Optional<std::string> original_design_manufacturer) {
  if (original_design_manufacturer) {
    original_design_manufacturer_ =
        Sanitize(original_design_manufacturer, isAsciiAlphaDigit);
  }
}
void UserAgentPlatformInfo::set_device_type(SbSystemDeviceType device_type) {
  device_type_ = device_type;
  device_type_string_ = CreateDeviceTypeString(device_type_);
}

void UserAgentPlatformInfo::set_chipset_model_number(
    base::Optional<std::string> chipset_model_number) {
  if (chipset_model_number) {
    chipset_model_number_ = Sanitize(chipset_model_number, isAsciiAlphaDigit);
  }
}

void UserAgentPlatformInfo::set_model_year(
    base::Optional<std::string> model_year) {
  if (model_year) {
    model_year_ = Sanitize(model_year, base::IsAsciiDigit);
  }
}

void UserAgentPlatformInfo::set_firmware_version(
    base::Optional<std::string> firmware_version) {
  if (firmware_version) {
    firmware_version_ = Sanitize(firmware_version, isTCHAR);
  }
}

void UserAgentPlatformInfo::set_brand(base::Optional<std::string> brand) {
  if (brand) {
    brand_ = Sanitize(brand, isVCHARorSpace, kStripParenthesesAndComma);
  }
}

void UserAgentPlatformInfo::set_model(base::Optional<std::string> model) {
  if (model) {
    model_ = Sanitize(model, isVCHARorSpace, kStripParenthesesAndComma);
  }
}

void UserAgentPlatformInfo::set_aux_field(const std::string& aux_field) {
  aux_field_ = Sanitize(aux_field, isTCHARorForwardSlash);
}

void UserAgentPlatformInfo::set_connection_type(
    base::Optional<SbSystemConnectionType> connection_type) {
  if (connection_type) {
    connection_type_ = connection_type;
    connection_type_string_ = CreateConnectionTypeString(connection_type_);
  } else {
    connection_type_string_ = "";
  }
}

void UserAgentPlatformInfo::set_javascript_engine_version(
    const std::string& javascript_engine_version) {
  javascript_engine_version_ =
      Sanitize(javascript_engine_version, isTCHARorForwardSlash);
}

void UserAgentPlatformInfo::set_rasterizer_type(
    const std::string& rasterizer_type) {
  rasterizer_type_ = Sanitize(rasterizer_type, isTCHARorForwardSlash);
}

void UserAgentPlatformInfo::set_evergreen_type(
    const std::string& evergreen_type) {
  evergreen_type_ = Sanitize(evergreen_type, isTCHARorForwardSlash);
}

void UserAgentPlatformInfo::set_evergreen_version(
    const std::string& evergreen_version) {
  evergreen_version_ = Sanitize(evergreen_version, isTCHAR);
}

void UserAgentPlatformInfo::set_cobalt_version(
    const std::string& cobalt_version) {
  cobalt_version_ = Sanitize(cobalt_version, isTCHAR);
}

void UserAgentPlatformInfo::set_cobalt_build_version_number(
    const std::string& cobalt_build_version_number) {
  cobalt_build_version_number_ = Sanitize(cobalt_build_version_number, isTCHAR);
}

void UserAgentPlatformInfo::set_build_configuration(
    const std::string& build_configuration) {
  build_configuration_ = Sanitize(build_configuration, isTCHAR);
}

}  // namespace browser
}  // namespace cobalt
