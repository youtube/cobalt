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

#include "cobalt/browser/user_agent/user_agent_platform_info.h"

#include <map>
#include <memory>
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/system/sys_info_starboard.h"
#include "build/build_config.h"
#include "starboard/common/system_property.h"
#include "starboard/extension/platform_info.h"

#include "cobalt/cobalt_build_id.h"  // Generated
#include "cobalt/version.h"
#include "v8/include/v8-version-string.h"

#if BUILDFLAG(USE_EVERGREEN)
#include "cobalt/updater/util.h"  //nogncheck
#include "starboard/extension/installation_manager.h"
#endif

namespace cobalt {

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

  constexpr char escape_char = '\\';
  constexpr char override_delimit_char = ';';
  constexpr char field_value_delimit_char = '=';
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

// https://datatracker.ietf.org/doc/html/rfc5234#appendix-B.1
static bool isVCHARorSpace(int c) {
  return c >= 0x20 && c <= 0x7E;
}

// https://datatracker.ietf.org/doc/html/rfc7230#section-3.2.6
static bool isTCHAR(int c) {
  if (base::IsAsciiAlphaNumeric(c)) {
    return true;
  }
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

static bool isTCHARorForwardSlash(int c) {
  return isTCHAR(c) || c == '/';
}

const char kStripParentheses[] = "()";
const char kStripParenthesesAndComma[] = "(),";

// Replace reserved characters with Unicode homoglyphs
std::string Sanitize(const std::string& str,
                     bool (*allowed)(int),
                     const char* strip = nullptr) {
  std::string clean;
  for (auto c : str) {
    if (allowed(c) && (!strip || !strchr(strip, c))) {
      clean.push_back(c);
    }
  }
  return clean;
}

std::optional<std::string> Sanitize(std::optional<std::string> str,
                                    bool (*allowed)(int),
                                    const char* strip = nullptr) {
  std::string clean;
  if (str) {
    clean = Sanitize(str.value(), allowed, strip);
  }
  if (clean.empty()) {
    return std::optional<std::string>();
  }
  return std::optional<std::string>(clean);
}
}  // namespace

#if BUILDFLAG(IS_ANDROID)
void UserAgentPlatformInfo::InitializePlatformDependentFieldsAndroid() {
  const std::string os_name = base::SysInfo::OperatingSystemName();
  const std::string os_version = base::SysInfo::OperatingSystemVersion();

#define STRINGIZE_NO_EXPANSION(x) #x
#define STRINGIZE(x) STRINGIZE_NO_EXPANSION(x)
  set_os_name_and_version(base::StringPrintf("Linux " STRINGIZE(ANDROID_ABI) "; %s %s", os_name.c_str(), os_version.c_str()));

  set_firmware_version(base::SysInfo::GetAndroidBuildID());

  // Rasterizer type is gles for both Linux and Android.
  set_rasterizer_type("gles");
}
#elif BUILDFLAG(IS_STARBOARD)
void UserAgentPlatformInfo::InitializePlatformDependentFieldsStarboard() {
  std::string os_name = base::SysInfo::OperatingSystemName();
  const std::string os_friendly_name =
      base::starboard::SbSysInfo::OSFriendlyName();
  if (!os_friendly_name.empty()) {
    os_name = os_friendly_name + "; " + os_name;
  }
  const std::string os_version = base::SysInfo::OperatingSystemVersion();
  set_os_name_and_version(
      base::StringPrintf("%s %s", os_name.c_str(), os_version.c_str()));
  set_firmware_version(
      starboard::GetSystemPropertyString(kSbSystemPropertyFirmwareVersion));
  // Rasterizer type is gles for both Linux and Android.
  set_rasterizer_type("gles");
}

#elif BUILDFLAG(IS_IOS_TVOS)
void UserAgentPlatformInfo::InitializePlatformDependentFieldsTvOS() {
  const std::string os_name = base::SysInfo::OperatingSystemName();
  const std::string os_version = base::SysInfo::OperatingSystemVersion();
  set_os_name_and_version(
      base::StringPrintf("%s %s", os_name.c_str(), os_version.c_str()));

  set_rasterizer_type("metal");
}

#endif  // BUILDFLAG(IS_ANDROID)

void UserAgentPlatformInfo::InitializeUserAgentPlatformInfoFields() {
// TODO(b/443337017): Fix InitializePlatformDependentFields...() for AOSP
// platforms, which are IS_ANDROID but also IS_STARBOARD.
#if BUILDFLAG(IS_ANDROID)
  InitializePlatformDependentFieldsAndroid();
#elif BUILDFLAG(IS_STARBOARD)
  InitializePlatformDependentFieldsStarboard();
#elif BUILDFLAG(IS_IOS_TVOS)
  InitializePlatformDependentFieldsTvOS();
#endif

#if defined(ENABLE_DEBUG_COMMAND_LINE_SWITCHES)
  // Because we add Cobalt's user agent string to Crashpad before we actually
  // start Cobalt, the command line won't be initialized when we first try to
  // get the user agent string.
  if (base::CommandLine::InitializedForCurrentProcess()) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    const char kUserAgentOsNameVersion[] = "user_agent_os_name_version";
    if (command_line->HasSwitch(kUserAgentOsNameVersion)) {
      set_os_name_and_version(
          command_line->GetSwitchValueASCII(kUserAgentOsNameVersion));
    }
  }
#endif  // ENABLE_DEBUG_COMMAND_LINE_SWITCHES

