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

#include <map>
#include <memory>

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "cobalt/browser/switches.h"
#include "cobalt/renderer/get_default_rasterizer_for_platform.h"
#include "cobalt/script/javascript_engine.h"
#include "cobalt/version.h"
#include "cobalt_build_id.h"  // NOLINT(build/include_subdir)
#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/common/system_property.h"
#include "starboard/extension/platform_info.h"
#if SB_IS(EVERGREEN)
#include "starboard/extension/installation_manager.h"
#endif  // SB_IS(EVERGREEN)
#include "starboard/system.h"
#if SB_IS(EVERGREEN)
#include "cobalt/updater/utils.h"
#endif

using starboard::kSystemPropertyMaxLength;

namespace cobalt {
namespace browser {

void GetUserAgentInputMap(
    const std::string& user_agent_input,
    std::map<std::string, std::string>& user_agent_input_map) {
  struct state {
    std::string field{""};
    std::string value{""};
    bool field_value_delimiter_found{false};

    void reset() {
      field.clear();
      value.clear();
      field_value_delimiter_found = false;
    }
  } current_state;

  char escape_char = '\\';
  char override_delimit_char = ';';
  char field_value_delimit_char = '=';
  bool prev_is_escape_char = false;

  for (auto cur_char : user_agent_input) {
    if (cur_char == override_delimit_char) {
      if (prev_is_escape_char) {
        if (!current_state.value.empty() &&
            current_state.value.back() == escape_char) {  // escape delimiter
                                                          // found in value.

          current_state.value.back() = override_delimit_char;
        } else {  // not a valid case for field, reset
          current_state.reset();
        }
      } else {
        if (current_state.field_value_delimiter_found) {  // valid field value
                                                          // pair found.
          user_agent_input_map[current_state.field] = current_state.value;
        }  // else, in current captured override, field_value_delimit_char
           // is not found, invalid.
        current_state.reset();
      }
    } else if (cur_char == field_value_delimit_char) {
      if (current_state.field.empty()) {  // field is not found when encounter
                                          // field_value_delimit_char,
                                          // invalid.
        current_state.reset();
      } else {
        current_state.field_value_delimiter_found =
            true;  // a field is found, next char is expected to be value
      }
    } else {
      if (current_state.field_value_delimiter_found) {
        current_state.value.push_back(cur_char);
      } else {
        current_state.field.push_back(cur_char);
      }
    }
    if (cur_char == escape_char) {
      prev_is_escape_char = true;
    } else {
      prev_is_escape_char = false;
    }
  }
  if (current_state.field_value_delimiter_found) {
    user_agent_input_map[current_state.field] = current_state.value;
  }
}

namespace {

#if SB_API_VERSION < 15

struct DeviceTypeName {
  SbSystemDeviceType device_type;
  char device_type_string[10];
};

const DeviceTypeName kDeviceTypeStrings[] = {
    {kSbSystemDeviceTypeBlueRayDiskPlayer, "BDP"},
    {kSbSystemDeviceTypeGameConsole, "GAME"},
    {kSbSystemDeviceTypeOverTheTopBox, "OTT"},
    {kSbSystemDeviceTypeSetTopBox, "STB"},
    {kSbSystemDeviceTypeTV, "TV"},
    {kSbSystemDeviceTypeAndroidTV, "ATV"},
    {kSbSystemDeviceTypeDesktopPC, "DESKTOP"},
#if SB_API_VERSION >= 14
    {kSbSystemDeviceTypeVideoProjector, "PROJECTOR"},
#endif  // SB_API_VERSION >= 14
    {kSbSystemDeviceTypeUnknown, "UNKNOWN"}};

std::string CreateDeviceTypeString(SbSystemDeviceType device_type) {
  for (auto& map : kDeviceTypeStrings) {
    if (map.device_type == device_type) {
      return std::string(map.device_type_string);
    }
  }
  NOTREACHED();
  return "UNKNOWN";
}

#if !defined(COBALT_BUILD_TYPE_GOLD)
SbSystemDeviceType GetDeviceType(std::string device_type_string) {
  for (auto& map : kDeviceTypeStrings) {
    if (!SbStringCompareNoCase(map.device_type_string,
                               device_type_string.c_str())) {
      return map.device_type;
    }
  }
  return kSbSystemDeviceTypeUnknown;
}
#endif
#endif  // SB_API_VERSION < 15

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

