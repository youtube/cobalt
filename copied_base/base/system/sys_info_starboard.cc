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

#include "base/system/sys_info_starboard.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include <sys/system_properties.h>
#elif BUILDFLAG(IS_STARBOARD)
#include "base/system/sys_info.h"
#include "starboard/common/system_property.h"
using starboard::GetSystemPropertyString;
#elif BUILDFLAG(IS_IOS_TVOS)
#include <map>

#include <sys/utsname.h>

#include "base/containers/contains.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_STARBOARD)
namespace base {
std::string SysInfo::HardwareModelName() {
  return GetSystemPropertyString(kSbSystemPropertyModelName);
}
}
#endif // BUILDFLAG(IS_STARBOARD)
namespace base {
namespace starboard {

#if BUILDFLAG(IS_ANDROID)
std::string SbSysInfo::OriginalDesignManufacturer() {
  char original_design_manufacturer_str[PROP_VALUE_MAX];
  __system_property_get("ro.product.manufacturer", original_design_manufacturer_str);
  return std::string(original_design_manufacturer_str);
}

std::string SbSysInfo::ChipsetModelNumber() {
  char chipset_model_number_str[PROP_VALUE_MAX];
  __system_property_get("ro.board.platform", chipset_model_number_str);
  return std::string(chipset_model_number_str);
}

std::string SbSysInfo::ModelYear() {
  const std::string kUnknownValue = "unknown";

  char model_year_cstr[PROP_VALUE_MAX];
  __system_property_get("ro.oem.key1", model_year_cstr);
  std::string model_year_str(model_year_cstr);

  if (model_year_str == kUnknownValue || model_year_str.length() < 10) {
    return model_year_str;
  }

  // See
  // https://support.google.com/androidpartners_androidtv/answer/9351639?hl=en
  // for the format of |model_year_str|.
  std::string year_str = "20";
  year_str += model_year_cstr[9];
  year_str += model_year_cstr[10];
  return year_str;
}

std::string SbSysInfo::Brand() {
  char brand_str[PROP_VALUE_MAX];
  __system_property_get("ro.product.brand", brand_str);
  return std::string(brand_str);
}
#elif BUILDFLAG(IS_STARBOARD)
std::string SbSysInfo::OriginalDesignManufacturer() {
  return GetSystemPropertyString(kSbSystemPropertySystemIntegratorName);
}

std::string SbSysInfo::ChipsetModelNumber() {
  return GetSystemPropertyString(kSbSystemPropertyChipsetModelNumber);
}

std::string SbSysInfo::ModelYear() {
  return GetSystemPropertyString(kSbSystemPropertyModelYear);
}

std::string SbSysInfo::Brand() {
  return GetSystemPropertyString(kSbSystemPropertyBrandName);
}

#elif BUILDFLAG(IS_IOS_TVOS)
std::string SbSysInfo::OriginalDesignManufacturer() {
  // Cobalt 25: https://github.com/youtube/cobalt/blob/62c2380b7eb0da5889a387c4b9be283656a8575d/starboard/shared/uikit/system_get_property.mm#L126
  return "YouTube";
}

std::string SbSysInfo::ChipsetModelNumber() {
  struct utsname systemInfo;
  uname(&systemInfo);
  // Extracted from
  // https://github.com/youtube/cobalt/blob/62c2380b7eb0da5889a387c4b9be283656a8575d/starboard/shared/uikit/system_get_property.mm#L27-L44
  const std::map<std::string_view, std::string_view> kChipsets{
      {"AppleTV1,1", "Intel Pentium M"},
      {"AppleTV2,1", "Apple A4"},
      {"AppleTV3,1", "Apple A5"},
      {"AppleTV3,2", "Apple A5"},
      {"AppleTV5,3", "Apple A8"},
      {"AppleTV6,2", "Apple A10X Fusion"},
      {"AppleTV11,1", "Apple A12 Bionic"},
      {"AppleTV14,1", "Apple A15 Bionic"},
  };
  if (const auto it = kChipsets.find(systemInfo.machine);
      it != kChipsets.end()) {
    return std::string(it->second);
  }
  // https://github.com/youtube/cobalt/blob/62c2380b7eb0da5889a387c4b9be283656a8575d/starboard/shared/uikit/system_get_property.mm#L49
  static constexpr std::string_view kUnknownChipset = "ChipsetUnknown";
  return std::string(kUnknownChipset);
}

std::string SbSysInfo::ModelYear() {
  struct utsname systemInfo;
  uname(&systemInfo);
  // Extracted from
  // https://github.com/youtube/cobalt/blob/62c2380b7eb0da5889a387c4b9be283656a8575d/starboard/shared/uikit/system_get_property.mm#L27-L44
  const std::map<std::string_view, std::string_view> kYears{
      {"AppleTV1,1", "2007"},
      {"AppleTV2,1", "2010"},
      {"AppleTV3,1", "2012"},
      {"AppleTV3,2", "2013"},
      {"AppleTV5,3", "2015"},
      {"AppleTV6,2", "2017"},
      {"AppleTV11,1", "2021"},
      {"AppleTV14,1", "2022"},
  };
  if (const auto it = kYears.find(systemInfo.machine); it != kYears.end()) {
    return std::string(it->second);
  }
  // https://github.com/youtube/cobalt/blob/62c2380b7eb0da5889a387c4b9be283656a8575d/starboard/shared/uikit/system_get_property.mm#L48
  static constexpr std::string_view kUnknownYear = "2022";
  return std::string(kUnknownYear);
}

std::string SbSysInfo::Brand() {
  return "Apple";
}

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace starboard
}  // namespace base