  set_model(base::SysInfo::HardwareModelName());

  set_original_design_manufacturer(
      base::starboard::SbSysInfo::OriginalDesignManufacturer());
  set_chipset_model_number(base::starboard::SbSysInfo::ChipsetModelNumber());
  set_model_year(base::starboard::SbSysInfo::ModelYear());
  set_brand(base::starboard::SbSysInfo::Brand());

  // Below UA info fields can NOT be retrieved directly from platform's native
  // system properties.

  // We only support JIT for both Linux and Android.
  set_javascript_engine_version(
      base::StringPrintf("v8/%s-jit", V8_VERSION_STRING));

#if BUILDFLAG(USE_EVERGREEN)
  updater::EvergreenLibraryMetadata evergreen_library_metadata =
      updater::GetCurrentEvergreenLibraryMetadata();
  set_evergreen_version(evergreen_library_metadata.version);
  set_evergreen_file_type(evergreen_library_metadata.file_type);
  if (!SbSystemGetExtension(kCobaltExtensionInstallationManagerName)) {
    // If the installation manager is not initialized, the "evergreen_lite"
    // command line parameter is specified and the system image is loaded.
    set_evergreen_type("Lite");
  } else {
    set_evergreen_type("Full");
  }
#endif
  if (!avoid_access_to_starboard_for_testing_) {
    set_device_type(
        starboard::GetSystemPropertyString(kSbSystemPropertyDeviceType));
    set_aux_field(
        starboard::GetSystemPropertyString(kSbSystemPropertyUserAgentAuxField));
    // Retrieve additional platform
    auto platform_info_extension =
        static_cast<const CobaltExtensionPlatformInfoApi*>(
            SbSystemGetExtension(kCobaltExtensionPlatformInfoName));
    if (platform_info_extension) {
      if (platform_info_extension->version >= 1) {
        char build_fingerprint[1024];
        if (platform_info_extension->GetFirmwareVersionDetails(
                build_fingerprint, 1024)) {
          set_android_build_fingerprint(build_fingerprint);
        }
        set_android_os_experience(platform_info_extension->GetOsExperience());
      }
      if (platform_info_extension->version >= 2) {
        int64_t ver = platform_info_extension->GetCoreServicesVersion();
        if (ver != 0) {
          std::string sver = std::to_string(ver);
          set_android_play_services_version(sver);
        }
      }
    }
  }