  // System Integrator
  result = SbSystemGetProperty(kSbSystemPropertySystemIntegratorName, value,
                               kSystemPropertyMaxLength);
  if (result) {
    info.set_original_design_manufacturer(value);
  }

  info.set_javascript_engine_version(
      script::GetJavaScriptEngineNameAndVersion());
  info.set_rasterizer_type(
      renderer::GetDefaultRasterizerForPlatform().rasterizer_name);

// Evergreen info
#if SB_IS(EVERGREEN)
  updater::EvergreenLibraryMetadata evergreen_library_metadata =
      updater::GetCurrentEvergreenLibraryMetadata();
  info.set_evergreen_version(evergreen_library_metadata.version);
  info.set_evergreen_file_type(evergreen_library_metadata.file_type);
  if (!SbSystemGetExtension(kCobaltExtensionInstallationManagerName)) {
    // If the installation manager is not initialized, the "evergreen_lite"
    // command line parameter is specified and the system image is loaded.
    info.set_evergreen_type("Lite");
  } else {
    info.set_evergreen_type("Full");
  }
#endif

  // Additional Platform Info
  auto platform_info_extension =
      static_cast<const CobaltExtensionPlatformInfoApi*>(
          SbSystemGetExtension(kCobaltExtensionPlatformInfoName));
  if (platform_info_extension &&
      strcmp(platform_info_extension->name, kCobaltExtensionPlatformInfoName) ==
          0 &&
      platform_info_extension->version >= 1) {
    result = platform_info_extension->GetFirmwareVersionDetails(
        value, kSystemPropertyMaxLength);
    if (result) {
      info.set_firmware_version_details(value);
    }
    info.set_os_experience(platform_info_extension->GetOsExperience());
  }

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

#if SB_API_VERSION >= 15
  result = SbSystemGetProperty(kSbSystemPropertyDeviceType, value,
                               kSystemPropertyMaxLength);
  SB_DCHECK(result);
  info.set_device_type(value);
#else
  // Fill platform info if it is a hardware TV device.
  info.set_device_type(SbSystemGetDeviceType());
#endif

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

// Apply overrides from command line
#if !defined(COBALT_BUILD_TYPE_GOLD)
  if (base::CommandLine::InitializedForCurrentProcess()) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kUserAgentClientHints)) {
      LOG(INFO) << "Entering UA overrides";
      std::string user_agent_input =
          command_line->GetSwitchValueASCII(switches::kUserAgentClientHints);

      std::map<std::string, std::string> user_agent_input_map;
      GetUserAgentInputMap(user_agent_input, user_agent_input_map);

      for (const auto& input : user_agent_input_map) {
        LOG(INFO) << "Overriding " << input.first << " to " << input.second;

        if (!input.first.compare("starboard_version")) {
          info.set_starboard_version(input.second);
          LOG(INFO) << "Set starboard version to " << input.second;
        } else if (!input.first.compare("os_name_and_version")) {
          info.set_os_name_and_version(input.second);
          LOG(INFO) << "Set os name and version to " << input.second;
        } else if (!input.first.compare("original_design_manufacturer")) {
          info.set_original_design_manufacturer(input.second);
          LOG(INFO) << "Set original design manufacturer to " << input.second;
        } else if (!input.first.compare("device_type")) {
#if SB_API_VERSION < 15
          info.set_device_type(GetDeviceType(input.second));
#else
          info.set_device_type(input.second);
#endif
          LOG(INFO) << "Set device type to " << input.second;
        } else if (!input.first.compare("chipset_model_number")) {
          info.set_chipset_model_number(input.second);
          LOG(INFO) << "Set chipset model to " << input.second;
        } else if (!input.first.compare("model_year")) {
          info.set_model_year(input.second);
          LOG(INFO) << "Set model year to " << input.second;
        } else if (!input.first.compare("firmware_version")) {
          info.set_firmware_version(input.second);
          LOG(INFO) << "Set firmware version to " << input.second;
        } else if (!input.first.compare("brand")) {
          info.set_brand(input.second);
          LOG(INFO) << "Set brand to " << input.second;
        } else if (!input.first.compare("model")) {
          info.set_model(input.second);
          LOG(INFO) << "Set model to " << input.second;
        } else if (!input.first.compare("aux_field")) {
          info.set_aux_field(input.second);
          LOG(INFO) << "Set aux field to " << input.second;
        } else if (!input.first.compare("javascript_engine_version")) {
          info.set_javascript_engine_version(input.second);
          LOG(INFO) << "Set javascript engine version to " << input.second;
        } else if (!input.first.compare("rasterizer_type")) {
          info.set_rasterizer_type(input.second);
          LOG(INFO) << "Set rasterizer type to " << input.second;
        } else if (!input.first.compare("evergreen_type")) {
          info.set_evergreen_type(input.second);
          LOG(INFO) << "Set evergreen type to " << input.second;
        } else if (!input.first.compare("evergreen_file_type")) {
          info.set_evergreen_file_type(input.second);
          LOG(INFO) << "Set evergreen file type to " << input.second;
        } else if (!input.first.compare("evergreen_version")) {
          info.set_evergreen_version(input.second);
          LOG(INFO) << "Set evergreen version to " << input.second;
        } else if (!input.first.compare("firmware_version_details")) {
          info.set_firmware_version_details(input.second);
          LOG(INFO) << "Set firmware version details to " << input.second;
        } else if (!input.first.compare("os_experience")) {
          info.set_os_experience(input.second);
          LOG(INFO) << "Set os experience to " << input.second;
        } else if (!input.first.compare("cobalt_version")) {
          info.set_cobalt_version(input.second);
          LOG(INFO) << "Set cobalt type to " << input.second;
        } else if (!input.first.compare("cobalt_build_version_number")) {
          info.set_cobalt_build_version_number(input.second);
          LOG(INFO) << "Set cobalt build version to " << input.second;
        } else if (!input.first.compare("build_configuration")) {
          info.set_build_configuration(input.second);
          LOG(INFO) << "Set build configuration to " << input.second;
        } else {
          LOG(WARNING) << "Unsupported user agent field: " << input.first;
        }
      }
    }
  }

#endif
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

#if SB_API_VERSION < 15
void UserAgentPlatformInfo::set_device_type(SbSystemDeviceType device_type) {
  device_type_ = device_type;
  device_type_string_ = CreateDeviceTypeString(device_type_);
}
#endif
void UserAgentPlatformInfo::set_device_type(const std::string& device_type) {
  device_type_string_ = device_type;
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

void UserAgentPlatformInfo::set_evergreen_file_type(
    const std::string& evergreen_file_type) {
  evergreen_file_type_ = Sanitize(evergreen_file_type, isTCHARorForwardSlash);
}

void UserAgentPlatformInfo::set_evergreen_version(
    const std::string& evergreen_version) {
  evergreen_version_ = Sanitize(evergreen_version, isTCHAR);
}

void UserAgentPlatformInfo::set_firmware_version_details(
    const std::string& firmware_version_details) {
  firmware_version_details_ =
      Sanitize(firmware_version_details, isVCHARorSpace);
}

void UserAgentPlatformInfo::set_os_experience(
    const std::string& os_experience) {
  os_experience_ = Sanitize(os_experience, isTCHAR);
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