  set_starboard_version(base::StringPrintf("Starboard/%d", SB_API_VERSION));
  set_cobalt_version(COBALT_VERSION);
  set_cobalt_build_version_number(COBALT_BUILD_VERSION_NUMBER);

#if BUILDFLAG(COBALT_IS_RELEASE_BUILD)
  set_build_configuration("gold");
#elif defined(OFFICIAL_BUILD)
  set_build_configuration("qa");
#elif !defined(NDEBUG)
  set_build_configuration("debug");
#else
  set_build_configuration("devel");
#endif

// Apply overrides from command line
#if !BUILDFLAG(COBALT_IS_RELEASE_BUILD)
  if (!base::CommandLine::InitializedForCurrentProcess()) {
    return;
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  const char kUserAgentClientHints[] = "user_agent_client_hints";
  if (command_line->HasSwitch(kUserAgentClientHints)) {
    LOG(INFO) << "Entering UA overrides";
    const std::string user_agent_input =
        command_line->GetSwitchValueASCII(kUserAgentClientHints);

    std::map<std::string, std::string> user_agent_input_map;
    GetUserAgentInputMap(user_agent_input, user_agent_input_map);

    for (const auto& input : user_agent_input_map) {
      LOG(INFO) << "Overriding " << input.first << " to " << input.second;

      if (!input.first.compare("starboard_version")) {
        set_starboard_version(input.second);
        LOG(INFO) << "Setting starboard version to " << input.second;
      } else if (!input.first.compare("os_name_and_version")) {
        set_os_name_and_version(input.second);
        LOG(INFO) << "Setting os name and version to " << input.second;
      } else if (!input.first.compare("original_design_manufacturer")) {
        set_original_design_manufacturer(input.second);
        LOG(INFO) << "Setting original design manufacturer to " << input.second;
      } else if (!input.first.compare("device_type")) {
        set_device_type(input.second);
        LOG(INFO) << "Setting device type to " << input.second;
      } else if (!input.first.compare("chipset_model_number")) {
        set_chipset_model_number(input.second);
        LOG(INFO) << "Setting chipset model to " << input.second;
      } else if (!input.first.compare("model_year")) {
        set_model_year(input.second);
        LOG(INFO) << "Setting model year to " << input.second;
      } else if (!input.first.compare("firmware_version")) {
        set_firmware_version(input.second);
        LOG(INFO) << "Setting firmware version to " << input.second;
      } else if (!input.first.compare("brand")) {
        set_brand(input.second);
        LOG(INFO) << "Setting brand to " << input.second;
      } else if (!input.first.compare("model")) {
        set_model(input.second);
        LOG(INFO) << "Setting model to " << input.second;
      } else if (!input.first.compare("aux_field")) {
        set_aux_field(input.second);
        LOG(INFO) << "Setting aux field to " << input.second;
      } else if (!input.first.compare("javascript_engine_version")) {
        set_javascript_engine_version(input.second);
        LOG(INFO) << "Setting javascript engine version to " << input.second;
      } else if (!input.first.compare("rasterizer_type")) {
        set_rasterizer_type(input.second);
        LOG(INFO) << "Setting rasterizer type to " << input.second;
      } else if (!input.first.compare("evergreen_type")) {
        set_evergreen_type(input.second);
        LOG(INFO) << "Setting evergreen type to " << input.second;
      } else if (!input.first.compare("evergreen_file_type")) {
        set_evergreen_file_type(input.second);
        LOG(INFO) << "Setting evergreen file type to " << input.second;
      } else if (!input.first.compare("evergreen_version")) {
        set_evergreen_version(input.second);
        LOG(INFO) << "Setting evergreen version to " << input.second;
      } else if (!input.first.compare("android_build_fingerprint")) {
        set_android_build_fingerprint(input.second);
        LOG(INFO) << "Setting android build fingerprint to " << input.second;
      } else if (!input.first.compare("android_os_experience")) {
        set_android_os_experience(input.second);
        LOG(INFO) << "Setting android os experience to " << input.second;
      } else if (!input.first.compare("android_play_services_version")) {
        set_android_play_services_version(input.second);
        LOG(INFO) << "Setting android play services version to "
                  << input.second;
      } else if (!input.first.compare("cobalt_version")) {
        set_cobalt_version(input.second);
        LOG(INFO) << "Setting cobalt type to " << input.second;
      } else if (!input.first.compare("cobalt_build_version_number")) {
        set_cobalt_build_version_number(input.second);
        LOG(INFO) << "Setting cobalt build version to " << input.second;
      } else if (!input.first.compare("build_configuration")) {
        set_build_configuration(input.second);
        LOG(INFO) << "Setting build configuration to " << input.second;
      } else {
        LOG(WARNING) << "Unsupported user agent field: " << input.first;
      }
    }
  }
#endif
}

UserAgentPlatformInfo::UserAgentPlatformInfo(bool for_testing) {
  avoid_access_to_starboard_for_testing_ = for_testing;
  InitializeUserAgentPlatformInfoFields();
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
    std::optional<std::string> original_design_manufacturer) {
  if (original_design_manufacturer) {
    original_design_manufacturer_ =
        Sanitize(original_design_manufacturer, base::IsAsciiAlphaNumeric);
  }
}

void UserAgentPlatformInfo::set_device_type(const std::string& device_type) {
  device_type_string_ = device_type;
}

void UserAgentPlatformInfo::set_chipset_model_number(
    std::optional<std::string> chipset_model_number) {
  if (chipset_model_number) {
    chipset_model_number_ =
        Sanitize(chipset_model_number, base::IsAsciiAlphaNumeric);
  }
}

void UserAgentPlatformInfo::set_model_year(
    std::optional<std::string> model_year) {
  if (model_year) {
    model_year_ = Sanitize(model_year, base::IsAsciiDigit);
  }
}

void UserAgentPlatformInfo::set_firmware_version(
    std::optional<std::string> firmware_version) {
  if (firmware_version) {
    firmware_version_ = Sanitize(firmware_version, isTCHAR);
  }
}

void UserAgentPlatformInfo::set_brand(std::optional<std::string> brand) {
  if (brand) {
    brand_ = Sanitize(brand, isVCHARorSpace, kStripParenthesesAndComma);
  }
}

void UserAgentPlatformInfo::set_model(std::optional<std::string> model) {
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

void UserAgentPlatformInfo::set_android_build_fingerprint(
    const std::string& android_build_fingerprint) {
  android_build_fingerprint_ =
      Sanitize(android_build_fingerprint, isVCHARorSpace);
}

void UserAgentPlatformInfo::set_android_os_experience(
    const std::string& android_os_experience) {
  android_os_experience_ = Sanitize(android_os_experience, isTCHAR);
}

void UserAgentPlatformInfo::set_android_play_services_version(
    const std::string& android_play_services_version) {
  android_play_services_version_ =
      Sanitize(android_play_services_version, base::IsAsciiDigit);
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

// Official Youtube certification on user agent:
// https://developers.google.com/youtube/devices/living-room/2025/software-certification-2025#device-id-certification
std::string UserAgentPlatformInfo::ToString() const {
  // Cobalt's user agent contains the following sections:
  //   Mozilla/5.0 (ChromiumStylePlatform)
  //   Cobalt/Version.BuildNumber-BuildConfiguration (unlike Gecko)
  //   JavaScript Engine Name/Version
  //   Starboard/APIVersion,
  //   Device/FirmwareVersion (Brand, Model, ConnectionType)
  //
  // In the case of Evergreen, it contains three additional sections:
  //   Evergreen/Version
  //   Evergreen-Type
  //   Evergreen-FileType

  std::string user_agent =
      base::StringPrintf("Mozilla/5.0 (%s)", os_name_and_version_.c_str());

  base::StringAppendF(
      &user_agent, " Cobalt/%s.%s-%s (unlike Gecko)", cobalt_version_.c_str(),
      cobalt_build_version_number_.c_str(), build_configuration_.c_str());

  if (!javascript_engine_version_.empty()) {
    base::StringAppendF(&user_agent, " %s", javascript_engine_version_.c_str());
  }

  if (!rasterizer_type_.empty()) {
    base::StringAppendF(&user_agent, " %s", rasterizer_type_.c_str());
  }

  if (!evergreen_version_.empty()) {
    base::StringAppendF(&user_agent, " Evergreen/%s",
                        evergreen_version_.c_str());
  }

  if (!evergreen_type_.empty()) {
    base::StringAppendF(&user_agent, " Evergreen-%s", evergreen_type_.c_str());
  }

  if (!evergreen_file_type_.empty()) {
    base::StringAppendF(&user_agent, " Evergreen-%s",
                        evergreen_file_type_.c_str());
  }

  if (!starboard_version_.empty()) {
    base::StringAppendF(&user_agent, " %s", starboard_version_.c_str());
  }

  const std::string kUnknownFieldName = "Unknown";

  base::StringAppendF(
      &user_agent, ", %s_%s_%s_%s/%s (%s, %s)",
      original_design_manufacturer_.value_or(kUnknownFieldName).c_str(),
      device_type_string_.c_str(),
      chipset_model_number_.value_or(kUnknownFieldName).c_str(),
      model_year_.value_or("0").c_str(),
      firmware_version_.value_or(kUnknownFieldName).c_str(),
      brand_.value_or(kUnknownFieldName).c_str(),
      model_.value_or(kUnknownFieldName).c_str());

  if (!aux_field_.empty()) {
    base::StringAppendF(&user_agent, " %s", aux_field_.c_str());
  }
  return user_agent;
}

}  // namespace cobalt
